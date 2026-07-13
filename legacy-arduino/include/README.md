# Legacy Arduino builds

These targets are for boards that do not run the ESP-IDF dashboard. They still share the core CAN handlers with the root project, but they do not provide WiFi, OTA, web settings, plugins, or Support diagnostics.

- `feather_rp2040_can` — Feather RP2040 with MCP2515 CAN controller
- `feather_m4_can` — Feather M4 CAN Express with native CAN

Read the root [Build and flash guide](../../docs/building.md) first. Use an isolated bench bus or listen-only setup before connecting a live vehicle bus.

Build from this directory with:

```bash
pio run -e feather_rp2040_can
pio run -e feather_m4_can
```

The firmware source is shared with the repository root. The root project owns ESP32-family ESP-IDF builds.
