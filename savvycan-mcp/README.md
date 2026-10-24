# savvycan-mcp

An MCP server that lets an agent drive the SavvyCAN CLI: list serial ports, capture
CAN frames, send a frame, and rank which bits change like a toggled switch. It is a thin
wrapper over `bin\<arch>\SavvyCAN.exe --cli` — see the scope contract at the top of
`server.py` before adding anything.

### Which build it runs

`build-win.cmd` produces two builds, `bin\x86\SavvyCAN.exe` and `bin\x64\SavvyCAN.exe`.
`capture`, `send_frame` and `connection_help` take an `arch` argument (`x86` default, or
`x64`). The rule: a vendor driver DLL can only be loaded by a process of the same bitness.

| Adapter | `arch` |
|---|---|
| J2534 / `passthrucan` with a 32-bit vendor DLL (XHorse, clone MVCI) | `x86` |
| lawicel / slcan (CANable), gvret, socketcand, mqtt, canserver (no vendor DLL) | `x86` (either works) |
| `serialbus` driver whose vendor DLL is 64-bit (e.g. 64-bit PCANBasic) | `x64` |

`connection_help` also returns which builds exist and this rule, so an agent can check
before capturing.

## Tools

| Tool | What it does |
|------|--------------|
| `list_serial_ports` | List COM ports, to find a CANable/slcan adapter (e.g. COM8). |
| `capture` | Capture frames (`port`, `speed`, `adapter`, `driver`, `serial_speed`, `count`, `timeout_s`, `listen_only`, `output_csv`) and return them. Listen-only by default. |
| `send_frame` | Transmit one frame `ID#HEXDATA` (`adapter`/`driver` too). Writes to a live bus. |
| `connection_help` | Return the CLI's own `--help`: the authoritative connection types, drivers and options. |
| `analyze_capture` | Run `workspace/toggle_finder.py` on a captured CSV: `presses` to rank toggle bits, or `inspect_id` to dump one ID's byte history. |

### Connection types and settings

The agent sees the valid adapters as a schema enum on `capture`/`send_frame`
(`lawicel`, `serialbus`, `gvret`, `kayak`, `mqtt`, `canserver`), so invalid values are
rejected before a run. `serialbus` also needs `driver` (the Qt plugin, e.g. `socketcan`,
`peakcan`, `passthrucan`, `virtualcan`); `lawicel` takes `serial_speed` (default 115200).
For the full, always-current option list, the agent calls `connection_help`, which returns
the binary's own `--help` — the single source of truth, so it never drifts from the CLI.

## Requirements

- The SavvyCAN builds at `bin\x86\SavvyCAN.exe` and/or `bin\x64\SavvyCAN.exe`
  (`build-win.cmd`; `SAVVYCAN_EXE` overrides the path for testing).
- Python with the MCP SDK: `pip install mcp`.

## Register

This repo ships a project-scoped `.mcp.json`, so Claude Code picks the server up when run
from `C:\projects\SavvyCAN`. To add it at user scope from anywhere:

```sh
claude mcp add savvycan -- C:\Python314\python.exe C:\projects\SavvyCAN\savvycan-mcp\server.py
```

Run it standalone (stdio) for testing: `python savvycan-mcp\server.py`.

## Example flow

1. `list_serial_ports` → find the CANable, e.g. COM8.
2. `capture` with `port=COM8, count=400, output_csv=cap.csv` while toggling a switch.
3. `analyze_capture` with `csv_path=cap.csv, presses=3` → ranked candidate bits.

Captures and the analyser live in `../workspace`; the method is the `can-signal-finder` skill.
