# Matrix Manager

The Matrix Manager is the sole software owner of the 8x10 subscriber matrix.
Call Controller must be the only application-level client. No monitor, decoder,
test loop, or UI task may write ADG1608, MC33879A, or TLE94112 directly while
this manager is active.

## Route model

For apartment number 1..80:

- zero-based index = apartment - 1;
- decade D index = index / 10 (0..7);
- unit E index = index % 10 (0..9);
- D index maps to one MC33879A OUT bit;
- E index maps to one TLE94112 half-bridge using the non-linear BSP map;
- ADG1608 selects the decade and is enabled only after the D/E outputs are configured.

The hardware adapter must implement the non-linear unit-to-HB mapping using
`levisw_bsp_unit_to_tle_hb()`. The reserved TLE outputs HB2/OpenD and
HB12/SW-OUT12 are never selected as ordinary E0..E9 units.

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

## Hardware integration gate

The core deliberately requires the board adapter to specify HIGH_SIDE or
LOW_SIDE for the selected E output; it has no polarity default. The LED bench
report demonstrated MC D-high-side plus one TLE E-low-side output, but this is
not, by itself, proof that every intercom call mode must use that exact polarity.
The first board adapter should use a named, explicitly reviewed operating
profile and be scope-verified before connecting an apartment line.

The 150 us break-before-make and 500 us route-settle values are provisional
software starting points only. The 15 ms driver wake interval is taken from the
board's documented bench sequence. Verify all timings and output polarity on
the real board with current-limited supplies before intercom use.

## Native tests

```sh
cmake -S components/matrix_manager/tests -B build/matrix_manager_tests
cmake --build build/matrix_manager_tests
ctest --test-dir build/matrix_manager_tests --output-on-failure
```
