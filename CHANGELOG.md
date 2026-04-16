# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

- Migrate esp32_feather_v2_mcp2515 to new mcp2515 driver and web ui

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
