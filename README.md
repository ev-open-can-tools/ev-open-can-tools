# EV Open Can Mod

[Live Docs](https://ev-open-can-tools.github.io/ev-open-can-tools/) | [Documentation](docs/index.md) | [Dashboard Guide](docs/dashboard.md) | [Build & Flash](docs/building.md) | [Plugin System](docs/plugins.md) | [Release Notes](CHANGELOG.md) | [Community Discord](https://discord.gg/ZTQKAUTd2F)

An open-source CAN bus modification tool for supported vehicles and ESP32/Feather CAN hardware. The firmware sits on the vehicle CAN bus, watches the frames it cares about, modifies the required bits or bytes, and retransmits the result in real time.

ESP32 dashboard builds add a local web interface for runtime feature control, CAN diagnostics, plugins, WiFi management, and OTA firmware updates.

## Disclaimer

> **Warning:** Modifying CAN bus traffic can cause dangerous behavior or permanently damage a vehicle. The CAN bus touches safety-critical systems including steering, braking, airbags, and gateway functions. If you do not fully understand the frames you are changing, do not install or use this firmware on a vehicle.
>
> This project is for testing and educational use only. You are responsible for complying with local laws, safety requirements, and any warranty or road-use implications in your jurisdiction.

## Highlights

- **Vehicle-side features**: AD activation, nag suppression, Actually Smart Summon EU unlock, speed profile mapping, HW4 ISA speed chime suppression, HW4 emergency vehicle detection, optional TLSSC bypass, and a dedicated Autosteer nag-killer build
- **ESP32 dashboard**: runtime hardware mode switching, live status, feature toggles, CAN sniffer, CAN recorder, controller stats, live log, stop/resume injection, and reboot control
- **Connectivity and OTA**: configurable WiFi hotspot, hidden SSID support, WiFi STA with scan and optional static IP, GitHub release updates, beta channel, auto-update on boot, and manual `.bin` upload
- **Plugin system**: install plugins by URL, file upload, or pasted JSON; inspect rules; detect firmware overlap; keep enable state across reboot; build plugins in the browser with the Plugin Editor; test generated frames before install
- **Persistent settings**: WiFi hotspot, WiFi internet, CAN pins, update channel, auto-update, plugin enabled state, and other runtime settings are stored in NVS/SPIFFS where applicable

## Supported Environments

| PlatformIO env | Board / target | CAN interface | Dashboard |
| --- | --- | --- | --- |
| `feather_rp2040_can` | Adafruit Feather RP2040 CAN | MCP2515 | No |
| `feather_m4_can` | Adafruit Feather M4 CAN Express | Native CAN | No |
| `esp32_twai` | Generic ESP32 dev board | TWAI | Yes |
| `lilygo_tcan485_hw3` | LILYGO TCAN485 | TWAI | Yes |
| `m5stack-atomic-can-base` | M5Stack Atom CAN Base | TWAI | Yes |
| `m5stack-atoms3-mini-can-base` | M5Stack AtomS3 Mini CAN Base | TWAI | Yes |
| `esp32_feather_v2_mcp2515` | Feather ESP32 V2 + external MCP2515 | SPI MCP2515 | Yes |
| `esp32_ext_mcp2515` | ESP32-S3 + external MCP2515 | SPI MCP2515 | Yes |
| `waveshare_ESP32_S3_RS485_CAN` | Waveshare ESP32-S3 RS485/CAN | TWAI | Yes |

ESP32 dashboard builds are the full-featured path: they include the web UI, plugin engine, WiFi, OTA, and persistent runtime settings. Non-dashboard builds keep the core CAN modification logic but do not expose the web management interface.

## Quick Start

1. Choose your target environment from `platformio.ini`.
2. Set your board, vehicle mode, and initial dashboard credentials in `platformio_profile.h`.
3. Build the firmware:

```bash
pio run -e esp32_ext_mcp2515
```

4. Flash the board:

```bash
pio run -e esp32_ext_mcp2515 -t upload
```

5. For ESP32 dashboard builds, connect to the hotspot configured by `DASH_SSID` / `DASH_PASS` and open `http://192.168.4.1/`.
6. After first boot, use the dashboard to adjust hotspot credentials, connect the device to WiFi internet, configure CAN pins when relevant, and manage plugins or OTA updates.

For a fuller setup flow and board-specific notes, see [Build & Flash](docs/building.md).

## Documentation

- [Documentation index](docs/index.md)
- [Dashboard guide](docs/dashboard.md)
- [Build and flash guide](docs/building.md)
- [Plugin system reference](docs/plugins.md)
- [Release notes](CHANGELOG.md)

## Versioning

- The project version is tracked in [`VERSION`](VERSION) using Semantic Versioning.
- Release notes are tracked in [`CHANGELOG.md`](CHANGELOG.md).
- Ongoing work should be added to the `Unreleased` section before merge.

## Third-Party Libraries

This project depends on the following open-source libraries. Their full license texts are in [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES).

| Library | License | Copyright |
| --- | --- | --- |
| [autowp/arduino-mcp2515](https://github.com/autowp/arduino-mcp2515) | MIT | (c) 2013 Seeed Technology Inc., (c) 2016 Dmitry |
| [adafruit/Adafruit_CAN](https://github.com/adafruit/Adafruit_CAN) | MIT | (c) 2017 Sandeep Mistry |
| [espressif/esp-idf](https://github.com/espressif/esp-idf) (TWAI driver) | Apache 2.0 | (c) 2015-2025 Espressif Systems (Shanghai) CO LTD |
| [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) | MIT | (c) 2014-2024 Benoit BLANCHON |

## License

This project is licensed under the **GNU General Public License v3.0** — see the [GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html) for details.
