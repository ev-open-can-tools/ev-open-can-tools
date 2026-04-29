# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.5.2] - 2026-04-29

Stable release bundling the AP Injection Gate Smart Summon fixes from `2.5.2-beta.1` through `2.5.2-beta.6`.

### Fixed

- AP Injection Gate no longer drops during Smart Summon launches that report `DI_autonomyControlActive` while still in Park before an immediate turn. Definite Park frames now clear the summon latch only when ACA is inactive, so the earlier non-zero `UI_selfParkRequest` survives the shift out of Park even when no fresh request frame appears after the shift.

## [2.5.2-beta.6] - 2026-04-29

### Fixed

- AP Injection Gate no longer drops during Smart Summon launches that report `DI_autonomyControlActive` while still in Park before an immediate turn. Definite Park frames now clear the summon latch only when ACA is inactive, so the earlier non-zero `UI_selfParkRequest` survives the shift out of Park even when no fresh request frame appears after the shift.

## [2.5.2-beta.5] - 2026-04-28

### Fixed

- AP Injection Gate no longer stays open for 3-5 s after Autopilot is disengaged, and no longer treats plain TACC as an open-gate condition. `DI_autonomyControlActive` (ACA) on CAN 280 is set during AP, TACC, *and* Smart Summon, so it cannot drive Summoning by itself. Summoning now requires both `ACA=1` *and* a `UI_selfParkRequest` non-zero command observed in the current autonomy episode. ACA falling edge resets the spr-seen flag, so a subsequent TACC engagement does not re-latch the gate. The 5 s ACA-only timeout is removed: AP disengage drops ACA immediately and the gate closes immediately, restoring instant AP / TACC re-engagement.

## [2.5.2-beta.4] - 2026-04-28

### Fixed

- AP Injection Gate now opens on a freshly-booted module when the car is asleep / locked with Sentry. While the DI is asleep, CAN ID 280 (`DI_systemStatus`) is not broadcast at all (only DAS / autopilot ECU IDs 921/1016/1021/2047 keep transmitting), so the previous boot-time `Parked=false` default left the gate stuck at `Waiting AP` until the driver pressed the brake to wake the DI. `Parked` now defaults to `true` at boot; the first `DI_systemStatus` frame with a driving gear (R/N/D) flips it to false. If the DI never reports, the car is asleep / parked and the gate remains open by design.

## [2.5.2-beta.3] - 2026-04-28

### Fixed

- AP Injection Gate now stays open for the full duration of a Smart Summon / Smart Park session. Detection now uses `DI_autonomyControlActive` (CAN ID 280, bit 50 / byte 6 bit 2) as the primary "summon active" signal in addition to `UI_selfParkRequest` on CAN 1016. The DI bit is held high for the entire time the car is being driven by an autonomy stack, so the 5 s spr-only timeout no longer expires mid-summon when the UI command pulse drops to 0 while the car keeps driving itself.

## [2.5.2-beta.2] - 2026-04-28

### Fixed

- AP Injection Gate no longer drops while Summon shifts the vehicle to Reverse. `clearSummonOnPark()` now fires only on a definitive `DI_gear == 1 (P)` value, not on the permissive `isVehicleParked` set that also includes `0=INVALID` and `7=SNA`. SNA can blip during gear transitions (e.g. P->R under Summon control), and the previous logic would clear `Summoning` on that blip and close the gate mid-summon. Shift to Drive was unaffected because `selfParkRequest` stayed non-zero long enough to re-latch `Summoning`; Reverse was reaching gear faster than the latch could recover.

## [2.5.2-beta.1] - 2026-04-28

### Fixed

- AP Injection Gate now opens while the car is asleep / locked with Sentry. `isVehicleParked` now treats `DI_gear` values `0=INVALID` and `7=SNA` as parked in addition to `1=P`. When the DI is asleep it reports SNA on CAN ID 280, which previously left `Parked=false` and kept the gate stuck at `Waiting AP` until the driver pressed the brake to wake the DI. Driving states (R=2, N=3, D=4) are still not parked, so the gate behavior on a moving car is unchanged.

## [2.5.1] - 2026-04-28

Stable release bundling all changes from 2.4.2 onwards (`2.5.0-beta.5` through `2.5.0-beta.12`). Notably, the `Start after AP` dashboard toggle now gates plugin injection on AP, Park, *and* Summon / Smart Park, which makes the firmware compatible with vehicles running Tesla software release **2026.14.3** and newer — these versions reject the always-on injection used by earlier dashboards, so the toggle must be enabled to keep injection working on those vehicles.

### Added

- Dashboard speed profiles now include an `Auto` mode. Auto follows the vehicle follow-distance selection, while a manual dashboard profile stays locked and is injected instead of being overwritten by the car.

### Fixed

