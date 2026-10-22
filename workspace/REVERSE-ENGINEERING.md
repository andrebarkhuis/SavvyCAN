# Reverse engineering the IS F drive-mode switches (Sport / Snow / Traction)

How the Sport, Snow and Traction-control CAN signals on a Lexus IS F (2UR-GSE) were
found, for an IS F-drivetrain-into-GT86 swap whose CAN gateway must synthesise the
frames a missing body/cluster ECU used to send. Done 2026-10-21/22. Results are in
`isf-drivemode-can-spec.md` and `isf-sport.dbc`; the capture logs and tools are beside
this file.

## The goal

The sports car runs the IS F powertrain (ECU, TCM, ABS) but has no IS F body or cluster
ECU. The physical Sport/Snow/Traction buttons normally reach those modules over CAN. With
the sending module gone, the drivetrain never hears the mode change. We needed the exact
CAN frames so an ESP32 gateway can generate them.

## Tools

Capture is now done with the **SavvyCAN CLI** (`build-win32\deploy\SavvyCAN.exe --cli`,
headless), which writes the native CSV the analyser reads, e.g.:

```sh
SavvyCAN.exe --cli --port COM8 --speed 500000 --listen-only --count 400 --output cap.csv --format csv
```

- `toggle_finder.py` (in this folder) — the core analysis: ranks every bit of every ID by
  how rarely it changes and, optionally, how well its changes line up with button presses.

The original captures for this write-up were taken with a small purpose-built 32-bit J2534
logger and supervisor (it drove the XHorse MVCI directly, chunked around an ~88 s DLL crash,
and filtered by ID range to fit the adapter's ~31 frame/s limit). Those have been removed in
favour of the CLI, which reaches the same MVCI via `--type serialbus --driver passthrucan`
and any other adapter besides; the raw capture CSVs they produced are kept here as evidence.

## Method: find the bit that changes only when you press

A switch changes exactly as often as it is pressed; counters, checksums and sensors change
hundreds of times a minute. So the finder scores each bit by transition count (closest to
the number of presses wins) and discards anything that changes too often. The press count
is only a ranking hint, not a timing assumption, which is why imprecise press timing did
not matter.

Capture procedure that worked:
1. Ignition on, engine off (quieter bus; the Sport lamp confirmed the switch was live).
2. Start a filtered capture, then toggle the switch a few times with the state held long
   enough (~15 s) that even 1 s-period frames are sampled in each state.
3. Run `toggle_finder.py --presses N`; read the top-ranked ID/byte/bit; dump its history
   with `--id XXX` to see whether it is a single bit, a multi-value field, or a momentary
   pulse, and whether a second frame follows a moment later (a consumer echo).

### Hard-won adapter facts

- The XHorse MVCI (a clone J2534) returns **no frames** if the channel is opened with
  CAN_ID_BOTH; it must be opened 11-bit only. (This is also why SavvyCAN's stock passthru
  plugin saw nothing — see `../win32-passthrucan/`.)
- It delivers only ~31 frames/s regardless of the FTDI latency timer, and accepts a single
  filter interpreted as a contiguous ID range. Fast frames therefore had to be captured a
  range at a time.
- Its transmit is unreliable (fails after ~2 frames), so final command testing belongs on
  a CANable or the gateway, not this adapter.
- Only one process can hold the adapter; Techstream, SavvyCAN and the logger conflict.

### A mistake worth remembering

Early on, several range captures were reported "clear" when in fact no button had been
pressed during them. A capture with no change is indistinguishable from a capture with no
press. The rule adopted: a range is only clear if the capture itself shows the presses
(via a known press-marker frame) or the user confirms presses for that specific run.

## What was found

| Signal | Frame | Detail |
|--------|-------|--------|
| Sport state | 0x59D byte1 bit6 | 25 04 00 00 normal, 25 44 00 00 Sport |
| Snow state  | 0x59D byte1 bit5 | 25 24 00 00 Snow (same frame as Sport) |
| Mode press  | 0x59D byte0 bit7 | one-frame pulse on each press |
| Switch reader | 0x583 | ~0.1 s before 0x59D; byte2 = 20 Sport / 10 Snow during a press |
| Snow echo | 0x5D2 byte1 bit4 | a consumer ECU following Snow only |
| Traction OFF | 0x591 byte5 bit5 | reported by the ABS (18 vs 38) |

Who sends what was settled by diffing the full car against the bare drivetrain: on the
bare sports car (ECU+TCM+ABS only) 0x59D and 0x583 are **absent** but 0x591 is **present**.
So Sport/Snow came from the missing body/cluster ECU (the gateway must generate 0x59D),
while traction is owned by the ABS.

### Confirmation by injection

On a complete donor car, injecting one 0x583 press edge (`E6 E0 20 12`) on top of the real
0x583 heartbeat made 0x59D latch Sport and hold; a second press cleared it. This proved the
mapping end to end. Whether the headless drivetrain adopts Sport from the 0x59D status frame
alone is the one item still to confirm on the car with the gateway/CANable (the J2534
adapter could not transmit reliably enough to finish it).

### Ground truth caveat

Techstream (reading the ABS directly) showed traction OFF while 0x591 byte5 read the
"on" value from the donor. On an incomplete car the ABS disables the traction *function*
because of missing inputs, independent of the switch *request* bit. Techstream is
authoritative; the donor-derived bit label does not fully map to the bare car. The CAN
command (if any) that toggles traction was never found and may be hardwired to the ABS.

## Reproducing or extending

Watch a switch live:

```sh
SavvyCAN.exe --cli --port COM8 --speed 500000 --listen-only
```

Or hunt a new switch the same way the others were found: capture to CSV while toggling,
then rank and inspect.

```sh
SavvyCAN.exe --cli --port COM8 --speed 500000 --listen-only --count 400 --output out.csv --format csv
python -I workspace/toggle_finder.py out.csv --presses 3
python -I workspace/toggle_finder.py out.csv --id 59D     # inspect a candidate
```

See `../.claude/skills/can-signal-finder/` for the workflow as a reusable skill, and
`isf-drivemode-can-spec.md` for the gateway-ready message list.
