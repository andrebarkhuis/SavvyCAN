---
name: can-signal-finder
description: Capture CAN bus traffic and find which frame, byte and bit carries a signal the user can change — a switch, mode, button, door, light, pedal or any state they can toggle or vary. Use this whenever the user wants to sniff, log, reverse engineer or "find the packet/ID for" something on a vehicle or any CAN bus, asks which message a feature lives in, or wants a DBC signal for a control, on any car and through any adapter the SavvyCAN CLI supports (CANable/slcan, GVRET, SocketCAN, or a J2534 passthru device via the serialbus plugin). Also use it when a CAN tool shows no frames from an adapter.
---

# Finding which CAN message carries a signal

Goal: identify the CAN frame that carries a state the user can change, by logging the bus
while they toggle it and ranking bits by how rarely they change. Counters, checksums and
sensors change hundreds of times a minute; a switch changes exactly as often as the user
presses it. That difference is the whole method, and it does not depend on precise timing.

## Tools

Capture with the **SavvyCAN CLI** (headless, in `build-win32\deploy\SavvyCAN.exe`); analyse
with `workspace\toggle_finder.py`.

- Capture to the native CSV that the analyser reads:
  `SavvyCAN.exe --cli --port COM8 --speed 500000 --listen-only --count 400 --output cap.csv --format csv`
  The file is written when capture stops (on `--count` or Ctrl+C). Use `--listen-only` so the
  tool never transmits onto a live vehicle bus.
- Adapter selection: `--type lawicel` (default) for a CANable/slcan on a COM port;
  `--type gvret`, `--type socketcan`, or `--type serialbus --driver <plugin>` otherwise.
  Find the CANable's COM port with `[System.IO.Ports.SerialPort]::GetPortNames()` in PowerShell.
- `python -I workspace\toggle_finder.py cap.csv --presses N` ranks candidate bits.
  `--exclude 0B4,025` skips IDs, `--id 59D` dumps one frame's byte history. (`--toggles`
  aligns on press times, but only if the capture has real timestamps — some adapters write
  zero timestamps over slcan, in which case rely on the flip-count ranking, which still works.)

## Before capturing

- **Only one process can open a given adapter.** If the CLI hangs on connect or a J2534
  open fails, another tool (SavvyCAN GUI, Techstream, another CLI) holds the port. Check and
  ask the user to close it; don't kill their apps.
- **Bus and speed:** most vehicle buses are 500 kbit. Ignition on with the engine off is
  usually enough as long as the feature's indicator responds; it also gives a quieter bus.
  If the indicator needs the engine running, capture at idle.
- **One bus at a time.** A car may have several CAN buses; the signal may be on one the
  adapter isn't connected to (a body network behind a gateway). If nothing matches, suspect this.

## Workflow

1. **Probe.** Capture ~5 s with `--count` small and confirm frames flow. No frames on a clean
   connect means the wrong bus, wrong speed, or the adapter isn't on the bus.

2. **Plan the toggles.** Hold each state long enough (~15 s) that even a 1 s-period frame is
   sampled in it. An even number of presses so the capture ends where it started. Nothing else
   touched (pedals, doors, windows add changing bytes). For a multi-mode control, do each mode
   as its own phase in one capture so one byte can be seen taking each value.

3. **Capture, then cue.** Start the capture in the background for the planned length plus
   margin, then tell the user to begin, because they can't see anything until your message
   lands. When the user is remote (phone), reply with just the word PRESS as the cue; one press
   per 30 s capture is enough, since a latched state then holds for the rest of the window.
   Store captures under `workspace\` with a descriptive name.

4. **Analyse.** `python -I workspace\toggle_finder.py cap.csv --presses N`. The right bit has a
   flip count equal to N. Then `--id XXX` on each candidate to see the byte history: whether
   it's a single bit, a multi-value field, or a momentary pulse, and whether a second frame
   changes a moment later (a consumer ECU echoing the state, not the source).

5. **Report plainly.** Give ID, byte, bit or value mapping, the frame's period, and which frame
   is the source versus the echo. Say what you are unsure of. Offer a DBC entry so the signal
   decodes in SavvyCAN.

6. **Confirm before the user relies on it.** A second capture with a different cadence, or
   reading the value steadily while the user holds each state, rules out a coincidence such as a
   timer that happened to line up with the presses.

## Never call a range clear without evidence of the presses

A capture with no change means either "the signal isn't here" or "nobody pressed". Those look
identical, and a user walking to the car will not press on every run. Treat "no change" as
*untested* unless the capture itself shows the presses (a known press-marker frame changing) or
the user confirms presses for that specific run. Say in the report which runs are verified and
which are not. (On the worked example below, the press marker is ID 0x499 byte 2, E4→E8.)

## Interpreting results

- A bit with ~N flips, in a frame sent every 100 ms to 1 s, is almost certainly the control.
  Mode fields are often a 2-bit value inside a status byte that also carries unrelated flags.
- A bit that flips a short delay *after* the candidate is a consumer (ABS/VDIM, ECM, cluster)
  echoing the state. Useful for learning who listens.
- A bit with ~2N flips is often the momentary "button pressed" pulse, not the latched state.
- Nothing matching: capture too short, the signal is sent only on change (look for IDs with
  very few frames), or it's on another bus.

## Worked example — Lexus IS F (in workspace/)

Found with this method: SPORT = ID 0x59D byte1 bit6 (`25 04`→`25 44`), SNOW = byte1 bit5
(`25 24`), mode press pulse = byte0 bit7; the switch-reader 0x583 leads 0x59D by ~0.1 s;
TRAC state 0x591 is the ABS's own output. Full write-up in `workspace\REVERSE-ENGINEERING.md`,
signals in `workspace\isf-sport.dbc`, gateway message list in
`workspace\isf-drivemode-can-spec.md`.
