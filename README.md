# EV Open Can Mod

[Repository](.) | [Release Notes](CHANGELOG.md) | [Community Discord](https://discord.gg/ZTQKAUTd2F)

An open-source general-purpose CAN bus modification tool for supported vehicles. The goal is to expose and control everything accessible via CAN — as a fully open project.

## Disclaimer

> [!WARNING]
> **Modifying CAN bus messages can cause dangerous behavior or permanently damage your vehicle.** The CAN bus controls everything from braking and steering to airbags — a malformed message can have serious consequences. If you don't fully understand what you're doing, **do not install this on your car**.

This project is for testing and educational purposes only and for use on **private property**. The authors accept no responsibility for any damage to your vehicle, injury, or legal consequences resulting from the use of this software. This project may void your vehicle warranty and **may not comply with road safety regulations in your jurisdiction**.

For any use beyond private testing, you are responsible for complying with all applicable local laws and regulations. Always keep your hands on the wheel and stay attentive while driving.

## Features

- **Nag Suppression** — Clears the hands-on-wheel ECE R79 nag bit, suppressing the periodic "apply pressure to the steering wheel" warning
- **Autosteer Nag Killer** — Suppresses the Autopilot "hands on wheel" alert by echoing modified EPAS steering torque frames on CAN bus 4
- **Actually Smart Summon (ASS)** — Removes EU regulatory restrictions on Smart Summon (HW3/HW4)
- **Speed Profiles** — Maps the follow-distance stalk setting to Auto driving aggressiveness profiles (Chill / Normal / Hurry / Max / Sloth)
- **ISA Speed Chime Suppression** — Mutes the Intelligent Speed Assistance audible chime while keeping the visual indicator active (HW4, optional)
- **Emergency Vehicle Detection** — Enables approaching emergency vehicle detection on Auto driving v14 (HW4, optional)
- **Web Interface** — WiFi hotspot on ESP32 boards for real-time monitoring, runtime feature toggles, and over-the-air firmware updates

Feature details are maintained in this repository.

## What It Does

The firmware runs on multiple supported boards — Adafruit Feather RP2040 CAN, Feather M4 CAN Express, ESP32-based boards, and M5Stack Atomic CAN Base. It sits on the vehicle CAN bus, intercepts relevant frames, modifies the necessary bits, and re-transmits the modified frames — all in real time.

Features are selected at compile time via `platformio_profile.h` and vary by hardware variant (Legacy / HW3 / HW4). ESP32 boards additionally expose a WiFi web interface for runtime control and OTA firmware updates.

## Installation

The repository is PlatformIO-only. Select your board and vehicle variant in `platformio_profile.h`, then flash the firmware to your board.

See the installation section below for the current flashing flow.

## Versioning

- The project version is tracked in [`VERSION`](VERSION) using Semantic Versioning.
- Release notes are tracked in [`CHANGELOG.md`](CHANGELOG.md).
- Ongoing work should be added to the `Unreleased` section before merge.

## Third-Party Libraries

This project depends on the following open-source libraries. Their full license texts are in [THIRD_PARTY_LICENSES](THIRD_PARTY_LICENSES).

| Library | License | Copyright |
|---------|---------|-----------|
| [autowp/arduino-mcp2515](https://github.com/autowp/arduino-mcp2515) | MIT | (c) 2013 Seeed Technology Inc., (c) 2016 Dmitry |
| [adafruit/Adafruit_CAN](https://github.com/adafruit/Adafruit_CAN) | MIT | (c) 2017 Sandeep Mistry |
| [espressif/esp-idf](https://github.com/espressif/esp-idf) (TWAI driver) | Apache 2.0 | (c) 2015-2025 Espressif Systems (Shanghai) CO LTD |

## License

This project is licensed under the **GNU General Public License v3.0** — see the [GPL-3.0 License](https://www.gnu.org/licenses/gpl-3.0.html) for details.
