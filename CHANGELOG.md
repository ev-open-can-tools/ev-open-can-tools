# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Added a lightweight first-visit dashboard onboarding wizard for vehicle, safety gate, WiFi, and CAN pin setup. It reuses existing endpoints, can be reopened at `#/onboarding`, and never arms CAN injection automatically.
- Added a GitHub Pages onboarding preview generated from the embedded dashboard source, using simulated device APIs so the complete setup flow can be tested safely without an ESP32.

## [3.0.2-beta.6] - 2026-07-13

### Added

- Reworked the documentation for beginners: project overview, board selection, first boot, dashboard use, plugin safety, CAN evidence, troubleshooting, and runtime measurements now follow one guided path.
- Added clear explanations of CAN IDs, frames, DLC, muxes, counters, checksums, TWAI, MCP2515, GVRET, and Support diagnostics.
- Restored Support as collapsed final dashboard section backed by one on-demand, bounded plain-text report. It includes firmware/build/chip/flash/reset, heap and task stack watermarks, WiFi identity/signal, CAN/TWAI health and queue pressure, filters/gates/safety limits, Last Write Check, NVS recovery, GVRET, and web metrics without credentials or secrets.
- Added lightweight request/response, support size, task wake, heap low-water, logging throttle, and TWAI maximum queue-depth measurements plus ESP32 optimization guidance.

### Changed

- Replaced frequently polled `/status` `String` construction with fixed-buffer formatting, removed periodic plugin polling, reduced idle WiFi/status requests, and polls GVRET only while active.
- Reduced idle web-maintenance and disabled-GVRET task wakeups from 100 Hz to 4 Hz without changing CAN timing, task priorities, injection behavior, or safety gates.


## [3.0.2-beta.5] - 2026-07-11

### Added

- ESP-IDF startup/runtime diagnostics: reset reason, RTC boot count, brownout warning, IDF version, five-second loop/CAN/web heartbeats, CAN age, TX results, free heap, uptime, and detailed TWAI state/error/recovery status.
- Read-only SavvyCAN USB serial logging through reference GVRET framing, with explicit dashboard arming, clean binary-log ownership, one 500 kbit/s bus, validated standard frames, idle/disconnect handling, and bounded nonblocking sessions.
- Nag-suppression guide covering Party CAN placement, startup gates, torque limits, target IDs, SavvyCAN observation, and Tesla electrical-reference lookup.

### Changed

- Dashboard now starts before TWAI, TWAI waits 10 seconds before initialization, and injection waits 15 seconds from actual CAN initialization plus more than 1,000 valid received frames.
- Dashboard diagnostics are reduced to runtime status and Last Write Check. Sniffer, recorder, controller/mux views, live log, rule-test/editor diagnostics, obsolete endpoints, and backing state were removed while configuration, plugins, connectivity, safety, backup, and updates remain.
- Dashboard configuration and statistics now use separate responses, with retry/error handling, request overlap guards, null-safe rendering, and disabled controls until configuration loads.
- Dashboard source header is authoritative; generated raw/gzip header now comes from deterministic standard-library generation during dashboard builds.

### Fixed

- Missing CAN traffic no longer causes restart; firmware/dashboard stay alive and emit throttled warnings. TWAI bus-off now uses ESP-IDF recovery API, and TX failure logs are throttled while preserving two-millisecond transmit wait.
- Dashboard TWAI states and colors now map stopped/running/bus-off/recovering correctly.
- Nag echo handling validates DLC, detects self frames using full 12-bit torque, enforces raw and `-1.80 Nm` to `+1.80 Nm` hard bounds, resets injection sequence state on live mode changes, and counts only successful transmissions.
- NVS recovery erases storage only for no-free-pages or new-version initialization errors.
- ESP-IDF image headers now follow each board's declared 4 MB or 8 MB flash size; generic `esp32_twai` also uses project 4 MB OTA partition layout, matching current firmware size.

## [3.0.2-beta.4] - 2026-07-11

### Added

- ESP32-C6 support via the new `esp32c6_twai` PlatformIO environment (ESP-IDF, `DRIVER_TWAI` on GPIO5/GPIO4, `partitions_4mb_ota_1536k.csv`). Added to the Tests CI matrix and the Release workflow so automation builds and OTA release assets include the C6 board.
- Board-specific OTA artifact selection for every supported ESP-IDF environment.
- Verified HTTPS downloads using the ESP-IDF certificate bundle and SNTP-backed certificate-time validation.

### Fixed

