# Call Controller FSM

The Call Controller is a platform-independent, event-driven finite-state
machine. It receives *semantic* events from the LU decoder and a future handset
detector; it does not read GPIO or access the bus drivers directly.

## States

```text
STANDBY
  -> CAPTURE_NUMBER
  -> VALIDATE_TARGET
  -> ROUTE_PREPARE
  -> RINGING
  -> ANSWER_VERIFY
  -> TALKING
  -> HANGUP
  -> STANDBY

Any hardware/Matrix Manager error -> FAULT
FAULT -> explicit FAULT_RESET after Matrix Manager safe-off succeeds
```

- `CALL_DETECTED` starts number capture.
- `NUMBER_READY` is accepted only in CAPTURE_NUMBER and only when the decoder
  marks the packet valid and the apartment is in 1..80.
- ROUTE_PREPARE calls Matrix Manager. RINGING begins only after the route is
  successfully connected.
- PICKUP_CANDIDATE starts ANSWER_VERIFY. Only PICKUP_CONFIRMED enters TALKING.
  A rejected or unconfirmed pickup returns to RINGING without opening the route.
- The selected crosspoint remains continuously connected in RINGING,
  ANSWER_VERIFY, and TALKING. There is no subscriber scan while a call is active.
- HANGUP/CANCEL opens the crosspoint and returns to STANDBY.
- A hardware fault latches FAULT and invokes Matrix Manager emergency stop.
  Reset is explicit; it does not automatically retry the call.

## Boundaries not implemented yet

This FSM does not generate an analog ring voltage or audio, and it does not
decide handset pickup. A separate detector must provide stable semantic
PICKUP_CANDIDATE / PICKUP_CONFIRMED / PICKUP_REJECTED / HANGUP events.
No direct voltage/current threshold is assumed. DD6 measures +V2 rail current,
not subscriber loop current, and its conversion time is unsuitable as a
sub-millisecond loop detector.

Timeout values default to disabled (zero) until measured panel timing is
available. Configure them explicitly after capturing real call traces.

## Host tests

```sh
cmake -S components/call_controller/tests -B build/call_controller_tests
cmake --build build/call_controller_tests
ctest --test-dir build/call_controller_tests --output-on-failure
```

The tests use fake Matrix Manager hardware operations and verify the normal
lifecycle, rejection of invalid addresses, timeouts, no rescan during TALKING,
and fault recovery.
