# Call Runtime — decoder / handset / FSM integration

This single-owner runtime connects the pure components:

`LU edge stream → LU Decoder → Call Controller → Matrix Manager`

and, after a route is connected:

`LU edge stream on the active line → Handset Detector → Call Controller`

## Processing rules

1. While STANDBY, both LU lines are fed into the decoder.
2. A complete valid address result generates CALL_DETECTED and NUMBER_READY.
   The Call Controller validates the address and requests the Matrix Manager route.
3. Only after the Matrix Manager reports the route connected does Runtime arm
   the Handset Detector for the LU line that produced the number. The current
   level is a baseline; a new Off-Hook transition is required.
4. A fresh Off-Hook transition reports PICKUP_CANDIDATE. A sustained level
   reports PICKUP_CONFIRMED and enters TALKING. A short transition is rejected.
5. A sustained On-Hook transition reports HANGUP and disconnects the route.
6. Events from another LU line do not retarget an active call. A second call
   arriving while busy is not accepted.
7. If the BSP/dispatcher reports dropped events during an active call, Runtime
   sends a hardware-fault event and forces Matrix Manager emergency stop.
   In STANDBY it simply re-baselines the decoder.

Call Runtime is single-task-owned. An application must use exactly one task to
call feed/tick/notify/reset APIs. The LU Event Dispatcher continues to have one
decoder-queue consumer; that task forwards each event to Runtime.

## Handset polarity and timing

The board's legacy example 04 labels HIGH as Off-Hook and LOW as On-Hook. This
is a configurable profile, not a verified electrical contract for every panel.
Older bench notes discuss LU signal transitions under ringing separately.
The defaults (HIGH active, 60 ms pickup and 60 ms hangup verification) are
provisional. Capture actual LU waveforms and verify both channels before
connecting subscriber lines.

DD6 is the +V2 rail monitor and is not a subscriber-loop current sensor. No
handset threshold is inferred from INA226.

## Address-map gate

The runtime inherits the sequential 1..80 software map from Matrix Manager.
The unresolved discrepancy between "flat 47 = D4×E6" (sequential map) and
the LED bench report's "flat 47 = D0×E6" remains a hard integration gate.
Do not enable real crosspoint outputs until the physical apartment wiring map
has been reconciled.

## Native tests

```sh
cmake -S components/call_runtime/tests -B build/call_runtime_tests
cmake --build build/call_runtime_tests
ctest --test-dir build/call_runtime_tests --output-on-failure
```