- Legacy speed profile selection is now written back into outgoing CAN ID `1006` mux `0` frames so the selected profile actually takes effect on vehicle behavior instead of only changing the observed internal state. (rolled up from 2.4.2-beta.1)
- AP Injection Gate now detects active AP from recorded HW3 `1021` mux `0` frames by reading the observed AD bit, so plugin injection no longer stays stuck at `Waiting AP` after Autopilot is engaged.
- AP Injection Gate now waits for DAS `AutopilotStatus` active states instead of the 1021 UI/config bit, preventing plugin injection from switching to Active when AP is not engaged.
- OTA update finalize now passes `true` to `Update.end()` to force completion regardless of residual byte count, fixing the `Update finalize failed` error that could occur after a successful download via GitHub S3 redirects.
- OTA error paths now log `Update.errorString()` at every failure point (begin, write, finalize) so the root cause is visible in the dashboard log instead of a generic message.
- AP Injection Gate now also opens while the vehicle is in Park, so Summon unlock injection can run while parked and stops again after shifting to Drive.
- Dashboard manual profile selection now persists in firmware state and is applied to Legacy, HW3, and HW4 injection paths.
- AP Injection Gate park detection now also reads `DI_systemStatus` (CAN ID 280) `DI_gear`, so the gate reopens when the vehicle is shifted to Park on Chassis-bus connections that do not carry `DIF_torque`/`DIR_torque` (CAN ID 390). MCP2515 hardware filter slots updated for Legacy, HW3, and HW4 modes to admit ID 280.
- AP Injection Gate now also stays open while Summon / Smart Park is active. HW3 and HW4 handlers parse `UI_driverAssistControl` (CAN ID 1016) `UI_selfParkRequest` (byte 3 bits 4-7); when the request is non-zero (4=PRIME, 5=PAUSE, 7/8=AUTO_SUMMON_FWD/REV, 11=SMART_SUMMON), `Summoning` is asserted and held for 5 s after the last activity, so injection keeps running once the vehicle shifts out of Park into Drive/Reverse under Summon control.
- AP Injection Gate `Summoning` flag is force-cleared whenever the vehicle returns to Park, so a manual P->D shift after a completed summon correctly waits for AP again instead of latching the gate open until reboot.

## [2.5.0-beta.12] - 2026-04-27

### Fixed

- AP Injection Gate Summon detection no longer latches `Summoning` permanently after the first summon use. `UI_summonHeartbeat` is no longer used for activity tracking because it keeps cycling 0..3 indefinitely once summon has run, which prevented the gate from ever closing again. Detection now relies solely on `UI_selfParkRequest` (CAN 1016, byte 3 bits 4-7); the flag holds for 5 s after the last non-zero command and is also force-cleared whenever the vehicle returns to Park, so a manual P->D shift after a completed summon correctly waits for AP again.

## [2.5.0-beta.11] - 2026-04-27

### Fixed

- AP Injection Gate now also stays open while Summon / Smart Park is active. HW3 and HW4 handlers parse `UI_driverAssistControl` (CAN ID 1016) `UI_summonHeartbeat` (byte 0 bits 2-3) and `UI_selfParkRequest` (byte 3 bits 4-7); when either is non-zero, `Summoning` is asserted and held for 1500 ms after the last activity, so injection keeps running once the vehicle shifts out of Park into Drive/Reverse under Summon control.

## [2.5.0-beta.10] - 2026-04-27

### Fixed

- AP Injection Gate park detection now also reads `DI_systemStatus` (CAN ID 280) `DI_gear`, so the "Start after AP" gate reopens when the vehicle is shifted to Park on Chassis-bus connections that do not carry `DIF_torque`/`DIR_torque` (CAN ID 390). MCP2515 hardware filter slots updated for Legacy, HW3, and HW4 modes to admit ID 280.

## [2.5.0-beta.9] - 2026-04-27

### Added

- Dashboard speed profiles now include an `Auto` mode. Auto follows the vehicle follow-distance selection, while a manual dashboard profile stays locked and is injected instead of being overwritten by the car.

### Fixed

- Dashboard manual profile selection now persists in firmware state and is applied to Legacy, HW3, and HW4 injection paths.

## [2.5.0-beta.8] - 2026-04-27

### Fixed

- AP Injection Gate now also opens while the vehicle is in Park, so Summon unlock injection can run while parked and stops again after shifting to Drive.

## [2.5.0-beta.7] - 2026-04-25

### Fixed

- OTA update finalize now passes `true` to `Update.end()` to force completion regardless of residual byte count, fixing the `Update finalize failed` error that could occur after a successful download via GitHub S3 redirects.
- OTA error paths now log `Update.errorString()` at every failure point (begin, write, finalize) so the root cause is visible in the dashboard log instead of a generic message.

## [2.5.0-beta.6] - 2026-04-24

### Fixed

- AP Injection Gate now waits for DAS `AutopilotStatus` active states instead of the 1021 UI/config bit, preventing plugin injection from switching to Active when AP is not engaged.

## [2.5.0-beta.5] - 2026-04-24

### Fixed

