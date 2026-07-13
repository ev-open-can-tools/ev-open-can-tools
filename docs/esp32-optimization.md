# ESP32 runtime optimization

[Documentation](index.md) · [Build and flash](building.md) · [Dashboard](dashboard.md)

This page explains how to make ESP32 firmware use less time and memory without changing CAN behavior. It is for contributors and advanced testers. The rule is simple: measure first, change one thing, then measure again.

## What is already applied

- Support diagnostics are on demand instead of continuously assembled in the browser.
- Frequent `/status` output uses a bounded fixed buffer instead of repeated text concatenation.
- Idle dashboard traffic is about 22 requests/minute instead of about 72 in the previous design.
- Idle web-maintenance and disabled-GVRET wakeups are 4/second instead of 100/second.
- CAN task timing, CAN task priority, injection timing, physical limits, and safety gates are unchanged.
- Chip, flash, and RAM totals are cached at boot; dynamic heap, stack, WiFi, CAN, queue, and HTTP values are collected when needed.

These are software measurements and estimates. Real stack margin, heap fragmentation, CAN queue pressure, and WiFi coexistence need hardware runs.

## What the metrics mean

- **Free heap:** memory available now.
- **Minimum free heap:** lowest observed free heap since boot.
- **Largest free block:** largest single allocation that can fit; it can fall because of fragmentation even when free heap looks healthy.
- **Stack high-water mark:** remaining task stack margin; lower is closer to overflow.
- **Queue maximum:** highest observed backlog; a growing value means a task cannot keep up.
- **Wakeups:** task loop iterations, not necessarily useful work.
- **Request rate:** dashboard HTTP requests, separated from device CAN traffic.

Open **Support diagnostics** after a representative run to capture these values. Do not compare numbers from different board targets as if they were identical.

## Deferred work: measure before changing

| Area | Current approach | Safe next experiment | Main risk | Evidence to collect | Hardware? |
| --- | --- | --- | --- | --- | --- |
| Task stacks | Conservative task stacks; Support reports high-water marks | Reduce one stack only after stress testing with at least 25% margin | Rare stack overflow/reset | Stack watermark, watchdog and reset reason | Yes |
| Heap fragmentation | Fixed status/support buffers; other handlers use small dynamic objects | Repeat WiFi scans, plugin installs, settings restore, and OTA checks; use ESP-IDF heap tools when available | Fragmentation or permanent RAM cost | Free heap, minimum heap, largest block over hours; heap tracing and allocation failure hooks | Yes |
| Task priorities | Existing CAN priorities and pinning | Trace deadline misses before tuning | CAN jitter, starvation, watchdog | App Trace/SystemView, queue depth, write latency | Yes |
| Dashboard polling | Slow status/WiFi polling; active-only GVRET polling | Stream updates only if request count is still material | Socket/reconnect complexity | HTTP count/bytes, HTTP CPU, reconnect errors | Recommended |
| Logging | Heartbeat and fault logs retained; GVRET stays binary-clean | Remove only proven noisy release logs | Less field evidence | Serial bytes, task CPU, captured faults | Recommended |
| Dashboard asset | Deterministic compressed asset in flash | Revisit only if image size becomes a limit | Browser/update compatibility | Raw/gzip length, flash sections, page latency | Optional |
| JSON | Fixed buffers only where frequent; ArduinoJson elsewhere | Convert one proven hot response | Malformed output or code complexity | Allocation failures, response validity, heap | Yes |
| NVS wear | Writes occur on durable changes, not polling | Batch only a measured multi-key path | Lost settings or flash wear | Commit errors, reboot recovery, write count | Yes |
| CAN queue pressure | Existing bounded queues and counters | Tune only after observing drops/backlog | Dropped frames or changed timing | RX/TX queue max, missed/error counters | Yes |
| Compiler optimization | Target-specific existing settings | Compare size/debug/optimization flags one at a time | Image incompatibility or timing shift | RAM/flash, warnings, runtime trace | Build + hardware |
| Firmware/partition size | Board-specific OTA partitions | Resize only with a release and OTA compatibility check | Unbootable or non-updatable image | ELF sections, binary size, partition headroom | Build + hardware |
| CPU profiling | Heartbeats and counters, no continuous profiler | Use CPU profiling only after a reproducible workload exists | Probe overhead or changed timing | App Trace/SystemView, task runtime, CAN latency | Yes |

## Safe measurement sequence

1. Record board, firmware version, build flags, partition, traffic rate, and test duration.
2. Capture Support diagnostics before the experiment.
3. Exercise CAN observation, dashboard requests, WiFi reconnect, plugin operations, and GVRET separately.
4. Repeat with combined load.
5. Change one variable.
6. Re-run the same workload and compare RAM, flash, heap, stack, queue, HTTP, and CAN metrics.
7. Stop if CAN age, missed frames, queue pressure, watchdog events, or safety-gate behavior changes.

Never optimize a CAN callback by adding network I/O, NVS writes, JSON parsing, verbose logging, or an unbounded allocation. Keep diagnostics outside the CAN hot path and fail closed on report overflow.
