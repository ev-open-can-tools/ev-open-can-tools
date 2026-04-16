#pragma once

// Shared PlatformIO profile.
// Edit this file from the repository root before building.

// ── BOARD SELECTION ──────────────────────────────────────────────
// Uncomment ONE of the following lines to match your board:
// #define DRIVER_MCP2515           // Adafruit Feather RP2040 CAN (MCP2515 over SPI)
// #define DRIVER_SAME51            // Adafruit Feather M4 CAN Express (native ATSAME51 CAN)
// #define DRIVER_TWAI              // ESP32 boards with built-in TWAI (CAN) peripheral
// #define DRIVER_ESP32_EXT_MCP2515 // ESP32-S3 + external MCP2515 via SPI (use esp32_ext_mcp2515 env)

// ── VEHICLE HARDWARE SELECTION ───────────────────────────────────
// Uncomment ONE of the following lines to match your vehicle:
// #define LEGACY // HW3-retrofit
// #define HW3    // HW3
// #define HW4    // HW4

// ── DASHBOARD CREDENTIALS ────────────────────────────────────────
// Required for all ESP32 dashboard builds. These are the initial values used
// on first boot; change them at runtime via the dashboard WiFi Hotspot card
// (persisted in NVS and survive firmware updates).
#define DASH_SSID "EVtools"      // WiFi AP name
#define DASH_PASS "changeme"     // WiFi password (min 8 chars)
#define DASH_OTA_USER "admin"    // OTA username
#define DASH_OTA_PASS "changeme" // OTA password

// ── BEHAVIOUR OPTIONS ────────────────────────────────────────────
// Uncomment any of the following lines:
// #define ISA_SPEED_CHIME_SUPPRESS    // Suppress ISA speed chime; speed limit sign will be empty while driving
// #define EMERGENCY_VEHICLE_DETECTION // Enable emergency vehicle detection
// #define BYPASS_TLSSC_REQUIREMENT    // Always enable drivepilot without requiring "Traffic Light and Stop Sign Control" toggle
// #define NAG_KILLER                  // Suppress Autosteer "hands on wheel" nag (CAN 880 counter+1 echo, X179 pin 2/3)
// #define ENHANCED_AUTOPILOT          // Enable UI_applyEceR79 override on HW3/HW4 and summon on HW4
