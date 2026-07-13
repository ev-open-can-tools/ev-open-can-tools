# ev-open-can-tools documentation

[Project home](../) · [Start onboarding](../onboarding/) · [Build and flash](building.md) · [Dashboard](dashboard.md) · [Onboarding guide](onboarding.md) · [Plugins](plugins.md) · [CAN safety](nag-killer.md) · [Runtime optimization](esp32-optimization.md) · [Release notes](../CHANGELOG.md)

## Start here

ev-open-can-tools is experimental firmware for selected Tesla CAN experiments. A small board listens to CAN frames, shows their state, and, when explicitly enabled, can send carefully defined changes back to a bus.

CAN is the vehicle's internal message network. A frame is a small message with an ID, a length, and data bytes. A wrong frame can affect steering, driver assistance, braking, or gateway behavior. This project is not plug-and-play and is not a substitute for a qualified vehicle technician.

New to the project? Use this order:

1. Start the [guided newcomer onboarding](../onboarding/). It helps you choose a goal, vehicle mode, supported board, installation approach, and matching build path without changing a device.
2. Read the [safety and testing guide](nag-killer.md).
3. Follow [Build and flash](building.md) for the selected PlatformIO environment.
4. Connect to the dashboard and use the [Dashboard guide](dashboard.md) with CAN transmission stopped.
5. Read [Plugins](plugins.md) before installing any rule that can transmit.
6. Start with a listen-only or isolated bench test. Do not begin on a public road.

## Choose your path

| Goal | Start with |
| --- | --- |
| Understand the whole setup step by step | [Guided newcomer onboarding](../onboarding/) |
| Use a supported ESP32 board | [Build and flash](building.md), then [Dashboard](dashboard.md) |
| Observe traffic with SavvyCAN | [Dashboard -> GVRET](dashboard.md#savvycan-usb-serial) |
| Create a CAN rule | [Plugin system](plugins.md) |
| Understand hands-on-wheel/nag experiments | [CAN safety and testing](nag-killer.md) |
| Improve firmware performance | [ESP32 runtime optimization](esp32-optimization.md) |
| Contribute code or documentation | Read the repository instructions, then use the validation checklist in the optimization and safety pages |

## What the firmware can do

- read selected standard CAN frames;
- report bus health, freshness, errors, and recovery state;
- serve a local ESP32 dashboard;
- store dashboard and plugin settings;
- expose read-only GVRET/SavvyCAN logging;
- apply enabled plugin rules when every runtime safety gate permits it.

The exact features depend on the board, driver, vehicle mode, and build profile. Built-in handlers are used for observation in dashboard builds; enabled plugins are the automatic injection path.

## Plain-language glossary

- **CAN bus:** the two-wire network used by vehicle controllers to exchange messages.
- **Frame:** one CAN message. It has an ID and up to eight data bytes in this firmware's classic-CAN paths.
- **ID:** the number that identifies the kind of message. Decimal and hexadecimal are both commonly shown.
- **DLC:** data length code; it tells firmware how many bytes are present.
- **Mux:** a selector inside a frame; one ID can carry several layouts.
- **Counter/checksum:** fields used by some ECUs to reject missing or altered messages.
- **TWAI:** ESP32's built-in CAN controller interface.
- **MCP2515:** an external CAN controller connected over SPI.
- **GVRET:** a serial framing protocol understood by SavvyCAN.
- **Plugin:** a JSON rule set loaded at runtime; it is not automatically safe because it is JSON.

## Safety baseline

Keep transmission stopped while learning. Validate the bus, bitrate, ID, DLC, mux, counter, checksum, and byte layout from evidence. Unknown or stale vehicle state must remain a reason to stop, not a reason to transmit. The firmware's hard limits and gates are safety boundaries; do not bypass them as a setup shortcut.

For vehicle-specific wiring, use the correct electrical documentation for the exact model and build date. A generic connector diagram is not enough.
