# Example 18 — Call Runtime software integration monitor

This target integrates:

`BSP LU ISR → LU Event Dispatcher → Call Runtime → LU Decoder → Call Controller`

and, after the address is accepted:

`active LU line → Handset Detector → CALL FSM`

## Safety boundary

**The matrix outputs are simulated.** This example intentionally supplies a
software-only Matrix Manager adapter. It does not initialise TLE94112EL or
MC33879A driver handles, enable GPIO20 EN-DE, switch ADG1608, or turn on the
TPS65261 power rails. It can exercise the event pipeline and print which route
would have been requested, without connecting subscriber lines.

Do not replace the simulation adapter with the physical adapter until the
address-map discrepancy, line polarity, output polarity and timing have been
verified on the actual board with current-limited supplies and measurement
equipment.

## Build

```powershell
cd examples/18_call_runtime_monitor
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

The example expects LU1/LU2 signals to be present on the board. With no board,
use the pure-C native tests instead.

## Default signal profile

The detector defaults to HIGH=Off-Hook, following the old example 04. Pickup
and hangup confirmation are set to 60 ms, inspired by that example's polling
cadence. These remain provisional and configurable; verify actual waveforms.
The number decoder uses the separate provisional 120 us / 80 ms profile.

Call timeouts are disabled by default. A second incoming call while another
call is active is ignored. On BSP or dispatcher event loss during a call, the
runtime requests emergency stop and latches FAULT.
