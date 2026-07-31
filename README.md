# ev-open-can-tools

[Start onboarding](https://ev-open-can-tools.github.io/ev-open-can-tools/onboarding/) · [Documentation](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/) · [Plugin repository](https://github.com/ev-open-can-tools/ev-open-can-tools-plugins) · [Discord](https://discord.gg/ZTQKAUTd2F)

Experimental open-source firmware for selected Tesla CAN experiments using ESP32 and other CAN-capable boards.

## Read this first

This project is **not plug-and-play**. CAN connects vehicle computers, including safety-critical systems. A wrong wire, bitrate, filter, checksum, or injected byte can cause dangerous behavior or damage.

Use an isolated bench harness or listen-only tool first. Do not begin on a public road. You are responsible for hardware, testing, local law, warranty, and the result of every frame sent.

New to the project? Follow this order:

1. Start the [guided newcomer onboarding](https://ev-open-can-tools.github.io/ev-open-can-tools/onboarding/) to choose a goal, vehicle mode, board, installation approach, and matching build path.
2. Read [CAN safety and testing](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/nag-killer.html).
3. Follow [Build and flash](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/building.html) for the selected board.
4. Keep injection stopped while learning the [Dashboard](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/dashboard.html).
5. Read the [Plugin system](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/plugins.html) before installing a transmit rule.

## What it does

Depending on board and build, firmware can:

- observe selected CAN frames;
- show CAN health and freshness in a local dashboard;
- store dashboard and plugin settings;
- provide read-only GVRET/SavvyCAN logging;
- load reviewed JSON plugin rules at runtime;
- transmit only when the configured runtime gates permit it.

Dashboard builds are the easiest starting point. Built-in handlers provide observation; enabled plugins are the automatic injection path.

## Supported boards

| PlatformIO environment | Board | CAN | Dashboard |
| --- | --- | --- | --- |
| `esp32_twai` | Generic ESP32 Dev Module | TWAI | Yes |
| `esp32s2_twai` | ESP32-S2 Saola | TWAI | Yes |
| `esp32c6_twai` | ESP32-C6 DevKitC-1 | TWAI | Yes |
| `lilygo_tcan485_hw3` | LILYGO TCAN485 | TWAI | Yes |
| `lilygo_t2can` | LILYGO T-2CAN | SPI MCP2515 (CAN A / Party CAN) | Yes |
| `m5stack-atomic-can-base` | M5Stack Atom CAN Base | TWAI | Yes |
| `m5stack-atoms3-mini-can-base` | M5Stack AtomS3 Mini CAN Base | TWAI | Yes |
| `esp32_feather_v2_mcp2515` | Feather ESP32 V2 + MCP2515 | SPI MCP2515 | Yes |
| `esp32_ext_mcp2515` | ESP32-S3 + MCP2515 | SPI MCP2515 | Yes |
| `waveshare_ESP32_S3_RS485_CAN` | Waveshare ESP32-S3 RS485/CAN | TWAI | Yes |
| `feather_rp2040_can` | Feather RP2040 CAN | MCP2515 | No |
| `feather_m4_can` | Feather M4 CAN Express | Native CAN | No |

## Quick technical start

Install PlatformIO, then from the repository root:

```bash
cp platformio_profile.example.h platformio_profile.h
pio run -e esp32_ext_mcp2515
pio run -e esp32_ext_mcp2515 -t upload
```

Replace the environment with the exact board. Configure the local profile before building. Never commit or share `platformio_profile.h`; it can contain credentials and keys.

After flashing an ESP32 dashboard board, connect to its hotspot and open `http://192.168.4.1/`. Read the [first-boot steps](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/building.html#first-boot).

## Documentation

- [Guided newcomer onboarding](https://ev-open-can-tools.github.io/ev-open-can-tools/onboarding/)
- [Documentation index](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/)
- [Build and flash](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/building.html)
- [Dashboard](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/dashboard.html)
- [Plugin system](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/plugins.html)
- [CAN safety and hands-on-wheel experiments](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/nag-killer.html)
- [ESP32 runtime optimization](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/esp32-optimization.html)
- [Release notes](CHANGELOG.md)

## Contributing

Useful contributions include sanitized CAN captures, board wiring notes, regression tests, documentation improvements, and carefully reviewed code. When reporting a problem, include board/environment, firmware version, bus/bitrate, exact IDs and DLCs, timestamps, Support diagnostics, and the smallest sanitized capture that demonstrates it. Remove credentials, keys, VINs, and private payloads.

## License and dependencies

GPL-3.0. Main dependencies include [ESP-IDF](https://github.com/espressif/esp-idf), [ArduinoJson](https://github.com/bblanchon/ArduinoJson), [arduino-mcp2515](https://github.com/autowp/arduino-mcp2515), and [Adafruit_CAN](https://github.com/adafruit/Adafruit_CAN). See `THIRD_PARTY_LICENSES` for full notices.

Version is tracked in [`VERSION`](VERSION); release notes are in [`CHANGELOG.md`](CHANGELOG.md).
