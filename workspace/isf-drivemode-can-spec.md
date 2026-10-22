# IS F drive-mode CAN spec (Sport, Snow, Traction)

Target: headless 2UR-GSE sports car, bus = ECU + TCM + ABS only, 500 kbit, 11-bit IDs.
The body/cluster ECU is gone, so the frames it used to broadcast are absent and the ADU
must generate them. Measured on the donor car (full) and the bare sports car, 2026-10-21.

## Bus ownership (who sends what)

| Frame | On bare sports car | Conclusion |
|-------|--------------------|------------|
| 0x59D (Sport/Snow state) | ABSENT | sent by body/cluster ECU → **ADU must send it** |
| 0x583 (switch reader)    | ABSENT | sent by body ECU → ADU may need to emulate |
| 0x591 (TRAC state)       | PRESENT (from ABS) | **owned by ABS — do NOT transmit this** |

## 1. Drive mode — TX 0x59D, 4 bytes, every 1000 ms

Send continuously at 1 Hz; set the data to the current mode:

| Mode   | 0x59D data  |
|--------|-------------|
| Normal | 25 04 00 00 |
| Sport  | 25 44 00 00 |
| Snow   | 25 24 00 00 |

Bit detail: byte1 bit6 = Sport, byte1 bit5 = Snow (mutually exclusive). On the donor,
byte0 momentarily reads A5/E6 for one frame at the instant of a press, but a gateway that
holds the steady value does not need that pulse IF the drivetrain reads the latched bits.
That assumption is the main thing to confirm on-car (see below).

## 2. Switch-reader heartbeat — TX 0x583, 8 bytes, every 1000 ms (include to be safe)

On the donor the drivetrain always saw this heartbeat underneath the mode frame. Until
confirmed otherwise, have the ADU emulate it so the drivetrain sees a "body ECU is alive"
signal:

| State        | 0x583 data              |
|--------------|-------------------------|
| Idle         | 66 E0 00 12 00 00 00 00 |
| Sport press  | E6 E0 20 12 00 00 00 00 (send for ~1 frame on change) |
| Snow press   | E6 E0 10 12 00 00 00 00 (send for ~1 frame on change) |

## 3. Traction control OFF — NOT YET SOLVED via CAN

0x591 is produced by the ABS, so the ADU cannot set it by transmitting (that just fights
the ABS). To turn TRAC off, the ABS must receive its button event, and that event was NOT
found on 0x583 or any identified CAN frame — on the IS F the TRAC button may be hardwired
to the ABS/body ECU rather than sent over this bus. Needs a dedicated RE pass, or the TRAC
switch may have to be wired to the ABS directly. For reference, the ABS reports TRAC state
on 0x591 byte5: 0x18 = on, 0x38 = off.

## Confirmation to run with the ADU (the J2534 adapter's TX was too unreliable to finish)

With the engine off there is no lamp and no dyno, so confirm via a drivetrain reaction frame:

1. Baseline: log the bus with the ADU sending nothing. Record every frame's steady value.
   (`workspace/sportscar-baseline.csv` is one such baseline.)
2. Have the ADU transmit **0x59D = 25 44 00 00 at 1 Hz** (plus the 0x583 idle heartbeat).
   Log again and diff. If any ECU/TCM frame changes value, the drivetrain accepted Sport
   via 0x59D status → that is all the gateway needs.
3. If nothing changes, add the 0x583 Sport-press edge (E6 E0 20 12) on top of the
   heartbeat and diff again. If that moves a drivetrain frame, the ECU/TCM key off the
   0x583 event, not the 0x59D status.
4. Best proof is behavioural: engine running, confirm throttle map / shift schedule change
   when the ADU asserts Sport.

## Verified vs assumed

- VERIFIED (donor, has body ECU): injecting an 0x583 Sport/Snow press edge, on top of the
  real 0x583 heartbeat, makes 0x59D latch Sport/Snow and hold. Second press clears it.
- ASSUMED, confirm with ADU: that on the headless car the ECU/TCM adopt the mode from the
  0x59D status frame alone (and whether the 0x583 heartbeat is a required precondition).
- OPEN: the CAN command (if any) that makes the ABS turn TRAC off.

Signal map also in `workspace/isf-sport.dbc`.
