# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