- AP Injection Gate now keeps standard DAS `ACTIVE` state 6 engaged while rejecting `AVAILABLE` state 2 and abort/fault states 8/9.
- HW4 AP state now comes from CAN `0x39B` byte 1 instead of misreading the HW4 `0x399` ISA frame, with the confirmed 2026.20 Highland byte-0 fallback after a three-frame latch.
- Legacy activation stability timing now works when AP becomes active at the `millis()` zero epoch.
- ESP-IDF HTTP response streaming, query-route matching, URL decoding, response status codes, bounded request handling, and authenticated multipart firmware uploads now work correctly without buffering firmware in RAM.
- GitHub OTA now selects only the current board artifact, rejects artifact substitutions and HTTPS downgrades, supports chunked transfers, reports install failures accurately, validates the completed image, and serializes concurrent update attempts.
- Settings backup no longer exposes WiFi credentials without authentication, now exports live AP and multi-network state, and preserves dashboard, CAN, plugin, update, and HW3 settings during restore.
- Static-IP configuration is validated and applied through ESP-NETIF, with DHCP fallback on runtime failure; malformed SSIDs, indexes, pins, and configuration values now fail closed instead of truncating or coercing.
- TWAI and external MCP2515 drivers now reject invalid, extended, and RTR frames, preserve all handler/plugin acceptance IDs across mode changes, recover after initialization and bus faults, and synchronize diagnostics, filtering, receive, transmit, and recovery operations.
- ESP-IDF now uses 1 kHz FreeRTOS ticks and a true scheduler yield, preserving millisecond CAN timeouts and preventing the main loop from sleeping 10 ms on every pass.
- The native MCP2515 SPI implementation now propagates bus/device/transaction errors, bounds register transfers and DLC values, clears invalid receive buffers, and handles timer rollover safely.
- Plugin JSON parsing now enforces schema types, numeric ranges, bus tokens, CAN IDs, frame DLC, operation limits, periodic-emission scope, and counter masks instead of wrapping or silently ignoring malformed rules.
- Plugin execution, diagnostics, tests, persistence, priority changes, and periodic/UDS state are synchronized across the CAN, HTTP, and maintenance tasks; counters advance only after successful sends.
- Plugin updates now use a reset-recoverable temp/backup swap compatible with SPIFFS rename semantics, and interrupted saves are repaired on boot.
- Dashboard logs, recorder state, WiFi state, preferences, handler publication, LED output, OTA state, and plugin-test state no longer race across FreeRTOS tasks or report failed writes as successful.
- NVS writes are committed once per preference session, write and erase errors propagate to callers, invalid stored values fall back safely, and multi-key changes restore runtime state when persistence fails.

## [3.0.2-beta.3] - 2026-06-12

### Fixed

- Legacy FSD activation injection on CAN `0x3EE` mux 0 now waits until Autopilot has remained active for 2 seconds when the AP Injection Gate is enabled, preventing false parked gate states and activation timing races from injecting during initial engagement.
## [3.0.2-beta.2] - 2026-05-28

### Fixed

- Dashboard AP Injection Gate now waits for AP to remain stable for one second before allowing plugin injection through the AP path, reducing activation-edge injection during FSD/AP engagement transients while keeping Park and Summon injection behavior unchanged.

## [3.0.2-beta.1] - 2026-05-26

### Fixed

- AP Injection Gate now treats live INVALID/SNA gear values as unknown instead of Park, preventing Start After AP plugin injection while AP is inactive unless the car is definitively in Park or Summoning.

## [3.0.1] - 2026-05-25

### Fixed

- Republished the stable 3.0 release through the normal main-branch release workflow so firmware assets can be attached before the release is published immutable.

## [3.0.0] - 2026-05-21

Stable release bundling the `3.0.0-beta.1` through `3.0.0-beta.9` changes.

### Added

- ESP-IDF native build path for supported targets, including ESP-IDF runtime shims, IDF RGB LED support, dashboard gzip serving, and multi-SSID WiFi.
- Status LED brightness control, AP Gate Diagnostics, CAN driver diagnostics, HW4 Offset Slew support, and plugin rule diagnostics in the dashboard and support reports.
- Plugin rule byte-mask matching and the example `0x370` duplicate-counter plugin.
- Manual firmware upload control to clear saved OTA credentials from browser local storage.

### Changed

- ESP-IDF builds are pinned through PlatformIO `platformio/espressif32` 7.0.0, with legacy Arduino boards moved under `legacy-arduino`.
- Release and CI workflows cover supported ESP-IDF and legacy Arduino targets.
- Dashboard assets are minified and served as gzip-compressed content to reduce flash usage.

### Fixed

- Dashboard CAN recorder CSV rows now use real comma separators.
- ESP-IDF dashboard serving avoids large `String` copies and httpd stack overflows.
- WiFi scan/save flows are more reliable in AP+STA mode.
- Release workflows handle legacy Arduino board builds from `legacy-arduino`.
- Plugin `or_byte` and `and_byte` operations apply their configured values correctly.
- Dashboard Support button placement is generated from the footer source HTML.

## [3.0.0-beta.9] - 2026-05-19

### Added

- Plugin rule diagnostics now report match, change, and transmit counters plus last original/modified frames in `/plugins` and support reports.

### Fixed

- Dashboard CAN recorder CSV rows now use real comma separators.

