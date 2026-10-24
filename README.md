# SavvyCAN
Qt based cross platform canbus tool
(C) 2015-2021 EVTV and Collin Kidder

A Qt5 based cross platform tool which can be used to load, save, and capture canbus frames.
This tool is designed to help with visualization, reverse engineering, debugging, and
capturing of canbus frames.

Please use the "Discussions" tab here on GitHub to ask questions and interact with the community.

Requires a resolution of at least 1024x768. Fully multi-monitor capable. Works on 4K monitors as well.

You are highly recommended to use the
[CANDue board from EVTV](http://store.evtv.me/proddetail.php?prod=ArduinoDueCANBUS&cat=23).

The CANDue board must be running the GVRET firmware which can also be found
within the collin80 repos.

It is now possible to use any Qt SerialBus driver (socketcan, Vector, PeakCAN, TinyCAN).
There may, however, be some loss of some functionality as
some functions of SavvyCAN are designed for use directly with the
EVTVDue and CANDue 2.0 boards.

It should, however, be noted that use of a capture device is not required to make use
of this program. It can load and save in several formats:

1. BusMaster log file
2. Microchip log file
3. CRTD format (OVMS log file format from Mark Webb-Johnson)
4. GVRET native format
5. Generic CSV file (ID, D0 D1 D2 D3 D4 D5 D6 D7)
6. Vector Trace files
7. IXXAT Minilog files
8. CAN-DO Logs
9. Vehicle Spy log files
10. CANDump / Kayak (Read only)
11. PCAN Viewer (Read Only)

## CLI Mode

SavvyCAN includes a headless CLI mode for live CAN capture, sending, ISO-TP/UDS diagnostics, and file playback without launching the GUI.

### Basic Usage

```sh
# Capture frames from a LAWicel/SLCAN device
SavvyCAN --cli --port COM6 --speed 500000

# Capture and save to file
SavvyCAN --cli --port COM6 --speed 500000 --output capture.csv

# Capture 100 frames then stop
SavvyCAN --cli --port COM6 --speed 500000 --count 100 --output log.crtd --format crtd

# Listen-only mode (no transmit)
SavvyCAN --cli --port COM6 --speed 500000 --listen-only
```

### Sending CAN Frames

```sh
# Send a single frame (ID#HEXDATA)
SavvyCAN --cli --port COM6 --speed 500000 --send 7DF#0201050000000000

# Send multiple frames
SavvyCAN --cli --port COM6 --speed 500000 --send 7DF#0201050000000000 --send 7E0#0210010000000000
```

### ISO-TP Support

ISO-TP (ISO 15765-2) provides transport-layer segmentation and reassembly for messages larger than 8 bytes.

```sh
# Listen for ISO-TP messages (reassembles multi-frame messages)
SavvyCAN --cli --port COM6 --speed 500000 --isotp-listen

# Send an ISO-TP message (auto-fragments if >7 bytes)
SavvyCAN --cli --port COM6 --speed 500000 --isotp-listen --isotp-send 7E0#22F190

# Use extended addressing mode
SavvyCAN --cli --port COM6 --speed 500000 --isotp-listen --isotp-extended
```

### UDS Diagnostics

UDS (Unified Diagnostic Services, ISO 14229) is used for ECU diagnostics. The CLI decodes service names and error codes automatically.

```sh
# Listen and decode UDS messages
SavvyCAN --cli --port COM6 --speed 500000 --uds-listen

# Send a Diagnostic Session Control request (service 0x10, subfunction 0x01)
SavvyCAN --cli --port COM6 --speed 500000 --uds-listen --uds-send 7E0#10.01

# Read Data By Identifier (service 0x22, DID 0xF190 = VIN)
SavvyCAN --cli --port COM6 --speed 500000 --uds-listen --uds-send 7E0#22.F190

# ECU Reset (service 0x11, subfunction 0x01 = hard reset)
SavvyCAN --cli --port COM6 --speed 500000 --uds-listen --uds-send 7E0#11.01
```

### File Playback

Play back captured CAN log files to hardware. Supports all 24 file formats that SavvyCAN can load (GVRET CSV, CRTD, BusMaster, Vector Trace, CANalyzer ASC/BLF, candump, PCAN, and more).

```sh
# Play a log file to hardware
SavvyCAN --cli --port COM6 --speed 500000 --playback capture.csv

# Use original timing from the recording
SavvyCAN --cli --port COM6 --speed 500000 --playback capture.csv --playback-original

# Fast playback (1ms interval, 10 frames per tick)
SavvyCAN --cli --port COM6 --speed 500000 --playback capture.csv --playback-speed 1 --playback-burst 10

# Loop playback 3 times
SavvyCAN --cli --port COM6 --speed 500000 --playback log.asc --playback-loop 3

# Infinite loop
SavvyCAN --cli --port COM6 --speed 500000 --playback log.blf --playback-loop -1

# Send on all buses
SavvyCAN --cli --port COM6 --speed 500000 --playback log.csv --playback-bus -1
```

### Example Output

Raw frame capture:
```
Connecting to LAWicel device on COM6 at 500000 bps...
Device connected (1 bus(es))
Connected! Listening for CAN frames...
(000012.345678) can0  0x7DF  [8]  02 01 05 00 00 00 00 00
(000012.346789) can0  0x7E8  [8]  04 41 05 7A 00 00 00 00
(000012.401234) can0  0x18A  [8]  00 00 1B 00 00 00 00 1B
```

UDS diagnostic session:
```
Connecting to LAWicel device on COM6 at 500000 bps...
UDS listener enabled
Connected! Listening for CAN frames...
Sending UDS: ID=7E0 Service=DIAG_CONTROL(0x10) Data=[1 bytes]
(000000.278803) can0  0x7E0  [8]  03 10 01 01 00 00 00 00  TX
[UDS] (000000.278803) can0  0x7E0  DIAG_CONTROL  SubFunc:01  [3 bytes]  10 01 01
[UDS] (000000.283456) can0  0x7E8  DIAG_CONTROL  SubFunc:01  [6 bytes]  50 01 00 19 01 F4
```

UDS negative response:
```
[UDS] (000001.123456) can0  0x7E8  NEG_RESPONSE  Service:SECURITY_ACCESS  Error:COND_INCORR
```

ISO-TP reassembled message:
```
[ISOTP] (000012.567890) can0  0x7E8  [17 bytes]  62 F1 90 57 30 4C 30 30 30 30 34 33 4D 42 35 34 31
```

File playback:
```
Loading capture.csv...
Loaded 12974 frames from capture.csv
Connecting to LAWicel device on COM6 at 500000 bps...
Device connected (1 bus(es))
Connected! Listening for CAN frames...
Starting playback (12974 frames, 1 loop(s))...
(000000.033435) can0  0x23A  [8]  00 37 00 12 00 00 00 00  TX
(000000.033530) can0  0x28C  [8]  00 00 00 00 00 00 F0 00  TX
(000000.033661) can0  0x22A  [8]  67 01 5E 1D 00 00 00 00  TX
...
Playback complete.
```

### All CLI Options

```
Options:
  --cli                  Run in CLI mode (no GUI)
  -p, --port <port>      Serial port for LAWicel device
  -s, --speed <speed>    CAN bus speed in bps (default: 500000)
  --serial-speed <baud>  Serial baud rate (default: 115200)
  --send <frame>         Send a CAN frame (ID#HEXDATA)
  -o, --output <file>    Save captured frames to file
  -f, --format <format>  Output format: csv, crtd, candump (default: csv)
  --listen-only          Enable listen-only mode
  -c, --count <count>    Stop after N frames (default: unlimited)
  --duration <seconds>   Stop after N seconds (default: unlimited). csv output is
                         written as frames arrive, so a crash keeps what was captured
  -q, --quiet            Do not print every frame; only status lines
  --device-time          Keep the adapter's own frame timestamps instead of host
                         time (microseconds since epoch)
  --isotp-listen         Enable ISO-TP message reassembly
  --isotp-send <frame>   Send ISO-TP message (ID#HEXDATA)
  --isotp-extended       Use ISO-TP extended addressing
  --uds-listen           Enable UDS message decoding
  --uds-send <frame>     Send UDS request (ID#SERVICE.SUBFUNC[.DATA])
  --playback <file>      Play back a CAN log file to hardware
  --playback-speed <ms>  Playback timer interval in ms (default: 5)
  --playback-burst <n>   Frames per tick (default: 1)
  --playback-original    Use original frame timing from file
  --playback-bus <n>     Send on bus N (-1=all, -2=from file, default: 0)
  --playback-loop <n>    Loop N times (-1=infinite, default: 1)
```

### Logging a whole session (cold start) through the XHorse

The XHorse / clone MVCI driver crashes after roughly a minute of an open channel, so a
single long capture is impossible. `log-session.cmd <name>` runs the 32-bit CLI in 45 s
chunks, each streamed to `workspace\logs\<name>\chunk-NNN.csv` with host timestamps, and
restarts around the crash; `log-session.cmd merge <name>` joins the chunks. Start it before
turning the ignition on. The `/savvy-log-session` skill has the full procedure.

## Dependencies and building

See **[BUILD.md](BUILD.md)** for the full dependency list (Qt 5.15.2 modules, Visual Studio
Build Tools, Linux packages, optional Python tooling) and the build steps for Windows x86,
Windows x64 and Linux.

Short version on Windows: install the two Qt 5.15.2 MSVC kits with aqtinstall, then run
`build-win.cmd`. The release folders are `bin\x86\` and `bin\x64\`. `clean.cmd` removes all
generated output for a clean build.

## What to do if your compile failed?

The very first thing to do is try:

```
qmake
make clean
make
```

Did that fix it? Great! If not, ensure that you selected SerialBUS support when you installed Qt.

### Used Items Requiring Attribution

nodes by Adrien Coquet from the Noun Project

message by Vectorstall from the Noun Project

signal by shashank singh from the Noun Project

signal by juli from the Noun Project

signal by yudi from the Noun Project

Death by Adrien Coquet from the Noun Project
