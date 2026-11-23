# Handset Detector

Platform-independent detector for the two LU Schmitt-trigger outputs. It does
not access GPIO, FreeRTOS, I2C, SPI, INA226, or subscriber-line power.

The existing board example 04 documents LU1/LU2 HIGH as Off-Hook and LOW as
On-Hook. This is used as the configurable default polarity, not a universal
electrical guarantee. Older lab notes describe signal behavior in a ringing
context differently; verify actual waveforms and polarity at XP1/TP7/TP6 and
TP50/TP51 before enabling call routing.

The detector is armed only after a valid apartment route has been connected.
The current input level becomes its baseline, so a line already HIGH at arm
time cannot immediately create a false pickup. A fresh Off-Hook edge creates a
PICKUP_CANDIDATE; a sustained level confirms pickup. A short return to On-Hook
rejects the candidate. During TALKING, a sustained On-Hook level confirms hangup.

Default 60 ms pickup and 60 ms hangup durations are provisional software
settings, inspired by the legacy polling example cadence; they are not measured
panel timing requirements. Configure and scope-verify them for the actual
intercom. The detector deliberately does not infer loop current from DD6:
DD6 measures the +V2 power rail, not the subscriber loop.

## Native tests

```sh
cmake -S components/handset_detector/tests -B build/handset_detector_tests
cmake --build build/handset_detector_tests
ctest --test-dir build/handset_detector_tests --output-on-failure
```
