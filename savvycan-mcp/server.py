#!/usr/bin/env python3
r"""SavvyCAN MCP server — lets an agent drive the SavvyCAN CLI over MCP.

SCOPE CONTRACT (read before adding anything):
  This is a THIN wrapper over `SavvyCAN.exe --cli`. Every tool is one CLI
  invocation: build args, run, shape the result. That is the whole job.
  - No CAN/vehicle knowledge lives here (no per-car IDs, no mode tables). That
    belongs in DBCs and docs, not in this server.
  - A genuinely new capability is a new small tool that still maps to one CLI
    run, not extra branches inside an existing one.
  - If this file passes ~250 lines or a tool grows business logic, stop and
    split it. The CLI is the product; this is glue.

Tools: list_serial_ports, capture, send_frame, connection_help, analyze_capture.
Run:   python server.py         (stdio MCP server)
Builds: the repo has two Windows builds, bin\x86\SavvyCAN.exe and bin\x64\SavvyCAN.exe
  (built by build-win.cmd). A vendor driver DLL can only be loaded by a process of the
  same bitness, so the `arch` argument on capture/send_frame/connection_help picks the
  build: x86 (default) for J2534 passthru adapters with 32-bit DLLs (XHorse / clone
  MVCI) and for everything that has no vendor DLL (lawicel/slcan, gvret, socketcand,
  mqtt, canserver); x64 only for adapters whose vendor DLL is 64-bit.
Env:   SAVVYCAN_EXE overrides the exe path for every arch (testing only).
"""
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Literal

from mcp.server.mcpserver import MCPServer

# The CLI's connection types, as an enum so the valid set is in each tool's schema
# (not just prose). This is the one place the set is named; the source of truth for
# full option detail is the connection_help tool, which returns the binary's own --help.
Adapter = Literal["lawicel", "serialbus", "gvret", "kayak", "mqtt", "canserver"]
# Which build to run; see the module docstring for when each applies.
Arch = Literal["x86", "x64"]

REPO = Path(__file__).resolve().parent.parent
TOGGLE_FINDER = REPO / "workspace" / "toggle_finder.py"


def _exe(arch: Arch) -> Path:
    """Path of the SavvyCAN build for `arch` (bin\\x86 or bin\\x64, from build-win.cmd)."""
    override = os.environ.get("SAVVYCAN_EXE")
    return Path(override) if override else REPO / "bin" / arch / "SavvyCAN.exe"

mcp = MCPServer("savvycan")

# candump line printed by the CLI: "(0000012.345678) can0  0x1A6  [8] 00 11 ..  [TX]"
_FRAME = re.compile(r"^\((\d+\.\d+)\)\s+can(\d+)\s+(\S+)\s+\[(\d+)\]\s*([0-9A-Fa-f ]*?)\s*(TX)?\s*$")


