# Dashboard guide

[Documentation](index.md) · [Build and flash](building.md) · [Plugins](plugins.md) · [CAN safety](nag-killer.md) · [Release notes](../CHANGELOG.md)

ESP32 dashboard builds create a private web page on the board. Connect your phone or laptop to the board hotspot and open `http://192.168.4.1/`. The dashboard is a control and observation tool, not proof that an ECU accepted a message.

## Safe first session

1. Keep **injection stopped**.
2. Confirm the board and CAN driver in the status card.
3. Check that received-frame count and frame age change as expected.
4. Resolve wiring, bitrate, bus, or recovery errors before installing a plugin.
5. Use [SavvyCAN](#savvycan-usb-serial) or the Support report to collect evidence.

## Status card

The status card shows the current CAN state, received-frame count, newest-frame age, transmit successes/failures, uptime, heap, and driver information. A missing frame does not reboot the firmware; the dashboard stays available and logs are throttled.

**Last Write Check** compares a write attempt with the next received frame that has the same ID and mux. It can show whether another transmitter overwrote a value. It cannot prove ECU acceptance, physical effect, or safety.

TWAI colors mean:

- green: running;
- yellow: stopped or recovering;
- red: bus-off or another error state.

## Configuration

The configuration card controls settings such as:

- vehicle mode: `Legacy`, `HW3`, or `HW4`;
- automatic or manual speed profile;
- plugin replay and AP-injection gate;
- beta Summon-only injection;
- HW3/HW4 offset slew and status LED brightness;
- CAN pins on supported TWAI boards;
- WiFi, update channel, automatic updates, and saved networks.

Save one change at a time and watch the status card afterward. A setting being accepted by the form does not make an unverified CAN rule safe.

### Built-in nag suppression

The Configuration card has four direct firmware controls: **Off**, **Mode A**, **Mode B**, and **Mode C**. These modes do not install or execute a plugin. Dashboard builds default to Off and preserve the selection in NVS and settings backups.

- **Mode A** echoes eligible Party CAN `0x370` frames with fixed `+1.80 Nm`, `handsOnLevel=1`, counter `+1`, and a recalculated checksum.
- **Mode B** cycles `+1.80`, `+1.50`, `-1.50`, and `-1.80 Nm` every 200 ms during a one-second active burst, then pauses for 1.5 seconds.
- **Mode C** observes DAS state on `0x399` for Legacy/HW3, plus steering angle on `0x129`. It validates the steering-angle signal, blocks unless both frames are fresh within one second, requires AP state 3 through 6 and steering angle within `±5 degrees`, then applies the hands-on state-machine delay. Context observation continues while the AP gate is closed, but transmission remains blocked.

Modes A/B are available on HW4 for Party CAN experiments. Mode C remains blocked on HW4 after extended testing reported red take-over plus traction-control and auto-hold faults; a stored HW4 Mode C selection fails closed to Off.

All modes retain the firmware `±1.80 Nm` hard bound and global dashboard injection gates. Use Party CAN only and validate exact frame layouts against a capture from the target vehicle before enabling any mode.

### Summon-only injection (beta)

Use this optional mode only when normal injection interferes with Autopilot on newer Tesla software. It defaults to disabled, including after upgrading. Existing injection behavior remains unchanged while disabled.

When enabled, one final driver-level policy protects every ESP-IDF transmit path. Injection is allowed only when fresh CAN state confirms either:

- `DI_gear=P` on `DI_systemStatus` (`0x118`) and `DI_vehicleSpeed=0.00 km/h` on `DI_speed` (`0x257`); or
- `DI_autonomyControlActive=1` on `0x118` and a valid active `UI_selfParkRequest` session on `UI_driverAssistControl` (`0x3F8`), with gear in P, R, or D.

The policy also requires a fresh non-active DAS AP state from `0x399` on Legacy/HW3 or `0x39B` on HW4. Fresh `DIF/DIR_gear` from `0x186`, when available, must agree with `DI_gear`. Missing, stale (over 500 ms), SNA, invalid, contradictory, manually driven, AP-active, or unexpectedly moving state blocks injection. A block clears pending periodic plugin emission and pending ESP-IDF TWAI or MCP2515 hardware transmissions.

Signal behavior can differ by vehicle and Tesla software version. This beta gate is conservative and does not establish compatibility with any specific release. Start with an isolated bench or stationary vehicle, keep an independent disconnect available, and inspect Support diagnostics before enabling a plugin.

## Plugins

Use the Plugins card to install a JSON file by URL, file upload, or paste. New plugins start disabled. Review the JSON, confirm the target bus and frame layout, and test on an isolated bench before enabling transmission. See the [Plugin system reference](plugins.md).

## Support diagnostics

Support is the collapsed final card. Expanding it creates one on-demand report; it does not poll in the background. Use **Refresh report** for a new snapshot and **Copy report** when preparing a bug report.

The report includes:

- firmware/build, chip, flash, reset reason, uptime, and boot count;
- free/minimum heap, largest free block, RAM/PSRAM totals, and task stack watermarks;
- task heartbeats, wakeups, logging throttles, and HTTP request/response sizes;
- sanitized WiFi identity and signal information;
- mode, non-secret configuration, CAN driver state, errors, recovery, filters, and queue pressure;
- AP active/stability/park/summon gate state and reason, other injection gates, Last Write Check, safety bounds, NVS recovery, and GVRET state.

It intentionally excludes passwords, OTA credentials, tokens, keys, complete plugin payloads, and private captures. If a report is too large for its bounded buffer, the endpoint fails instead of returning a misleading partial report.

## SavvyCAN USB serial

1. Open **SavvyCAN USB serial** and select **Arm GVRET**.
2. Connect the board's USB port.
3. In SavvyCAN choose **Connection → Add Connection → Serial Connection (GVRET)**.
4. Select the board serial port at `115200` baud.

GVRET exposes one read-only 500 kbit/s bus using reference framing. It is intended for observation. It does not advertise CAN transmit commands. Disconnect, stop the session, or wait for the timeout to release serial ownership.

## WiFi and updates

The board starts as an access point. Optional WiFi Internet enables release checks, plugin downloads, and OTA. Use a firmware `.bin` made for the exact board. Keep a serial recovery method because an interrupted update can leave the board unavailable until reflashed.

## Source for contributors

Edit [`include/web/mcp2515_dashboard_ui.src.h`](../include/web/mcp2515_dashboard_ui.src.h). [`include/web/mcp2515_dashboard_ui.h`](../include/web/mcp2515_dashboard_ui.h) is generated raw/gzip data and should not be edited directly.