- AP Injection Gate now detects active AP from recorded HW3 `1021` mux `0` frames by reading the observed AD bit, so plugin injection no longer stays stuck at `Waiting AP` after Autopilot is engaged.

## [2.5.0] - 2026-04-24

First stable release of the 2.5 series. Bundles all changes from 2.5.0-beta.1 through 2.5.0-beta.4.

### Added

- Dashboard configuration now includes a GTW 2047 plugin replay control, and settings backup/import now preserves plugin replay preferences.
- Dashboard configuration now includes an AP Injection Gate toggle that arms plugin injection but waits until AP/NoA is observed active before sending plugin frames.
- Plugin rules now support `counter` fields and `emit_periodic` for cached GTW mux 3 broadcasts, including editor support and updated plugin documentation.
- Plugin rules now support a `bus` field (`CH`, `VEH`, `PARTY`, comma-separated string, bitmask, or array) to restrict matching to specific CAN bus pins; frames with unknown bus still match for backwards compatibility.
- Plugin rules now support a `mux_mask` field (alias `muxMask`) to control which bits of byte 0 are compared for mux matching; values 0-7 default to low-3-bit mask, values 8-255 default to full-byte mask, enabling low-nibble and full-byte DBC mux styles.
- Plugin list API now includes `bus` and `mux_mask` per rule so the dashboard UI and support exports reflect full rule configuration.
- Plugin editor gained bus and mux-mask input fields per rule, and the rule label, summary, and conflict panel now show bus pin and mux/mask.
- GTW periodic emit can now optionally try to silence native gateway broadcasts through a UDS diagnostic sequence using extended session, SecurityAccess, `CommunicationControl`, and `TesterPresent`.
- HW3 dashboard builds now expose an optional offset slew limiter for plugin-driven mux 2 offset changes.
- Added the shared `INJECTION_AFTER_AP` build option for behaviour-option builds; `ENHANCED_AUTOPILOT` mux 1 injection now waits for AP to be active when this option is enabled.
- Added a WiFi dashboard regression test that covers the WiFi settings UI, backend routes, status payload, and backup fields.
- Added native PlatformIO test environments `native_plugin_engine` and `native_plugin_engine_custom_key` for running plugin engine unit tests without hardware.

### Changed

- Plugin rules now allow up to 16 operations per rule instead of 8.
- `PLUGIN_FILTER_IDS_MAX` raised to 32 (was equal to `PLUGIN_RULES_MAX` = 16) and made overridable at build time.
- Replayed GTW frames, periodic emits, and repeated Rule Test sends now advance counter fields and refresh checksums between sends.
- Dashboard plugin details, validation, support exports, and docs now describe the new replay, counter, and periodic emit behavior.
- `gtw_silent: true` is now silently treated as disabled at parse time unless `PLUGIN_GTW_UDS_CUSTOM_KEY` is defined at build time; the periodic emit still works but no UDS sequence is started and `0x684`/`0x685` filter IDs are not injected.
- UDS request frames sent by the GTW silencing state machine are now tagged with the bus of the frame that seeded the periodic emit cache.
- Incoming frames with `CAN_BUS_ANY` are now normalized to `CAN_BUS_DEFAULT` in the main app loop before being passed to the plugin engine.
- Plugin rule mux matching and test-rule matching refactored into shared `pluginRuleMatchesBus` / `pluginRuleMatchesMux` helpers used by both the engine and the dashboard.
- Plugin editor mux input range extended to -1-255 (was -1-7) to support full-byte DBC mux values.

### Fixed

- GTW silent-mode plugin rules now add the required UDS request/response CAN IDs to the active filter set so the diagnostic state machine can observe replies.
- The CAN analyzer now labels UDS `0x28 CommunicationControl` requests by name.
- WiFi Internet status now follows the live STA connection state reliably after connect attempts and page refreshes instead of getting stuck on `Connecting to ...` or `Not configured`.
- Corrupted WiFi SSID fragments are now ignored in saved settings and filtered from `/wifi_status` responses.
- The WiFi settings form no longer overwrites the SSID field while it is being edited, and the status header no longer depends on optional labels being present.
- Dashboard CAN sniffer and recorder buffers now clamp incoming frame DLC before copying frame data.
- Plugin mux matching now ignores zero-DLC frames instead of treating them as mux 0.
- Plugin conflict detection and detail panel now correctly account for bus mask and mux mask when determining whether two rules can affect the same frame.
- Custom-key plugin engine native tests now validate output against the configured `PLUGIN_GTW_UDS_KEY_READY` value instead of a hard-coded key byte.

## [2.5.0-beta.4] - 2026-04-24

### Added

- Dashboard configuration now includes an AP Injection Gate toggle that arms plugin injection but waits until AP/NoA is observed active before sending plugin frames.
- Added the shared `INJECTION_AFTER_AP` build option for behaviour-option builds; `ENHANCED_AUTOPILOT` mux 1 injection now waits for AP to be active when this option is enabled.

