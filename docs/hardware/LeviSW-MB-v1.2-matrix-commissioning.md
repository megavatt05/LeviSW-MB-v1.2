# LeviSW-MB v1.2 — Matrix wiring commissioning record

**Purpose:** establish the physical mapping from apartment number to one D output
and one E output before any firmware is permitted to switch a subscriber line.

**Status:** commissioning data not yet completed. This document does not certify
the physical apartment map.

## 1. Evidence from the supplied schematic fragment

The attached schematic crop shows the board D/E output signals brought to
individual test points in numerical order:

| Signal | Test point | Signal | Test point |
|---|---:|---|---:|
| D0 | TP27 | E0 | TP35 |
| D1 | TP28 | E1 | TP36 |
| D2 | TP29 | E2 | TP37 |
| D3 | TP30 | E3 | TP38 |
| D4 | TP31 | E4 | TP39 |
| D5 | TP32 | E5 | TP40 |
| D6 | TP33 | E6 | TP41 |
| D7 | TP34 | E7 | TP42 |
| — | — | E8 | TP43 |
| — | — | E9 | TP44 |

The excerpt supports signal-to-test-point correspondence. It does **not** show
the actual apartment harness mapping and must not be used as an apartment
address table.

## 2. Driver-side signal mapping already represented in the BSP

| Matrix line | Driver output | Driver channel | Test point |
|---|---|---|---:|
| D0 | MC33879A OUT1 | D0 | TP27 |
| D1 | MC33879A OUT2 | D1 | TP28 |
| D2 | MC33879A OUT3 | D2 | TP29 |
| D3 | MC33879A OUT4 | D3 | TP30 |
| D4 | MC33879A OUT5 | D4 | TP31 |
| D5 | MC33879A OUT6 | D5 | TP32 |
| D6 | MC33879A OUT7 | D6 | TP33 |
| D7 | MC33879A OUT8 | D7 | TP34 |
| E0 | TLE94112 PO1 / HB1 | HB1 | TP35 |
| E1 | TLE94112 PO5 / HB5 | HB5 | TP36 |
| E2 | TLE94112 PO7 / HB7 | HB7 | TP37 |
| E3 | TLE94112 PO9 / HB9 | HB9 | TP38 |
| E4 | TLE94112 PO6 / HB6 | HB6 | TP39 |
| E5 | TLE94112 PO4 / HB4 | HB4 | TP40 |
| E6 | TLE94112 PO3 / HB3 | HB3 | TP41 |
| E7 | TLE94112 PO10 / HB10 | HB10 | TP42 |
| E8 | TLE94112 PO11 / HB11 | HB11 | TP43 |
| E9 | TLE94112 PO8 / HB8 | HB8 | TP44 |

TLE94112 HB2 / PO2 (OpenD) and HB12 / PO12 (SW-OUT12) are reserved and are not
used as ordinary E0..E9 subscriber lines.

The D output numbering and E-to-TLE mapping above are drawn from the checked-in
board-support/bench source. Confirm them against the complete electrical
drawing for the exact PCB revision before hardware release.

## 3. Apartment map discrepancy — unresolved

Two incompatible pieces of historical software/bench information exist:

- The old sequential software convention maps flat 47 to D4 x E6.
- The LED bench record labels a D0 x E6 fixture as "flat 47".

The two must not be reconciled by guessing. The current firmware API therefore
requires a complete, one-to-one 80-entry map and rejects all routes while the
map is missing or marked unverified. The sequential map generator creates an
unverified candidate only.

## 4. Passive continuity commissioning procedure

Perform this with the board and subscriber panel unpowered. Disconnect external
equipment that could back-feed the test nodes. Do not probe a powered output
with an ohmmeter.

1. Record PCB assembly/revision, board serial number, harness drawing/revision,
   panel revision, date, operator and reviewer.
2. Verify each D signal at TP27..TP34 against the corresponding D harness pin.
3. Verify each E signal at TP35..TP44 against the corresponding E harness pin.
4. For every apartment 1..80, record exactly one D coordinate and one E
   coordinate from the actual panel harness. Record both corresponding test
   points and connector/pin identifiers from the full drawing.
5. Check for opens, shorts, unintended cross-connections and duplicate
   apartment crosspoints.
6. Confirm the resulting map uses each of the 80 D/E coordinate pairs exactly
   once. Review exceptions against the actual circuit drawing, not the software
   sequential convention.
7. Have a second engineer independently verify the full table and sign it.
8. Enter the approved table into the board application map. Only then may the
   map's verified flag be set true.

Use [LeviSW-MB-v1.2-apartment-map-template.csv](LeviSW-MB-v1.2-apartment-map-template.csv)
as the editable 80-row record. All mapping cells are deliberately blank.


## 4. Strict map generation from the commissioning record

Complete all 80 rows in
[LeviSW-MB-v1.2-apartment-map-template.csv](LeviSW-MB-v1.2-apartment-map-template.csv).
For each apartment, enter the D/E coordinate measured on the actual unpowered
harness, the corresponding test point and physical connector pins. Confirm
continuity and enter the same controlled commissioning metadata on every row:
operator, independent reviewer, ISO date, PCB revision, harness revision and
commissioning record ID.

Generate the board map from the repository root:

\`\`\`sh
python3 tools/matrix_address_map/generate_matrix_address_map.py \\
  docs/hardware/LeviSW-MB-v1.2-apartment-map-template.csv \\
  bsp/levisw-mb-v1.2/include/levisw_mb_v1_2_apartment_map_generated.h
\`\`\`

The generator rejects incomplete rows, incorrect D/E test-point numbers,
duplicate coordinates, missing continuity confirmations, inconsistent
commissioning metadata, missing connector pin references, invalid dates, or
the same person acting as operator and reviewer. Only a successful validation
produces a C map with verified=true and traceability metadata. The generated
header is an engineering-controlled artifact: review it, commit it, and include
the commissioning record in the hardware release package.

A successful script run validates the entered record's consistency; it does
not itself perform a continuity measurement or prove that the CSV entries are
true. Never use sample/test data or a sequential candidate for a physical
board. Firmware routing remains fail-closed until the approved generated map
is installed.

## 5. Powered output verification — separate approval

After passive continuity checks and map review, test the driver stages without
the subscriber panel first:

- use current-limited supplies and the board's approved rail settings;
- keep all crosspoints open during startup and check safe-off state;
- verify MC33879A D output levels at TP27..TP34;
- verify TLE94112 E output levels at TP35..TP44;
- verify active polarity and OFF/FLOATING behaviour for every E line;
- scope EN-DE, ADG1608 enable and D/E transitions;
- measure break-before-make, driver wake-up and route settling on the real
  assembly; replace provisional firmware timings with reviewed measurements;
- test one isolated dummy load / LED fixture at a time before connecting a
  subscriber panel.

Never infer subscriber loop current from DD6: it monitors the +V2 supply rail,
not the individual apartment loop.

## 6. Firmware release conditions

A physical-call firmware build is not approved until all of the following are
stored with the release:

- completed 80-entry apartment map and its revision;
- passive continuity record and independent review;
- powered test report, scope captures and current-limited supply settings;
- verified Off-Hook/On-Hook polarity on LU1 and LU2 during idle, ringing and call;
- verified E output polarity for the intended intercom operating mode;
- verified break-before-make, wake-up and route-settle times;
- documented safe-off behaviour for every SPI, telemetry and queue-loss fault.

Do not remove the address-map runtime gate to work around missing commissioning
data. A simulation-only map in Example 18 is not a physical approval.
