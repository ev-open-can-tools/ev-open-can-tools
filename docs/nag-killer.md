# Nag Suppression

[Project Home](../) | [Documentation](index.md) | [Dashboard Guide](dashboard.md) | [Plugin System](plugins.md)

> **Warning:** CAN injection affects safety-critical steering systems. Bench-test configuration first. Do not use this firmware to defeat driver-attention requirements or violate applicable law.

## Required CAN Bus

Nag-suppression traffic belongs on **Party CAN**. Do not connect this function to Vehicle CAN or Chassis CAN. Connector locations and pin assignments vary by model and build date; use Tesla's [Model 3 Electrical Reference](https://service.tesla.com/docs/Model3/ElectricalReference/) to identify relevant Party CAN access point for vehicle.

## Run It

Dashboard builds use plugin injection:

1. Build and flash supported ESP-IDF dashboard environment from [Build & Flash](building.md).
2. Wire CAN transceiver to verified Party CAN high/low pair, with correct power, ground, and termination for hardware.
3. Open `http://192.168.4.1/` on device hotspot.
4. Set matching hardware mode, then install reviewed nag-suppression plugin from URL, file, or pasted JSON.
5. Enable plugin only after verifying target ID, DLC, byte layout, counter, checksum, and torque encoding for vehicle firmware.
6. Confirm dashboard reports running CAN, more than 1,000 received frames, and completed injection delay before expecting writes.
7. Use **Last Write Check** and SavvyCAN logging for observation. Neither proves ECU acceptance or safe behavior.

Non-dashboard builds can compile built-in `NAG_KILLER` handler through `platformio_profile.h`, but ESP-IDF dashboard/plugin path is preferred for runtime control and diagnostics.

## Safety And Timing

- TWAI waits 10 seconds before driver initialization so transceiver and vehicle bus can wake while dashboard stays available.
- Injection waits another 15 seconds from actual CAN initialization and requires more than 1,000 valid received frames.
- No-CAN condition keeps firmware/dashboard alive and produces throttled warnings; it never forces reboot.
- Firmware enforces complete 12-bit torque encoding and hard `-1.80 Nm` to `+1.80 Nm` bounds (`0x74E` through `0x8B6`). Configuration cannot raise these limits.
- Echo logic validates DLC, compares complete configured 12-bit torque for self-frame detection, advances counters only after successful transmission, and uses two-millisecond TWAI transmit wait.

## Target IDs And Modes

Target CAN ID must match current vehicle and plugin. In configurations derived from older standalone nag-killer modes, **Mode B target is configurable**; `0x370` is default for current EPAS Party CAN use. `0x052` is not fixed Mode B target and must not be assumed. Live mode changes reset pending torque index, hands-on sequence, and timing/periodic state inside injection decision path before any new-mode write.

## Hardware Checks

Before vehicle testing, verify:

- transceiver voltage and board logic level;
- Party CAN H/L identity and polarity;
- expected 500 kbit/s traffic with listen-only tool;
- no extended/RTR frames entering modification path;
- stable TWAI running state without bus errors;
- exact firmware-specific `0x370` layout and checksum.