### Fixed

- Custom-key plugin engine native tests now validate output against the configured `PLUGIN_GTW_UDS_KEY_READY` value instead of a hard-coded key byte.

## [2.5.0-beta.3] - 2026-04-24

### Added

- Plugin rules now support a `bus` field (`CH`, `VEH`, `PARTY`, comma-separated string, bitmask, or array) to restrict matching to specific CAN bus pins; frames with unknown bus still match for backwards compatibility.
- Plugin rules now support a `mux_mask` field (alias `muxMask`) to control which bits of byte 0 are compared for mux matching; values 0–7 default to low-3-bit mask, values 8–255 default to full-byte mask, enabling low-nibble and full-byte DBC mux styles.
- Plugin list API now includes `bus` and `mux_mask` per rule so the dashboard UI and support exports reflect full rule configuration.
- Plugin editor gained bus and mux-mask input fields per rule, and the rule label, summary, and conflict panel now show bus pin and mux/mask.
- `PLUGIN_FILTER_IDS_MAX` raised to 32 (was equal to `PLUGIN_RULES_MAX` = 16) and made overridable at build time.
- Added native PlatformIO test environments `native_plugin_engine` and `native_plugin_engine_custom_key` for running plugin engine unit tests without hardware.

### Changed

- `gtw_silent: true` is now silently treated as disabled at parse time unless `PLUGIN_GTW_UDS_CUSTOM_KEY` is defined at build time; the periodic emit still works but no UDS sequence is started and `0x684`/`0x685` filter IDs are not injected.
- UDS request frames sent by the GTW silencing state machine are now tagged with the bus of the frame that seeded the periodic emit cache.
- Incoming frames with `CAN_BUS_ANY` are now normalized to `CAN_BUS_DEFAULT` in the main app loop before being passed to the plugin engine.
- Plugin rule mux matching and test-rule matching refactored into shared `pluginRuleMatchesBus` / `pluginRuleMatchesMux` helpers used by both the engine and the dashboard.
- Plugin editor mux input range extended to −1–255 (was −1–7) to support full-byte DBC mux values.

### Fixed

- Plugin conflict detection and detail panel now correctly account for bus mask and mux mask when determining whether two rules can affect the same frame.

## [2.5.0-beta.2] - 2026-04-24

### Fixed
- Dashboard CAN sniffer and recorder buffers now clamp incoming frame DLC before copying frame data.
- Plugin mux matching now ignores zero-DLC frames instead of treating them as mux 0.

## [2.5.0-beta.1] - 2026-04-23

### Added
- Dashboard configuration now includes a GTW 2047 plugin replay control, and settings backup/import now preserves plugin replay preferences.
- Plugin rules now support `counter` fields and `emit_periodic` for cached GTW mux 3 broadcasts, including editor support and updated plugin documentation.
- GTW periodic emit can now optionally try to silence native gateway broadcasts through a UDS diagnostic sequence using extended session, SecurityAccess, `CommunicationControl`, and `TesterPresent`.
- HW3 dashboard builds now expose an optional offset slew limiter for plugin-driven mux 2 offset changes.
- Added a WiFi dashboard regression test that covers the WiFi settings UI, backend routes, status payload, and backup fields.

### Changed
- Plugin rules now allow up to 16 operations per rule instead of 8.
- Replayed GTW frames, periodic emits, and repeated Rule Test sends now advance counter fields and refresh checksums between sends.
- Dashboard plugin details, validation, support exports, and docs now describe the new replay, counter, and periodic emit behavior.

### Fixed
- GTW silent-mode plugin rules now add the required UDS request/response CAN IDs to the active filter set so the diagnostic state machine can observe replies.
- The CAN analyzer now labels UDS `0x28 CommunicationControl` requests by name.
- WiFi Internet status now follows the live STA connection state reliably after connect attempts and page refreshes instead of getting stuck on `Connecting to ...` or `Not configured`.
- Corrupted WiFi SSID fragments are now ignored in saved settings and filtered from `/wifi_status` responses.
- The WiFi settings form no longer overwrites the SSID field while it is being edited, and the status header no longer depends on optional labels being present.

## [2.4.2-beta.1] - 2026-04-23

### Fixed
- Legacy speed profile selection is now written back into outgoing CAN ID `1006` mux `0` frames so the selected profile actually takes effect on vehicle behavior instead of only changing the observed internal state.
- Added a native Legacy regression test that verifies the selected speed profile bits are injected into the transmitted mux `0` frame.

## [2.4.1] - 2026-04-22

### Added
- Dashboard header now shows the latest observed `GTW_autopilot` state next to the selected hardware mode.
- Native dashboard regression tests now assert that dashboard handlers do not send frames, increment `framesSent`, or fire `onSend`.
- README and dashboard footer now include the support and gift information from `main`.