## [3.0.0-beta.8] - 2026-05-18

### Added

- Added AP Gate Diagnostics to /status and the Support report to debug false Active injection states.

## [3.0.0-beta.7] - 2026-05-18

### Added

- Add HW4 support for the web-native Offset Slew limiter while preserving existing HW3-compatible settings keys.

## [3.0.0-beta.6] - 2026-05-18

### Added

- CAN driver diagnostics are now exposed through the driver interface and logged at startup.
- TWAI builds now report TX/RX pins, driver state, queue counters, bus errors, recovery count, rejected frame count, and last ESP-IDF CAN errors.

### Fixed

- Added a regression test proving `or_byte` and `and_byte` plugin operations apply their configured values correctly.
- Moved the dashboard Support button from the Configuration card into the footer source HTML.

## [3.0.0-beta.5] - 2026-05-04

### Added

- RGB status LED now reflects runtime state at a glance: solid green = injecting + at least one client connected, blinking green = injecting with no client connected, solid red = injection stopped + connected, blinking red = injection stopped + no client. Solid blue indicates an in-progress OTA firmware update. "Connected" is true when any station is associated with the AP or when the STA uplink is up.

## [3.0.0-beta.4] - 2026-05-04

### Added

- Manual firmware upload now has a dashboard button to clear saved OTA username/password credentials from browser local storage.

## [3.0.0-beta.3] - 2026-05-04

### Added

- Plugin rules can now include an additional byte-mask match (`match_byte`, `match_mask`, `match_val`) so plugins can target specific bit states without adding dedicated firmware toggles.
- Added an example `0x370` duplicate-counter plugin that matches byte 4 bits 7:6 clear, forces byte 3 to `0xB6`, sets byte 4 bit 6, increments byte 6 low-nibble counter, and recomputes byte 7 checksum.

### Fixed

- ESP-IDF WiFi Internet saves now defer reconnect until after the HTTP response, preventing the dashboard request from being dropped while AP+STA mode changes channels.
- Switching saved WiFi networks now disconnects any existing STA association before connecting to the new SSID, avoiding repeated `sta is connected` / deauth log spam.
- WiFi scan now prepares AP+STA mode before scanning, so scans work when the device is otherwise in AP-only mode.
- ESP-IDF WiFi/httpd/netif component logs are reduced to warning/error levels to keep dashboard serial logs readable.

## [3.0.0-beta.2] - 2026-05-04

### Changed

- Pin ESP-IDF builds to ESP-IDF v6.0.1 through PlatformIO `platformio/espressif32` 7.0.0 and keep legacy Arduino boards in `legacy-arduino`.
- Build all supported ESP-IDF and legacy Arduino targets in GitHub Actions, including release artifacts for AtomS3 Mini CAN Base and Waveshare ESP32-S3 RS485/CAN.

### Fixed

- Release and test workflows now run legacy Arduino board builds from `legacy-arduino`, so RP2040 and Feather M4 CI no longer look for removed root Arduino environments.
- CI installs the Python package needed by ESP-IDF 6.0.1 tooling and skips clang-format on the generated dashboard payload header.

## [3.0.0-beta.1] - 2026-05-03

### Added

- ESP-IDF 5.5 native build path replacing the Arduino framework on supported targets (M5Stack AtomS3 Lite verified). Adds `src/espidf_runtime.cpp` plus `include/platform/espidf_runtime.h` providing Arduino-compatible shims (`String`, `WiFi`, `WebServer`, `Preferences`, `Update`, `HTTPClient`, `SPIFFS`, etc.) on top of ESP-IDF APIs.
- ESP-IDF `led_strip` (RMT) implementation of the on-board RGB status LED so the AtomS3 Lite indicator works under IDF (green = injecting, red = idle).
- "Status LED Brightness" subsection in the Configuration card with a slider and number input (0–255), persisted in NVS (`led_b`) and applied live via `/led_brightness`.
- Multi-SSID WiFi support — up to 4 saved networks. The device tries each in turn until one connects (e.g. home + phone hotspot). New endpoints `/wifi_networks`, `/wifi_delete`; `/wifi_config` accepts an `idx` argument to update a specific slot. Legacy single-SSID NVS keys auto-migrate to slot 0 on first boot.
- `scripts/minify_dashboard.py` — minifies the dashboard HTML/CSS/JS (csscompressor + terser + htmlmin) and emits a gzipped `DASH_HTML_GZ[]` payload served with `Content-Encoding: gzip`. Dashboard source-of-truth moved to `include/web/mcp2515_dashboard_ui.src.h`.

### Changed

- Flash usage on `m5stack-atoms3-mini-can-base` reduced from 79.0 % (1,243,217 B) to 77.4 % (1,218,733 B) by serving compressed dashboard HTML.

### Fixed

- ESP-IDF dashboard route registration now rejects duplicate registrations cleanly and serves compressed UI without stack-heavy copies.
