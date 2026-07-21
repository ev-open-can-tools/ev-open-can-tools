# CAN safety and hands-on-wheel experiments

[Documentation](index.md) · [Build and flash](building.md) · [Dashboard](dashboard.md) · [Plugins](plugins.md)

> **Safety warning:** CAN injection can change safety-critical vehicle behavior. This page is educational and experimental. Begin with a listen-only or isolated bench setup, never a public road. You are responsible for local law, vehicle safety, hardware isolation, and the consequences of every frame sent.

## What this feature is

The project can observe and, when deliberately enabled, modify selected CAN frames used by hands-on-wheel or driver-assistance experiments. It does not disable the vehicle's safety systems, prove that an ECU accepted a message, or make a vehicle autonomous.

## Use the correct bus

Nag-suppression traffic belongs on **Party CAN** in the supported experiments. Do not assume that a connector or pinout is the same across models or build dates. Do not connect this feature to Vehicle CAN or Chassis CAN because a diagram looks similar.

Use Tesla's [Model 3 Electrical Reference](https://service.tesla.com/docs/Model3/ElectricalReference/) or the correct service documentation for the exact vehicle to identify the bus. Verify H/L polarity, transceiver voltage, termination, and 500 kbit/s traffic with a listen-only tool before enabling any write.

## Required evidence

Before creating or enabling a rule, confirm from captures, tests, or authoritative documentation:

- CAN ID and bus;
- standard frame format and DLC;
- mux byte and valid mux values;
- bit numbering, scale, signedness, and units;
- rolling-counter width and wrap behavior;
- checksum or CRC coverage;
- expected cadence and freshness timeout;
- the vehicle state that permits a write.

Do not infer a signal from its name alone. A decimal ID and its hexadecimal form are the same ID; for example, `880` is `0x370`.

## Runtime gates

Dashboard ESP32 builds keep injection blocked until CAN has initialized, the startup delay has elapsed, and more than 1,000 valid frames have arrived. Additional AP, gear, freshness, mode, and plugin gates can still block a send. Unknown or stale state must block a write.

The firmware enforces a complete 12-bit torque encoding and hard `-1.80 Nm` to `+1.80 Nm` bounds (`0x74E` through `0x8B6`) for the built-in nag path. A plugin cannot safely be assumed to have these semantics; review its operations and target frame separately.

Dashboard builds expose the built-in path as Off plus Modes A, B, and C on Legacy/HW3. Mode A uses fixed positive torque, Mode B uses a bounded four-value burst/pause cycle, and Mode C requires fresh DAS hands-on status `0x399` and valid steering context `0x129`. All modes are blocked on HW4 after hardware testing reported red take-over plus traction-control and auto-hold faults. Stored HW4 selections fail closed to Off. These controls are direct firmware settings, not plugins, and default to Off.

## Recommended test progression

1. Validate decoding and exact output bytes with native tests.
2. Replay sanitized captures with transmission disabled.
3. Use a current-limited, isolated bench bus and a second CAN logger.
4. Confirm that disabled, stale, wrong-DLC, wrong-mux, unrelated-ID, and send-failure cases remain silent.
5. Only qualified people should consider stationary vehicle testing with an independent disconnect and continuous logging.

Never use Last Write Check as the only safety instrument. It detects a later overwrite; it does not prove ECU acceptance or physical response.

## If something looks wrong

Stop transmission and disconnect the interface. Save the Support report, serial log, board environment, local profile choices without secrets, bus wiring, bitrate, and a sanitized capture. Report exact IDs/DLCs and timestamps. Do not “fix” a mismatch by widening filters, bypassing a gate, or increasing a limit.