### Changed
- Dashboard builds no longer compile automatic CAN injection paths from Legacy, HW3, or HW4 handlers; enabled plugins are the automatic injection path.
- HW3 no longer listens for or injects Track Mode request frames.
- ESP32 dashboard builds now use a 4MB OTA partition layout with larger app slots while preserving SPIFFS storage.
- Plugin documentation now describes the dashboard's plugin-only automatic injection behavior instead of firmware overlap warnings.
- Rule Test count and interval controls now use visible labels with `1` and `100` as placeholders, while empty fields still default to one injection every 100 ms.
- Support issue flow now copies the dashboard support report to the clipboard and opens the General Issue form without prefilled title or body text.

### Fixed
- ESP32 dashboard hotspot startup is more reliable by starting the AP earlier, disabling WiFi sleep, validating saved AP/STA credentials, and falling back to AP-only mode when STA connection attempts time out.

## [2.4.1-beta.3] - 2026-04-22

### Fixed
- ESP32 dashboard hotspot startup is more reliable by starting the AP earlier, disabling WiFi sleep, validating saved AP/STA credentials, and falling back to AP-only mode when STA connection attempts time out.

## [2.4.1-beta.2] - 2026-04-20

### Changed
- Rule Test count and interval controls now use visible labels with `1` and `100` as placeholders, while empty fields still default to one injection every 100 ms.
- Support issue flow now copies the dashboard support report to the clipboard and opens the General Issue form without prefilled title or body text.

## [2.4.1-beta.1] - 2026-04-20

### Added
- Dashboard header now shows the latest observed `GTW_autopilot` state next to the selected hardware mode.
- Native dashboard regression tests now assert that dashboard handlers do not send frames, increment `framesSent`, or fire `onSend`.

### Changed
- Dashboard builds no longer compile automatic CAN injection paths from Legacy, HW3, or HW4 handlers; enabled plugins are the automatic injection path.
- HW3 no longer listens for or injects Track Mode request frames.
- ESP32 dashboard builds now use a 4MB OTA partition layout with larger app slots while preserving SPIFFS storage.
- Plugin documentation now describes the dashboard's plugin-only automatic injection behavior instead of firmware overlap warnings.

## [2.4.0] - 2026-04-19

## [2.4.0-beta.1] - 2026-04-19

### Added
- Dashboard plugin priority controls now let users choose which enabled plugin wins overlapping bit writes
- Dashboard plugin conflict warnings now show firmware overlaps, plugin priority overlaps, and the first enabled injection priority

### Changed
- Plugin installs now start disabled so users can review priority and conflicts before enabling them
- Rule Test now waits for the next matching live CAN frame, applies the selected rule to that frame, and injects the captured result with the chosen count and interval
- Dashboard polling now keeps `/status` as the connection gate and backs off non-critical polls during reconnects

### Fixed
- Plugin rules targeting the same CAN ID and mux are merged into one injected frame per incoming frame, preventing contradictory duplicate plugin sends in the same cycle
- TWAI dashboard filtering now drops sparse-mask false positives in software, reducing dashboard load when plugin CAN IDs widen the hardware mask
- Dashboard status rendering no longer breaks when optional status-grid elements are removed or rearranged
- Dashboard remains responsive under heavier CAN/plugin load by limiting per-loop frame draining and giving the web task more scheduling priority

## [2.3.2-beta.2] - 2026-04-18

### Changed
- Dashboard no longer exposes or persists speed profile control; follow distance and the derived profile remain visible as read-only status
- Dashboard profile-related boot logging and legacy NVS cleanup now reflect the new plugin-managed model without stale `SP` or profile-lock state

### Fixed
- Dashboard core no longer injects speed profile or speed offset back onto CAN for Legacy, HW3, or HW4 handlers; those values are now observational unless a plugin explicitly modifies those frames
- Dashboard plugin toggles now apply immediately and batch their persistence, avoiding repeated Wi-Fi stalls while enabling or disabling multiple plugins

## [2.3.2-beta.1] - 2026-04-18

### Changed
- Dashboard builds now ignore all `BEHAVIOUR OPTIONS` from `platformio_profile.h`; these overrides are plugin-managed and are no longer compiled into firmware when `ESP32_DASHBOARD` is enabled

### Fixed
- Dashboard injection stop now also blocks plugin-based frame injection instead of letting enabled plugins keep sending
- TWAI dashboard profile syncing no longer forces commented-out behavior options into the build
- Dashboard boot/runtime state no longer reports legacy built-in ISA, emergency vehicle detection, TLSSC bypass, or nag handling as active in plugin-managed builds
- Plugin cards no longer show the obsolete built-in conflict warning badge and message

## [2.3.1] - 2026-04-18

### Fixed
- Dashboard hardware defaults now follow `platformio_profile.h` reliably for dashboard builds, even when older `DASH_DEFAULT_HW` values still exist in the selected PlatformIO environment
- Reflashing a dashboard build with a new default hardware mode now migrates stale stored hardware defaults from NVS without overwriting an explicit hardware choice made later in the web UI

