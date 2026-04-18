# Build And Flash

[Project Home](../) | [Documentation](index.md) | [Dashboard Guide](dashboard.md) | [Plugin System](plugins.md) | [Release Notes](../CHANGELOG.md)

The project is PlatformIO-only. Pick the correct board environment in `platformio.ini`, then choose the matching driver and default vehicle mode in `platformio_profile.h`.

## Supported PlatformIO Environments

| Env | Board / target | Notes |
| --- | --- | --- |
| `feather_rp2040_can` | Adafruit Feather RP2040 CAN | MCP2515-based build, no web dashboard |
| `feather_m4_can` | Adafruit Feather M4 CAN Express | Native CAN build, no web dashboard |
| `esp32_twai` | Generic ESP32 dev board | TWAI dashboard build |
| `lilygo_tcan485_hw3` | LILYGO TCAN485 | TWAI dashboard build with board-specific default pins |
| `m5stack-atomic-can-base` | M5Stack Atom CAN Base | TWAI dashboard build with RGB status LED |
| `m5stack-atoms3-mini-can-base` | M5Stack AtomS3 Mini CAN Base | TWAI dashboard build with RGB status LED and built-in injection toggle button |
| `esp32_feather_v2_mcp2515` | Feather ESP32 V2 + external MCP2515 | Dashboard build using the new MCP2515 driver and web UI |
| `esp32_ext_mcp2515` | ESP32-S3 + external MCP2515 | Dashboard build for SPI MCP2515 hardware |
| `waveshare_ESP32_S3_RS485_CAN` | Waveshare ESP32-S3 RS485/CAN | TWAI dashboard build |

## Selecting Driver, Vehicle, And Defaults

You can edit [`platformio_profile.h`](../platformio_profile.h) directly:

- choose one driver define
- choose one vehicle define
- set initial hotspot and OTA credentials
- uncomment optional feature defines when you want compile-time defaults changed

You can also use the helper script:

```bash
python scripts/platformio_set_profile.py --driver DRIVER_ESP32_EXT_MCP2515 --vehicle HW4 --enable EMERGENCY_VEHICLE_DETECTION --enable ENHANCED_AUTOPILOT
```

For `DRIVER_TWAI` dashboard builds, the helper script intentionally enables all optional feature defines so the dashboard can control those options at runtime; the selected vehicle then becomes the default UI mode.

## Build

```bash
pio run -e esp32_ext_mcp2515
```

Replace `esp32_ext_mcp2515` with the environment you are targeting.

## Flash

```bash
pio run -e esp32_ext_mcp2515 -t upload
```

For boards that need a different upload path or boot mode, use the normal PlatformIO upload flow for that board.

## First Boot

- ESP32 dashboard builds start their hotspot from `DASH_SSID` / `DASH_PASS`
- Open `http://192.168.4.1/` after connecting to the hotspot
- Change hotspot and OTA credentials after first boot
- Use the `WiFi Internet` card if you want plugin downloads or OTA updates from GitHub releases
- Use the `CAN Pins` card only on TWAI-based boards when you need non-default GPIO assignments

## Build Outputs

- RP2040 builds produce a `firmware.uf2`
- ESP32 and ATSAME51 builds produce a `firmware.bin`
- Manual dashboard OTA expects a `.bin` built for the exact target board
