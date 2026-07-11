# Dashboard Guide

[Project Home](../) | [Documentation](index.md) | [Build & Flash](building.md) | [Nag Suppression](nag-killer.md) | [Plugin System](plugins.md) | [Release Notes](../CHANGELOG.md)

ESP32 builds with `ESP32_DASHBOARD` serve a local dashboard at `http://192.168.4.1/` while connected to device hotspot. Dashboard starts before CAN initialization, so WiFi, configuration, and diagnostics remain available during driver wake-up, CAN failure, and bus recovery.

## Runtime Status And Safety

- **Runtime status** shows CAN availability, injection state, received-frame count, last-frame age, successful/failed transmissions, TWAI state, free heap, and uptime.
- TWAI state colors are green for running, red for bus-off, and yellow for stopped or recovering.
- **Last Write Check** compares latest write attempt with next received frame having same CAN ID and mux. It can reveal later overwrites. It does not prove an ECU accepted a write.
- TWAI builds wait 10 seconds before initializing CAN. All ESP-IDF injection remains blocked for 15 seconds after actual CAN initialization and until more than 1,000 valid CAN frames have arrived.
- Missing CAN traffic never reboots firmware. Dashboard stays online and serial emits throttled `No CAN frames yet, staying alive.` warnings.
- A bus-off state starts ESP-IDF TWAI recovery. Runtime status and five-second serial heartbeat expose recovery state and counters.

## Configuration

- Select live hardware mode: `Legacy`, `HW3`, or `HW4`.
- Use automatic speed profile or select compatible manual profile. `Max` and `Sloth` require HW4.
- Configure plugin replay, AP injection gate, HW3/HW4 offset slew, and status LED brightness.
- Stop or arm injection and reboot device.
- Mode changes reset pending injection sequence state inside CAN decision path. Saved configuration cannot consume transition early.

## Plugins

- Install plugin from HTTPS URL, uploaded `.json`, or pasted JSON.
- Enable, disable, prioritize, or remove installed plugins.
- Dashboard builds use enabled plugins for automatic CAN injection; built-in vehicle handlers remain observational.
- Installed plugins and their enabled state persist on SPIFFS.

See [Plugin System](plugins.md) for JSON schema and safety rules.

## SavvyCAN USB Serial

1. Open dashboard **SavvyCAN USB serial** card and select **Arm GVRET**.
2. Connect board USB port to computer.
3. In SavvyCAN, choose **Connection → Add Connection → Serial Connection (GVRET)**.
4. Select board serial port and `115200` baud.

Firmware exposes one read-only 500 kbit/s CAN bus using reference GVRET framing. Valid GVRET device-info, bus-count, bus-parameter, and keep-alive commands are supported. CAN transmit commands are not advertised or invented. When SavvyCAN handshake succeeds, text logs are suppressed on that transport so binary frames remain clean. Disconnect, 15 seconds without a command, manual stop, or 10-minute session limit releases serial ownership. CAN, web, and main loops remain nonblocking.

## Connectivity And Updates

- Change hotspot SSID/password and visibility.
- Save multiple WiFi networks with DHCP or validated static IPv4 settings.
- Override supported TWAI TX/RX GPIO pins.
- Export or restore settings with OTA authentication.
- Check stable/beta GitHub releases, configure automatic updates, or upload board-specific `.bin` firmware.

## Dashboard Source Ownership

[`include/web/mcp2515_dashboard_ui.src.h`](../include/web/mcp2515_dashboard_ui.src.h) is authoritative HTML/CSS/JavaScript source. [`include/web/mcp2515_dashboard_ui.h`](../include/web/mcp2515_dashboard_ui.h) is generated raw/gzip firmware data. Dashboard PlatformIO builds regenerate it through `scripts/minify_dashboard.py`; generator uses Python standard library and fixed gzip timestamp for reproducible output. Edit source file, never generated header directly.

## Support Diagnostics

Collapsed **Support diagnostics** section is final dashboard card. Expanding it makes one on-demand `/support` request; it never polls. Report covers firmware/build/chip/flash/reset, heap and stack watermarks, WiFi identity and signal, CAN/TWAI state and queue pressure, filters and safety gates, Last Write Check, NVS recovery, GVRET, and web/request counters. It intentionally excludes passwords, OTA credentials, tokens, keys, and plugin payloads. Use **Refresh report** for new snapshot and **Copy report** before opening GitHub issue.

See [ESP32 Runtime Optimization](esp32-optimization.md) for measured overhead reductions and further hardware-gated tuning guidance.