## [2.3.0] - 2026-04-18

### Added
- Dedicated documentation pages for build and flash setup, dashboard usage, and a docs index for GitHub Pages
- Hardware-specific example plugins for HW3 and HW4 feature replacements, including AD activation, TLSSC bypass, nag suppression, Summon unlock, ISA chime suppression, emergency vehicle detection, and HW4 speed offsets

### Changed
- Dashboard Features card now only exposes Enable Logging; the other vehicle overrides are no longer shown there
- GitHub Actions are now split into separate workflows for tests, releases, and GitHub Pages deployment
- Dashboard and README documentation now reflect the plugin-based override flow and current Pages structure

## [2.2.0] - 2026-04-18

First stable release of the 2.2 series. Bundles all changes from 2.2.0-beta.1 through 2.2.0-beta.14.

### Added
- Plugin Editor UI with live JSON preview, duplicate-name detection, download/export, install support, loading of installed plugins, and a rule test tool for sending generated CAN frames
- CAN Sniffer support for switching between on-wire 11-bit IDs and prefixed DBC JSON IDs, with filtering for both formats
- Dashboard improvements including plugin capacity visibility, default CAN pin hints, a hidden SSID option for the WiFi hotspot, and an Atom S3 Mini injection toggle on the built-in button

### Changed
- Dashboard defaults now follow the selected build flags, including injection-on-boot behavior and vehicle-aware web UI defaults for TWAI and MCP2515 targets
- `esp32_feather_v2_mcp2515` now uses the new MCP2515 driver and web UI
- Reinstalling an existing plugin now preserves its enabled state and works cleanly when the plugin list is already full

### Fixed
- Plugin enable/disable and remove state is now persisted correctly across reboot, and install/remove/toggle actions refresh the dashboard immediately without falling back to a misleading connection error
- Dashboard polling, confirmation flows, and plugin detail panels now behave reliably across reconnects and Chrome on iOS
- WiFi STA/AP persistence, optional NVS reads, injection persistence, and runtime AD gating now behave consistently across reboots and firmware updates
- Atom and Atom S3 builds now use the correct RGB LED pins and a release-safe ESP32 LED API path across the supported CI toolchains

## [2.2.0-beta.14] - 2026-04-18

### Fixed
- Plugin install, remove and enable/disable actions in the dashboard now refresh the plugin list immediately instead of waiting for a manual page refresh
- Dashboard plugin installs no longer fall back to a misleading "Connection error" when the plugin was already applied and only the response body was interrupted
- Atom and Atom S3 dashboard builds now use an ESP32 RGB LED API path that stays compatible across the release CI toolchains

## [2.2.0-beta.13] - 2026-04-18

### Added
- Atom S3 Mini builds can now toggle injection with the built-in button on GPIO41, with the state saved so it persists across reboot

### Fixed
- Atom and Atom S3 dashboard builds now drive the built-in RGB status LED from the correct board pins so injection-off shows red and injection-on shows green reliably

## [2.2.0-beta.12] - 2026-04-17

### Fixed
- Plugin detail panels in the dashboard now stay open across the background plugin-list refresh instead of collapsing unexpectedly
- Dashboard confirmation prompts now use an in-page modal so reboot and other confirm actions work reliably in Chrome on iOS

## [2.2.0-beta.11] - 2026-04-17

### Added
- Plugin Editor can now load installed plugins for in-place editing
- Plugin Editor now includes a rule test tool that can send a generated CAN frame multiple times at a chosen interval

### Changed
- Reinstalling an existing plugin by name now preserves its enabled/disabled state and still works when the plugin list is already full

### Fixed
- Atom S3 and Atom Lite dashboard builds can now save CAN pin settings that use GPIO 6-11 instead of being blocked by the generic ESP32 flash-pin restriction

## [2.2.0-beta.10] - 2026-04-17

### Fixed
- Plugin enabled/disabled state is now persisted across reboot instead of defaulting back to enabled on startup
- Removing a plugin now also clears its persisted enabled/disabled state
- Dashboard background polling now stops cleanly after repeated connection failures and points users to the STA IP after WiFi handoff instead of continuously spamming timeout errors

## [2.2.0-beta.9] - 2026-04-17

### Added
- CAN Sniffer now has a toggle to switch between on-wire 11-bit CAN IDs and DBC JSON IDs with the current bus prefix
- The sniffer filter now accepts both on-wire IDs and prefixed DBC JSON IDs

### Changed
- Migrated `esp32_feather_v2_mcp2515` to the new MCP2515 driver and web UI
- Dashboard feature and injection defaults now follow the selected build flags, and `DASH_INJECTION_ON_BOOT` can be used to start injecting automatically after boot
- Dashboard grid inputs now size correctly in narrower layouts, and `platformio_profile.h` no longer ships with `DRIVER_TWAI` and `HW3` preselected by default

