# EV Open Can Mod

[Documentation](https://ev-open-can-tools.github.io/ev-open-can-tools/) | [Plugin repo](https://github.com/ev-open-can-tools/ev-open-can-tools-plugins) | [Community Discord](https://discord.gg/ZTQKAUTd2F)

EV Open Can Mod is an open-source project for supported vehicles and ESP32 or Feather CAN hardware.

At a basic level, the firmware sits on the vehicle CAN bus, watches selected frames, and can apply real-time changes based on the selected build or installed plugins.

For most people, the easiest way to use this project is with an ESP32 dashboard build. That gives you a local web interface for setup, WiFi, plugins, updates, diagnostics, and basic runtime control.

## Before You Start

This project is **not** plug and play.

It is meant for people who want to learn, test, and experiment carefully. You do **not** need to be a developer to explore the project, read the docs, use the dashboard, or join the community. But you **do** need to be comfortable following instructions, checking your hardware, and understanding that mistakes on a vehicle CAN bus can be serious.

If you are completely new, start with these:

- Read the [Documentation](https://ev-open-can-tools.github.io/ev-open-can-tools/)
- Join the [Community Discord](https://discord.gg/ZTQKAUTd2F)
- Look at the [Build and Flash Guide](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/building.html)
- Use an ESP32 dashboard build if your hardware supports it

## Safety Warning

> **Warning:** Modifying CAN bus traffic can cause dangerous behavior or permanently damage a vehicle.
> 
> The CAN bus touches safety-critical systems including steering, braking, airbags, and gateway functions. If you do not fully understand the frames you are changing, do not install or use this firmware on a vehicle.
> 
> This project is for testing and educational use only. You are responsible for complying with local laws, safety requirements, and any warranty or road-use implications in your jurisdiction.

## Who This Project Is For

This project can be useful for different kinds of people:

- **Curious users** who want to understand what the tool does and follow development
- **Hardware users** who want to flash a supported board and use the dashboard
- **Testers** who want to try builds, compare behavior, and report findings
- **Advanced users** who want to inspect CAN traffic, create plugins, or contribute code and documentation

Not everyone needs to write code to be useful here. Testing, documenting, sharing recordings, and reporting clear findings are valuable contributions too.

## What You Can Do

Depending on your hardware and build, the project can provide:

- Vehicle-side features you can activate via [plugins](https://github.com/ev-open-can-tools/ev-open-can-tools-plugins)
- A local ESP32 dashboard with runtime hardware mode switching, live status, CAN sniffer, CAN recorder, controller stats, live log, stop or resume injection, and reboot control
- WiFi and OTA features including hotspot mode, WiFi internet, GitHub release updates, beta channel support, auto-update on boot, and manual `.bin` upload
- A plugin system that supports install by URL, file upload, or pasted JSON, plus a browser-based Plugin Editor and rule testing tools
- Persistent runtime settings for dashboard-related configuration

## Best Starting Point

If you are not sure where to begin, use this path:

1. Pick a supported **ESP32 dashboard** board.
2. Follow the [Build and Flash Guide](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/building.html).
3. Connect to the device hotspot after first boot.
4. Open the dashboard at `http://192.168.4.1/`.
5. Use the dashboard to configure WiFi, CAN pins if needed, updates, and plugins.
6. Read the [Dashboard Guide](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/dashboard.html) before changing anything important.

This is the easiest and most user-friendly setup path in the project.

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

ESP32 dashboard builds are the full-featured path. They include the web UI, plugin engine, WiFi, OTA, and persistent runtime settings.

Non-dashboard builds keep the core CAN modification logic but do not provide the web management interface.

## Quick Start For More Technical Users

If you already know what you are doing, the basic flow is:

1. Choose your target environment from `platformio.ini`
2. Set your board, vehicle mode, and initial dashboard credentials in `platformio_profile.h`
3. Build the firmware
4. Flash the board
5. Connect to the dashboard if your target supports it

Example:

```bash
pio run -e esp32_ext_mcp2515
pio run -e esp32_ext_mcp2515 -t upload
```

For a fuller setup flow and board-specific notes, see [Build & Flash](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/building.html).

## Documentation

- [Documentation index](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/index.html)
- [Dashboard guide](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/dashboard.html)
- [Build and flash guide](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/building.html)
- [Plugin system reference](https://ev-open-can-tools.github.io/ev-open-can-tools/docs/plugins.html)
- [Release notes](CHANGELOG.md)

## Community

The Discord is not only for developers.

It is also a place for:

- setup help
- hardware questions
- testing feedback
- CAN recordings and observations
- plugin testing
- documentation feedback

If you are learning, testing, or comparing results, you are welcome there too.

## Support & Gift

If you find this project valuable, consider sending a gift with Monero to support its development:

```
46CJEjnN74N83AZHHYKX3mD9kkV6UJYVjN58PTWvQ6VU8Vvn3tmyExkaC2kq9asD6SZY9weaZqx5o9nf1MxkKbmTKWLUeRD
```

Gifts help sustain the project and fund further development.

## Versioning

- The project version is tracked in [`VERSION`](VERSION) using Semantic Versioning
- Release notes are tracked in [`CHANGELOG.md`](CHANGELOG.md)
- Ongoing work should be added to the `Unreleased` section before merge

## Third-Party Libraries

This project depends on the following open-source libraries. Their full license texts are in [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES).

| Library | License | Copyright |
| --- | --- | --- |
| [autowp/arduino-mcp2515](https://github.com/autowp/arduino-mcp2515) | MIT | (c) 2013 Seeed Technology Inc., (c) 2016 Dmitry |
| [adafruit/Adafruit_CAN](https://github.com/adafruit/Adafruit_CAN) | MIT | (c) 2017 Sandeep Mistry |
| [espressif/esp-idf](https://github.com/espressif/esp-idf) (TWAI driver) | Apache 2.0 | (c) 2015-2025 Espressif Systems (Shanghai) CO LTD |
| [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) | MIT | (c) 2014-2024 Benoit BLANCHON |

## License

This project is licensed under the **GNU General Public License v3.0**. See the [GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html) for details.
