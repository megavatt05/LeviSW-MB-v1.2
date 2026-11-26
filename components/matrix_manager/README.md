# Matrix Manager

The Matrix Manager is the sole software owner of the 8x10 subscriber matrix.
Call Controller must be the only application-level client. No monitor, decoder,
test loop, or UI task may write ADG1608, MC33879A, or TLE94112 directly while
this manager is active.

## Route model and address-map gate

An apartment number is not converted to D/E coordinates by arithmetic at
runtime. The manager requires an explicit versioned 80-entry board address map.
Each entry contains one D index (0..7) and one E index (0..9).

The map validator requires:
- format version 1;
- all coordinates within D0..D7 and E0..E9;
- no duplicate crosspoints and every crosspoint used exactly once;
- null-terminated PCB revision, harness revision and commissioning-record fields;
- for verified maps, all traceability fields must be non-empty.

A sequential 1..80 map can be generated as a candidate for tests and simulation,
but is always created with verified=false. Routing is rejected with
MATRIX_MANAGER_ERR_ADDRESS_MAP_UNVERIFIED until an installed map is explicitly
marked verified. The flag represents a commissioning approval; it is not a
live electrical self-test. matrix_manager_is_ready() is false until a verified
map is installed.

## Board signal and test-point correspondence

The supplied schematic fragment shows the D and E signal test-point groups:

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

The board source identifies the D outputs as MC33879A OUT1..OUT8 and the
E outputs as TLE94112EL half-bridges with this non-linear mapping:

| E line | TLE output | Hardware half-bridge |
|---|---|---|
| E0 | PO1 | HB1 |
| E1 | PO5 | HB5 |
| E2 | PO7 | HB7 |
| E3 | PO9 | HB9 |
| E4 | PO6 | HB6 |
| E5 | PO4 | HB4 |
| E6 | PO3 | HB3 |
| E7 | PO10 | HB10 |
| E8 | PO11 | HB11 |
| E9 | PO8 | HB8 |

HB2 / PO2 (OpenD) and HB12 / PO12 (SW-OUT12) are not ordinary E0..E9 lines.
The schematic fragment confirms signal-to-test-point correspondence only. It
does not identify the apartment-to-crosspoint wiring.

## Commissioning procedure

Complete the [hardware matrix commissioning record](../../docs/hardware/LeviSW-MB-v1.2-matrix-commissioning.md)
and its [80-apartment CSV template](../../docs/hardware/LeviSW-MB-v1.2-apartment-map-template.csv).
The strict generator validates all apartment rows, connector pins, test points,
continuity confirmations, independent review and board/harness revision metadata
before generating a C header with verified=true.

Keep all power rails disabled and EN-DE low during passive continuity checks.
Disconnect external circuits that could back-feed test nodes. Powered checks are
a separate step using current-limited supplies and measurement equipment.
Do not mark the synthetic sequential map verified to make the application run.

### Known address discrepancy

The software convention previously mapped flat 47 to D4 x E6, while the LED
bench note calls a D0 x E6 test fixture "flat 47". This is unresolved. The map
API supports either coordinate assignment, but neither is approved by this
document. Resolve it from actual continuity evidence before physical call routing.

## Break-before-make sequence

Every route change first opens ADG1608, turns all MC33879A D outputs off,
places all TLE94112 half-bridges in FLOATING, then disables EN-DE. After the
configured blanking interval the manager enables the drivers, waits for wake-up,
sets the selected decade while the analog switch is still disabled, enables one
D output and one E output, closes ADG1608, then waits the configured route-settle
interval before reporting the route active.

Any failed operation causes a latched FAULT and a best-effort attempt to open
the mux, turn D outputs off, float all E outputs, and disable EN-DE. Recovery
is explicit and succeeds only if the complete safe-off sequence succeeds.

The core does not power TPS65261 rails or infer that PGOOD means line-safe.
Power sequencing and telemetry validation remain the responsibility of the
system supervisor before this manager is allowed to connect a route.

## Electrical timing and polarity

The core deliberately requires the board adapter to specify HIGH_SIDE or
LOW_SIDE for the selected E output; it has no polarity default. The LED bench
report demonstrated MC D-high-side plus one TLE E-low-side output, but this is
not proof that every intercom call mode must use that exact polarity.

The 150 us break-before-make and 500 us route-settle values are provisional
software starting points only. The 15 ms driver wake interval is taken from
the board's documented bench sequence. Verify all timings and output polarity
on the real board with current-limited supplies before intercom use.

## Native tests

\`\`\`sh
cmake -S components/matrix_manager/tests -B build/matrix_manager_tests
cmake --build build/matrix_manager_tests
ctest --test-dir build/matrix_manager_tests --output-on-failure
python3 -m unittest discover -s tools/matrix_address_map/tests -v
\`\`\`