### Fixed
- "Stop Injecting" now persists across reboot instead of silently re-enabling injection on startup
- Runtime AD gating is now applied consistently across Legacy, HW3, and HW4 handlers so blocked mux paths no longer keep injecting

## [2.2.0-beta.8] - 2026-04-16

### Added
- Plugins card now shows the maximum plugin capacity, current usage, and a clearer message when the plugin limit has been reached
- `/plugins` now returns `maxPlugins`, and plugin install errors include the configured maximum

### Fixed
- `DRIVER_TWAI` dashboard builds now treat the vehicle selection in `platformio_profile.h` as the default web UI hardware only; if none is selected, `HW3` is used by default
- WiFi STA credentials are now loaded correctly from NVS on boot after being saved through the dashboard
- Optional AP and WiFi preference reads no longer spam `Preferences` `NOT_FOUND` errors when keys have not been stored yet
- Added the option to show default can pins in webdashboard

## [2.2.0-beta.7] - 2026-04-15

### Added
- Hidden SSID option in the WiFi Hotspot card. When enabled, the access point does not broadcast its SSID — clients have to enter the name manually. Setting is persisted in NVS and included in the settings backup/restore JSON (`ap.hidden`)
- `/ap_config` endpoint accepts a new `hidden` parameter; `/ap_status` returns the current `hidden` flag

## [2.2.0-beta.6] - 2026-04-15

### Changed
- Dashboard layout: the two separate Firmware Update cards (GitHub OTA and manual .bin upload) have been merged into a single card. Manual .bin upload is now a collapsible section under the primary update controls
- WiFi Internet moved into its own top-level card (previously nested inside the Plugins card) — it is used for both firmware updates and plugin downloads, so it deserves its own slot

## [2.2.0-beta.5] - 2026-04-15

### Fixed
- Firmware Update check: no longer offers older versions as "updates". A proper semantic-version comparison is now used (major.minor.patch plus alpha/beta/rc pre-release ranking), so a device on `2.2.0-beta.4` will not be prompted to "update" to `2.0.0` or `2.1.0`
- Auto-Update on Boot uses the same comparison (older releases are skipped)
- CI release job: firmware binaries are now reliably attached to GitHub releases. The workflow first creates the release as a draft, uploads all assets, and then publishes it, which works around the repository's "immutable releases" setting that previously blocked asset uploads after publish

## [2.2.0-beta.4] - 2026-04-15

### Changed
- Default dashboard credentials (`changeme`) are now allowed at build time. Users are expected to change the WiFi AP password and OTA credentials at runtime via the dashboard WiFi Hotspot card (persisted in NVS, OTA-safe)
- Build no longer fails when `DASH_PASS` / `DASH_OTA_PASS` are left at the default `changeme` placeholder

### Removed
- Nag Killer toggle removed from the dashboard Features card. The underlying `NAG_KILLER` build flag remains available for advanced users who want to compile it in

## [2.2.0-beta.3] - 2026-04-15

### Fixed
- Firmware Update check: "JSON parse error" when Beta Channel is enabled (reduced GitHub API response size by using `per_page=1` instead of `per_page=5`, avoids ArduinoJson heap overflow on ESP32)
- CI: firmware artifacts are now correctly attached to GitHub releases. Previous releases failed to attach binaries due to an "immutable release" error when the release was published before the upload step ran

### Changed
- CI release notes: workflow now extracts the matching section from `CHANGELOG.md` for each tag instead of using the whole file, and auto-detects prerelease based on the tag name (`beta`/`alpha`/`rc`)

## [2.2.0-beta.2] - 2026-04-15

### Added
- Auto-Update on Boot: optional toggle in the Firmware Update card. When enabled, the device checks GitHub for a newer release ~15 seconds after the WiFi Internet connection comes up and installs it automatically
- Respects the Beta Channel toggle (only installs prereleases when that is on)
- Setting persisted in NVS so it survives firmware updates
- New endpoints: `/auto_update` (GET/POST)

## [2.2.0-beta.1] - 2026-04-15

### Added
- Plugin Editor card: create plugins via a form UI without writing JSON manually
- Support for all plugin ops: set_bit, set_byte, or_byte, and_byte, checksum
- Per-rule configuration of CAN ID, optional mux value, and send flag
- Live JSON preview updating as you edit, with collapsible rule sections
- Client-side validation (ID, mux, bit 0-63, byte 0-7, value 0-255, hex input `0xFF` supported)
- One-click Install via existing `/plugin_upload` endpoint (no backend changes)
- Download generated plugin as a standalone `.json` file for sharing or backup
- Duplicate-name detection against existing installed plugins

## [2.1.0] - 2026-04-15

First stable release of the 2.1 series. Bundles all changes from 2.1.0-beta.1 through 2.1.0-beta.5.

