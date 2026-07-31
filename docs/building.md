# Build and flash

[Documentation](index.md) · [Dashboard](dashboard.md) · [Plugins](plugins.md) · [Release notes](../CHANGELOG.md)

This project uses PlatformIO. You select a board environment, create a local profile, build a firmware image, and then flash that image to the matching board. A firmware image for one board is not interchangeable with another board.

## Before buying or wiring hardware

- Confirm the board, CAN transceiver, logic voltage, connector, and termination resistor.
- Confirm which vehicle bus your experiment belongs on. Party CAN, Vehicle CAN, and Chassis CAN are different networks.
- Use an isolated bench harness or listen-only tool first.
- Do not connect an unknown adapter to a live vehicle bus.
- Keep `platformio_profile.h` private: it can contain WiFi, OTA, or gateway credentials.

## Supported environments

| Environment | Board | CAN interface | Dashboard |
| --- | --- | --- | --- |
| `esp32_twai` | Generic ESP32 Dev Module | Built-in TWAI | Yes |
| `esp32s2_twai` | ESP32-S2 Saola | Built-in TWAI | Yes |
| `esp32c6_twai` | ESP32-C6 DevKitC-1 | Built-in TWAI | Yes |
| `lilygo_tcan485_hw3` | LILYGO TCAN485 | Built-in TWAI | Yes |
| `lilygo_t2can` | LILYGO T-2CAN | SPI MCP2515 (CAN A / Party CAN) | Yes |
| `m5stack-atomic-can-base` | M5Stack Atom CAN Base | Built-in TWAI | Yes |
| `m5stack-atoms3-mini-can-base` | M5Stack AtomS3 Mini CAN Base | Built-in TWAI | Yes |
| `esp32_feather_v2_mcp2515` | Feather ESP32 V2 + external MCP2515 | SPI MCP2515 | Yes |
| `esp32_ext_mcp2515` | ESP32-S3 + external MCP2515 | SPI MCP2515 | Yes |
| `waveshare_ESP32_S3_RS485_CAN` | Waveshare ESP32-S3 RS485/CAN | Built-in TWAI | Yes |
| `feather_rp2040_can` | Adafruit Feather RP2040 CAN | MCP2515 | No |
| `feather_m4_can` | Adafruit Feather M4 CAN Express | Native CAN | No |

The first ten are ESP-IDF dashboard builds. The last two are legacy Arduino builds without the web dashboard.

The LILYGO T-2CAN build uses the board connector labeled **CAN A**, backed by its onboard MCP2515. CAN B/TWAI is not used by this environment. Connect only CAN A to the verified Party CAN pair for nag suppression.

## Create a local profile

From the repository root:

```bash
cp platformio_profile.example.h platformio_profile.h
```

Edit the local file and choose the driver, vehicle mode, initial dashboard credentials, and optional compile-time features. It is ignored by Git. Never paste its secrets into an issue, Support report, plugin, or screenshot.

The helper can apply common choices:

```bash
python scripts/platformio_set_profile.py \
  --driver DRIVER_ESP32_EXT_MCP2515 \
  --vehicle HW4 \
  --enable EMERGENCY_VEHICLE_DETECTION
```

Use the exact driver and vehicle values documented by `platformio_profile.example.h`. Do not guess GPIOs from a similar board.

## VS Code IntelliSense

Install the recommended PlatformIO and Microsoft C/C++ extensions, then open the repository root in VS Code. The tracked workspace settings use PlatformIO as the IntelliSense configuration provider on a normal host and inside the dev container.

Use the PlatformIO environment switcher in the VS Code status bar to select the board you are editing. PlatformIO then supplies the matching compiler, build defines, and include paths for that environment. Generated `.vscode/c_cpp_properties.json`, `.vscode/launch.json`, and `compile_commands.json` files are machine-specific and remain local; do not commit them.

## Build

Install PlatformIO, then build the environment matching the board:

```bash
pio run -e esp32_ext_mcp2515
```

Replace the environment name with the one in the table. A successful build is not proof that wiring, bitrate, termination, or a vehicle frame interpretation is correct.

## Flash

Connect only the matching board and use:

```bash
pio run -e esp32_ext_mcp2515 -t upload
```

Some boards need a boot button or a different USB port. Follow the board maker's upload instructions. Do not flash an image built for a different target.

## First boot

1. Power the board from a safe bench supply.
2. Connect to the hotspot named by `DASH_SSID`.
3. Open `http://192.168.4.1/`.
4. Change default hotspot and OTA credentials.
5. Leave injection stopped while checking the dashboard and CAN wiring.
6. Read the [Dashboard guide](dashboard.md).

The dashboard starts before CAN initialization. On ESP-IDF builds, TWAI waits about 10 seconds before initialization. Injection remains blocked for at least 15 seconds after successful CAN initialization and until more than 1,000 valid frames have been received. These delays are readiness gates, not a guarantee that a vehicle is safe to control.

## Common problems

- **No dashboard:** check power, USB serial output, hotspot name, and the board environment.
- **No CAN frames:** check H/L polarity, transceiver power, bitrate, termination, bus selection, and GPIOs.
- **Bus-off or recovery:** stop transmission, inspect wiring and termination, and use the Support report.
- **Wrong behavior after flashing:** verify the environment and local profile; do not continue testing until the board and driver match.
- **OTA failure:** use a firmware `.bin` built for the exact board and keep a serial recovery path available.

Generated dashboard data lives in `include/web/mcp2515_dashboard_ui.h`. Contributors edit `include/web/mcp2515_dashboard_ui.src.h`; the generated header is recreated by `scripts/minify_dashboard.py`.
