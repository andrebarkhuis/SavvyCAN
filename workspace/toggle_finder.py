#!/usr/bin/env python3
"""Find CAN bits/bytes that flip in step with a switch you toggled during a capture.

Input: a SavvyCAN native CSV log (File > Save Log File, "CSV" format):
    Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8

Usage:
    python toggle_finder.py capture.csv                 # rank bits with few transitions
    python toggle_finder.py capture.csv --presses 6     # you pressed the button 6 times
    python toggle_finder.py capture.csv --toggles 12,31,52,70   # seconds (from log start) of each press
    python toggle_finder.py capture.csv --id 3B3        # dump the change history of one ID

Idea: a toggle pressed N times produces about N transitions in the bit that carries it,
while counters, checksums and sensor bytes change hundreds of times. We score every bit
of every ID by how close its transition count is to N and, if press times are given,
by how well its transitions line up with them.
"""
import argparse
import csv
import sys
from collections import defaultdict


def load(path):
    frames = defaultdict(list)  # id -> [(t_seconds, bytes)]
    t0 = None
    with open(path, newline="", encoding="utf-8", errors="replace") as f:
        reader = csv.reader(f)
        header = next(reader, None)
        if not header or not header[0].startswith("Time Stamp"):
            sys.exit("not a SavvyCAN native CSV log (missing 'Time Stamp' header)")
        for row in reader:
            if len(row) < 7:
                continue
            try:
                ts = int(row[0])
                fid = int(row[1], 16)
                n = int(row[5])
            except ValueError:
                continue
            if row[3].strip() != "Rx":
                continue
            data = bytes(int(x, 16) for x in row[6:6 + n] if x)
            if t0 is None:
                t0 = ts
            frames[fid].append(((ts - t0) / 1e6, data))
    return frames


def bit_series(samples, byte_i, bit_i):
    """Return list of (time, value) only where the bit changed."""
    out = []
    last = None
    for t, d in samples:
        if byte_i >= len(d):
            continue
        v = (d[byte_i] >> bit_i) & 1
        if v != last:
            out.append((t, v))
            last = v
    return out


def byte_series(samples, byte_i):
    out = []
    last = None
    for t, d in samples:
        if byte_i >= len(d):
            continue
        v = d[byte_i]
        if v != last:
            out.append((t, v))
            last = v
    return out


def alignment(transitions, toggles, tol=1.5):
    """Fraction of press times that have a transition within tol seconds."""
    if not toggles:
        return 0.0
    hits = sum(1 for tt in toggles if any(abs(tr - tt) <= tol for tr, _ in transitions))
    return hits / len(toggles)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log")
    ap.add_argument("--presses", type=int, default=None, help="how many times the switch was pressed")
    ap.add_argument("--toggles", default=None, help="comma separated press times in seconds from log start")
    ap.add_argument("--id", default=None, help="hex CAN ID: dump every byte change of this ID and exit")
    ap.add_argument("--exclude", default=None, help="comma separated hex IDs to skip, e.g. 0B4,025,2C1")
    ap.add_argument("--max-transitions", type=int, default=60, help="ignore bits that change more often than this")
    ap.add_argument("--top", type=int, default=25)
    args = ap.parse_args()

    frames = load(args.log)
    if not frames:
        sys.exit("no received frames in log")
    duration = max(s[-1][0] for s in frames.values())
    print(f"{sum(len(s) for s in frames.values())} frames, {len(frames)} IDs, {duration:.1f} s")

    if args.id:
        fid = int(args.id, 16)
        samples = frames.get(fid)
        if not samples:
            sys.exit(f"ID {args.id} not in log")
        print(f"ID {fid:03X}: {len(samples)} frames, period ~{duration / max(len(samples), 1) * 1000:.0f} ms")
        for byte_i in range(max(len(d) for _, d in samples)):
            changes = byte_series(samples, byte_i)
            print(f"  byte {byte_i}: {len(changes) - 1} changes " +
                  (", ".join(f"{t:.1f}s->{v:02X}" for t, v in changes[:40]) if len(changes) <= 41 else "(busy)"))
        return

    toggles = [float(x) for x in args.toggles.split(",")] if args.toggles else []
    expected = args.presses if args.presses else (len(toggles) if toggles else None)

    excluded = {int(x, 16) for x in args.exclude.split(",")} if args.exclude else set()
    results = []
    for fid, samples in frames.items():
        if len(samples) < 3 or fid in excluded:
            continue
        width = max(len(d) for _, d in samples)
        for byte_i in range(width):
            for bit_i in range(8):
                tr = bit_series(samples, byte_i, bit_i)
                n_tr = len(tr) - 1  # first entry is the initial value
                if n_tr < 1 or n_tr > args.max_transitions:
                    continue
                score = 0.0
                if expected:
                    score -= abs(n_tr - expected)
                if toggles:
                    score += 10 * alignment(tr[1:], toggles)
                results.append((score, fid, byte_i, bit_i, n_tr, tr))

    if not results:
        print("no slowly changing bits found; is the capture long enough and did the switch state change?")
        return

    # Secondary ordering: fewer transitions first (more switch-like), then ID.
    results.sort(key=lambda r: (-r[0], r[4], r[1], r[2], r[3]))
    print()
    print(f"{'ID':>4} {'byte':>4} {'bit':>3} {'flips':>5}  first transitions (s -> value)")
    for score, fid, byte_i, bit_i, n_tr, tr in results[:args.top]:
        times = ", ".join(f"{t:.1f}->{v}" for t, v in tr[1:9])
        more = " ..." if n_tr > 8 else ""
        print(f"{fid:>4X} {byte_i:>4} {bit_i:>3} {n_tr:>5}  {times}{more}")

    # Group hint: several bits of one byte flipping together usually means a multi-value field.
    by_id = defaultdict(set)
    for score, fid, byte_i, bit_i, n_tr, tr in results[:args.top]:
        by_id[fid].add(byte_i)
    print()
    print("candidate IDs (use --id XXX to see the full byte history):",
          ", ".join(f"{fid:03X}[bytes {sorted(b)}]" for fid, b in sorted(by_id.items())))


if __name__ == "__main__":
    main()