### Added
- **Rebrand & UX:** renamed UI from "ADUnlock" to "ev-open-can-tools"; dynamic footer with firmware version and device IP; GitHub and Discord links in the footer
- **Plugins:** info icon next to "Plugins" with inline explanation and link to plugin documentation with examples
- **CAN Pins:** runtime-configurable CAN TX/RX GPIO pins via the dashboard, persisted in NVS so custom pin configurations survive OTA firmware updates; validation (GPIO 0-39, TX != RX, GPIO 6-11 blocked for SPI flash)
- **WiFi Internet:** network scanner with RSSI and channel info; static IP configuration (IP, gateway, subnet, DNS); dedicated status and scan endpoints
- **WiFi Hotspot:** change AP name and password via the dashboard; credentials stored in NVS and survive firmware updates
- **OTA firmware updates from GitHub releases:** check for updates and install directly from the dashboard, with beta channel toggle
- **Status badges** ("saved" / "firmware default") on WiFi Hotspot and WiFi Internet cards, plus an info icon explaining NVS persistence
- **Settings Backup / Restore:** export all persistent settings (AP, WiFi Internet, CAN pins, beta flag) as JSON and restore in one go — disaster recovery for full-erase or cross-device migration

### Note
- WiFi credentials have been OTA-safe since the original 2.1 series; the badges and backup feature make that explicit and add recovery paths for full-flash-erase scenarios.

## [2.1.0-beta.5] - 2026-04-15

### Added
- Visible "saved" / "firmware default" status badge on the WiFi Hotspot and WiFi Internet cards
- Info icon on WiFi Hotspot card explaining that credentials are stored in NVS and survive firmware updates
- Settings Backup card: export all persistent settings (AP, WiFi Internet, CAN pins, beta flag) as JSON for safekeeping or migration to another device
- Settings Restore: upload a previously exported JSON to restore all persistent settings in one go
- New endpoints: /settings_export (GET), /settings_import (POST)

### Note
- WiFi credentials were already OTA-safe in prior versions; these changes make the persistence explicit in the UI and add disaster-recovery via backup file

## [2.1.0-beta.4] - 2026-04-15

### Added
- Runtime-configurable CAN TX/RX pins: configure GPIO pins for the TWAI transceiver directly from the dashboard
- Pin configuration is persisted in NVS so it survives OTA firmware updates
- New "CAN Pins" dashboard card with validation (GPIO 0-39, TX != RX, GPIO 6-11 blocked for SPI flash) and reboot flow
- New endpoints: /can_pins (GET/POST)

### Fixed
- OTA updates no longer risk breaking CAN communication on boards with custom pin configurations — once pins are configured via the dashboard they persist across updates

## [2.1.0-beta.3] - 2026-04-15

### Added
- Plugin info icon: click the (i) next to "Plugins" to see an inline explanation of what plugins are and how they work, with a link to the documentation and examples
- Footer community links: GitHub repository and Discord invite are now linked in the dashboard footer

## [2.1.0-beta.2] - 2026-04-15

### Changed
- Rebrand: renamed all UI references from "ADUnlock" to "ev-open-can-tools"
- Footer now shows current firmware version and dynamically adapts to the device IP
- Removed hardcoded hardware references from footer

### Added
- WiFi network scanner: scan and display available networks in the dashboard, select by clicking
- Signal strength indicators (RSSI) and channel info for each scanned network
- Static IP configuration: optionally set IP, gateway, subnet mask and DNS server
- OTA firmware update from GitHub releases: check for updates and install directly from the dashboard
- Beta channel toggle: switch between stable and pre-release firmware versions
- Firmware version auto-injected from VERSION file at build time
- Dedicated WiFi status endpoint (/wifi_status) and scan endpoint (/wifi_scan)
- WiFi hotspot configuration: change AP name and password via the dashboard
- New update endpoints: /update_check, /update_install, /update_beta
- New AP endpoints: /ap_config, /ap_status

## [2.0.0] - 2026-04-14

### Added
- Plugin system: install CAN frame modification rules as JSON files via web dashboard
- Plugin Manager UI card with install from URL, file upload, enable/disable and remove
- Paste JSON (offline): install plugins by pasting JSON directly into the dashboard — no internet or file picker needed
- Plugin detail view: expandable rule inspector showing CAN IDs, mux values and all operations per plugin
- Conflict detection: warns with a visual indicator when plugin CAN IDs overlap with base firmware handlers
- WiFi STA mode (AP+STA) for internet access to download plugins
- Plugin engine with mux-aware matching and operations: set_bit, set_byte, or_byte, and_byte, checksum
- Automatic CAN filter merging for plugin-required IDs
- ArduinoJson v7 dependency for all ESP32 dashboard environments
- New API endpoints: /plugins, /plugin_upload, /plugin_install, /plugin_toggle, /plugin_remove, /wifi_config
- Plugin documentation with format reference, examples and CAN ID table (docs/plugins.md)

### Fixed
- Credential placeholder check in build script now matches platformio_profile.h defaults
- CI release job: firmware artifacts renamed to unique filenames to prevent upload conflicts

## [1.0.0] - 2026-04-10

First release
