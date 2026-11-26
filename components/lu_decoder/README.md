# LU pulse decoder (platform independent)

`lu_decoder.c` is a pure C state machine. It has no dependency on ESP-IDF,
GPIO, FreeRTOS, I2C, SPI, or the board support package. The ESP application
supplies monotonic LU edge events and periodically calls `lu_decoder_poll()`
during idle periods.

## Provisional decoding profile

The default profile is derived from historical bench notes, not from a
published intercom protocol specification:

- reject a transition that returns to the previous level before 120 us;
- close a burst after 80 ms without a qualified edge;
- interpret two qualified level transitions as one apartment count;
- accept resulting apartment numbers 1..80.

All timings and the transition divisor are configurable. They must be validated
against the actual panel with a scope/logic analyser before enabling any matrix
routing. The decoder only reports a candidate apartment; it does not activate
relays, drivers, power rails, or crosspoints.

A missing single transition produces an odd/repeated-level sequence and is
rejected. A whole missing pulse (two transitions) cannot be detected by
pulse-count-only coding: it is indistinguishable from a lower apartment number.
Reliable detection of that failure requires a panel framing/checksum rule or a
verified timing/code constraint that is not yet documented.

## Native host tests

```sh
cmake -S components/lu_decoder/tests -B build/lu_decoder_tests
cmake --build build/lu_decoder_tests
ctest --test-dir build/lu_decoder_tests --output-on-failure
```

Tests cover all addresses 1..80, a sub-filter glitch, a missing transition,
the whole-pulse ambiguity, and an out-of-range code.
