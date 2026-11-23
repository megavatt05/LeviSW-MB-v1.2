# LU event dispatcher

This ESP-IDF component is the **only consumer** of
`levisw_bsp_lu_event_receive()`. It forwards each timestamped raw transition
to two independent FIFO queues:

- **decoder queue** — consumed by exactly one protocol decoder task;
- **monitor queue** — consumed by exactly one diagnostics/stable-level task.

The dispatcher does not debounce, decode apartment numbers, access I2C/SPI,
or control the matrix. The downstream queues are independent: a slow monitor
cannot steal events from the decoder.

Queue overflows and BSP receive errors are counted separately. The decoder
application must discard its current partial frame and resynchronise GPIO levels
after a decoder-queue drop, BSP queue overflow, or BSP read error. A consumer
must not silently treat an incomplete event stream as a valid apartment code.

Call `lu_event_dispatcher_start()` only after `levisw_bsp_init()`. Stop subscriber
tasks before calling `lu_event_dispatcher_stop()`. Each receive queue is
single-consumer; additional listeners must be added as another explicit fan-out
queue rather than calling a receive API from a second task.
