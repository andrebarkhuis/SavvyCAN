---
name: savvy-log-session
description: Log a complete CAN session on the car with the XHorse / clone MVCI J2534 adapter - especially a cold start, where logging must already be running before the ignition goes on and nothing may be lost when the adapter DLL crashes. Use when the user wants to record a drive, a cold start, a warm-up, or any long capture with SavvyCAN, asks to "log a session", "run a capture from start", or needs a continuous log despite the XHorse crash.
---

# Logging a session (cold start) with the XHorse

The adapter's driver (`MVCI32.dll`) crashes with an access violation after roughly 60-90 s
of an open channel (see `/savvy-debug-crash`). A single long capture therefore cannot work. The
repo handles this with chunked logging: `log-session.cmd` runs the 32-bit CLI for 45 s at
a time, each chunk written to its own CSV *as frames arrive* (SavvyCAN's continuous native
log, so a crash mid-chunk keeps everything received), and restarts at once when a chunk
ends or the process dies. All chunks carry host time in microseconds since the epoch, so
they line up across restarts.

Nothing here reimplements logging: the CLI uses `FrameFileIO`'s continuous native log,
and the script only restarts the CLI.

## Before the car

1. **Build present:** `bin\x86\SavvyCAN.exe` must exist (the XHorse DLL is 32-bit). If not,
   `cmd /c C:\projects\SavvyCAN\build-win.cmd x86`.
2. **Adapter visible:** `& "C:\Qt\5.15.2\msvc2019\bin\canbusutil.exe" -d passthrucan` must
   list `XHorse - MVCI`. If not, check USB and that no other program (Techstream, a
   previous SavvyCAN) holds the adapter: `Get-Process SavvyCAN` and close it.
3. **Smoke test, ignition on, engine off:**
   `bin\x86\SavvyCAN.exe --cli --type serialbus --driver passthrucan --port "XHorse - MVCI" --speed 500000 --listen-only --count 20`
   Twenty frames in a few seconds means the chain works. Do this once, not right before the
   cold start (a connect counts as "running" for the DLL and wastes nothing, but keep it short).

## The cold start itself

Order matters: the log must be running before anything is switched on.

1. Everything off. Adapter plugged into the OBD port and USB.
2. Start the logger with a descriptive session name:
   ```
   cmd /c C:\projects\SavvyCAN\log-session.cmd coldstart-2026-10-25
   ```
   It prints one line per chunk. The first chunk may contain few or no frames while the bus
   sleeps; that is fine, the file still has its header.
3. Wait for the first `chunk 001 start` line, then: ignition on, start the engine, do the
   drive or idle warm-up. Note the wall-clock time of each event (ignition, crank, first
   movement); timestamps in the log are host time so they can be matched later.
4. Chunks shorter than 45 s or lines saying `ended with code ... restarting` are the DLL
   crash; the restart takes about two seconds and that is the only gap. Do not stop the
   script when you see them.
5. When done, Ctrl+C in that window (answer Y to "Terminate batch job").

Optional arguments: `log-session.cmd <name> <chunk-seconds> <device> <speed>`. Keep chunks
at or below 45 s on the XHorse; other J2534 devices (`TIS Techstream VIM`, `WiFlash J2534`)
may not need chunking at all.

## After

- Merge: `cmd /c C:\projects\SavvyCAN\log-session.cmd merge coldstart-2026-10-25` writes
  `workspace\logs\coldstart-2026-10-25\coldstart-2026-10-25.csv` (one header, all frames,
  chronological because chunk files are numbered).
- Open the merged CSV in the GUI (`bin\x86\SavvyCAN.exe`, Load) or analyse with
  `python -I workspace\toggle_finder.py <merged.csv> ...` (see `/savvy-can-signal-finder`).
- `workspace\logs\` is gitignored. Copy a capture worth keeping into `workspace\` with a
  descriptive name and commit it as evidence, as the existing captures are.
- `workspace\logs\<name>\session.log` records every chunk start, finish or crash with
  timestamps; use it to see how many restarts happened and when.

## Timestamps

`Time Stamp` in the CSV is microseconds since the Unix epoch (host clock), because the
XHorse reports no usable timestamp of its own (the raw value wraps to about 1.8e13 s).
Convert in PowerShell: `[DateTimeOffset]::FromUnixTimeMilliseconds(1792345678901234/1000).LocalDateTime`.
The CLI uses host time by default; `--device-time` keeps the adapter's values for adapters
that do timestamp correctly.

## If frames stop arriving but nothing crashed

Only one process can hold the adapter. Check `Get-Process SavvyCAN` for a leftover, kill it,
and let the script continue (it restarts the CLI on its own). If the car's bus has gone to
sleep (ignition off for a while), the empty chunks are correct behaviour.