def _run(args, timeout, want_frames, arch: Arch = "x86"):
    """Run the CLI once. Returns (frames, raw_nonframe_lines, rc). On timeout,
    returns whatever was printed so far (capture stops are normal, not errors)."""
    exe = _exe(arch)
    if not exe.exists():
        raise FileNotFoundError(
            f"SavvyCAN.exe not found at {exe}; run build-win.cmd {arch} (or set SAVVYCAN_EXE)")
    proc = subprocess.Popen([str(exe), "--cli", *args],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        out, _ = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
    frames, other = [], []
    for line in out.splitlines():
        m = _FRAME.match(line) if want_frames else None
        if m:
            data = m.group(5).split()
            frames.append({
                "t": float(m.group(1)), "bus": int(m.group(2)), "id": m.group(3),
                "len": int(m.group(4)), "data": " ".join(data), "dir": "TX" if m.group(6) else "RX",
            })
        elif line.strip() and not line.startswith("[debug]"):
            other.append(line.rstrip())
    return frames, other, proc.returncode


@mcp.tool()
def list_serial_ports() -> dict:
    """List serial ports on this machine (to find a CANable/slcan adapter, e.g. COM8)."""
    ps = subprocess.run(
        ["powershell", "-NoProfile", "-Command",
         "[System.IO.Ports.SerialPort]::GetPortNames() -join ','"],
        capture_output=True, text=True, timeout=15)
    ports = [p for p in ps.stdout.strip().split(",") if p]
    return {"ports": ports}


@mcp.tool()
def capture(port: str, speed: int = 500000, adapter: Adapter = "lawicel",
            driver: str = "", serial_speed: int = 115200,
            count: int = 200, timeout_s: float = 20, listen_only: bool = True,
            output_csv: str = "", arch: Arch = "x86") -> dict:
    """Capture CAN frames from an adapter and return them.

    port: e.g. COM8, or can0 for socketcan. speed: bus bitrate, default 500000.
    adapter: lawicel (CANable/slcan over a COM port, default), gvret, kayak, mqtt,
    canserver, or serialbus. driver: required for serialbus — the Qt plugin, e.g.
    socketcan, peakcan, passthrucan, virtualcan. serial_speed: LAWICEL serial baud
    (default 115200). count: stop after this many frames. timeout_s: give up waiting
    after this long, returning whatever arrived. listen_only: never transmit (default
    true). output_csv: also save a SavvyCAN native CSV here (for analyze_capture).
    arch: which build runs — x86 (default) works for every adapter without a vendor
    DLL and for J2534/passthrucan devices with 32-bit DLLs (XHorse, clone MVCI);
    use x64 only when the adapter's vendor DLL is 64-bit (the DLL and the process
    must match). Call connection_help for the binary's full option list.
    """
    args = ["--type", adapter, "--port", port, "--speed", str(speed), "--count", str(count)]
    if driver:
        args += ["--driver", driver]
    if adapter == "lawicel":
        args += ["--serial-speed", str(serial_speed)]
    if listen_only:
        args.append("--listen-only")
    if output_csv:
        args += ["--output", output_csv, "--format", "csv"]
    frames, msgs, rc = _run(args, timeout_s, want_frames=True, arch=arch)
    return {"count": len(frames), "frames": frames[:count], "messages": msgs,
            "saved": output_csv or None, "arch": arch}


@mcp.tool()
def send_frame(port: str, frame: str, speed: int = 500000,
               adapter: Adapter = "lawicel", driver: str = "", arch: Arch = "x86") -> dict:
    """Transmit one CAN frame. frame is 'ID#HEXDATA', e.g. '59D#25440000'.
    adapter/driver/arch as in capture. This writes to a live bus — the caller owns that."""
    args = ["--type", adapter, "--port", port, "--speed", str(speed), "--send", frame]
    if driver:
        args += ["--driver", driver]
    _, msgs, rc = _run(args, timeout=10, want_frames=False, arch=arch)
    return {"ok": rc == 0, "messages": msgs, "arch": arch}


@mcp.tool()
def connection_help(arch: Arch = "x86") -> dict:
    """Return the SavvyCAN CLI's own --help: the authoritative, always-current list of
    connection types, serialbus drivers, and every option, straight from the binary.
    Also reports which builds exist (bin\\x86, bin\\x64) and when to use each."""
    _, lines, _ = _run(["--help"], timeout=10, want_frames=False, arch=arch)
    return {
        "help": "\n".join(lines),
        "builds": {a: str(_exe(a)) for a in ("x86", "x64") if _exe(a).exists()},
        "arch_rule": "x86 for J2534/passthrucan adapters with 32-bit vendor DLLs (XHorse, "
                     "clone MVCI) and for all adapters without a vendor DLL (lawicel, gvret, "
                     "socketcand, mqtt, canserver); x64 only for 64-bit vendor DLLs.",
    }


@mcp.tool()
def analyze_capture(csv_path: str, presses: int = 0, inspect_id: str = "") -> dict:
    """Rank which bits change like a toggled switch in a captured CSV (via
    workspace/toggle_finder.py). presses: how many times the control was toggled.
    inspect_id: a hex CAN ID to dump the full byte history of instead."""
    args = [sys.executable, "-I", str(TOGGLE_FINDER), csv_path]
    if inspect_id:
        args += ["--id", inspect_id]
    elif presses:
        args += ["--presses", str(presses)]
    r = subprocess.run(args, capture_output=True, text=True, timeout=120)
    return {"output": r.stdout, "error": r.stderr or None}


if __name__ == "__main__":
    mcp.run(transport="stdio")
