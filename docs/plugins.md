# Plugin system

[Documentation](index.md) · [Dashboard](dashboard.md) · [Build and flash](building.md) · [CAN safety](nag-killer.md) · [Release notes](../CHANGELOG.md) · [Plugin repository](https://github.com/ev-open-can-tools/ev-open-can-tools-plugins)

A plugin is a JSON file containing rules for matching CAN frames and changing selected bits or bytes. The ESP32 validates and stores it on SPIFFS, so a plugin can be updated without rebuilding firmware.

> **Safety warning:** JSON is only a format. A plugin can still send unsafe CAN traffic. Install disabled, review every rule, test on an isolated bench, and keep injection stopped until the target ID, bus, DLC, mux, counter, checksum, and vehicle-state gate are proven.

## Recommended workflow

1. Obtain the plugin from a source you trust.
2. Read the complete file before installing it.
3. Check the target vehicle, bus, ID, DLC, mux, operations, cadence, counter, and checksum.
4. Install by URL, file, or paste; new installs start disabled.
5. Confirm the plugin name, version, rule count, and priority in the dashboard.
6. Test with transmission disabled or on an isolated bench.
7. Enable only the reviewed plugin and monitor Status, Last Write Check, GVRET, and Support diagnostics.

## Smallest valid example

```json
{
  "name": "Example observer",
  "version": "1.0",
  "rules": [
    {
      "id": 921,
      "mux": -1,
        "ops": [
          { "type": "set_bit", "bit": 0, "val": 0 }
        ],
      "send": false
    }
  ]
}
```

Use `send: false` while learning. The harmless `set_bit` operation above demonstrates the schema without permitting a transmit.

## Top-level fields

| Field | Type | Required | Meaning |
| --- | --- | --- | --- |
| `name` | string | yes | Identifier, maximum 31 characters. Same-name install replaces the old file. |
| `version` | string | no | Display version; defaults to `1.0`. |
| `author` | string | no | Display author. |
| `rules` | array | yes | CAN rules, subject to firmware limits. |

## Rule fields

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | CAN ID to match. Decimal and hexadecimal are equivalent when represented as numbers. |
| `bus` | string, integer, or array | Optional `CH`, `VEH`, `PARTY`, comma-separated names, bitmask (`1=CH`, `2=VEH`, `4=PARTY`), or name array. Omit only when any bus is truly intended. |
| `mux` | integer | Byte-0 selector. `-1` or omitted matches any mux. |
| `mux_mask` | integer | Mask for mux comparison. Use the mask proven by the frame layout; aliases `muxMask`. |
| `match_byte` | integer | Optional extra byte index `0..7`; alias `matchByte`. |
| `match_mask` | integer | Mask for the extra byte; alias `matchMask`. |
| `match_val` | integer | Expected masked value; aliases `match_value`, `matchValue`. |
| `ops` | array | Ordered operations applied to a copy of the incoming frame. |
| `send` | boolean | Include the changed frame in the composed transmit; defaults to `true`. |

The alternative match form is:

```json
"match": { "byte": 4, "mask": 192, "val": 0 }
```

## Operations

Operations run in order. Change only the bits owned by the rule. If a frame has a checksum, make `checksum` the last operation.

### Bit and byte operations

```json
{ "type": "set_bit", "bit": 46, "val": 1 }
{ "type": "set_byte", "byte": 3, "val": 26, "mask": 63 }
{ "type": "or_byte", "byte": 1, "val": 32 }
{ "type": "and_byte", "byte": 4, "val": 191 }
```

`bit` is `0..63`; `byte` is `0..7`; values and masks are `0..255`. `set_byte` changes only bits in its mask. `or_byte` sets bits and `and_byte` clears bits.

### Rolling counter

```json
{ "type": "counter", "byte": 0, "mask": 15, "step": 1 }
```

The firmware reads the masked field, adds `step`, wraps within that field, and writes it back. Confirm the counter's width, location, and increment point from the target protocol. Do not reuse this example blindly.

### Checksum

```json
{ "type": "checksum" }
```

For the vehicle checksum used by this operation, byte 7 is calculated from the CAN ID and bytes 0–6. Always run checksum after every other mutation. Verify exact output bytes with fixtures before enabling `send`.

### Periodic gateway frame

```json
{ "type": "emit_periodic", "interval": 100, "gtw_silent": false }
```

This is restricted to the documented gateway ID/mux combination and emits a cached frame at the requested interval. Periodic emission is a transmit feature, not a passive monitor. Keep `gtw_silent` false unless the custom security implementation and complete UDS sequence are explicitly reviewed. The repository does not contain Tesla's SecurityAccess key algorithm.

## Limits and conflict rules

| Resource | Limit |
| --- | ---: |
| Installed plugins | 8 |
| Rules per plugin | 16 |
| Operations per rule | 16 |
| Filter IDs per plugin | 32 |

Enabled rules matching one incoming frame are composed into one output frame. Plugin priority is installation/order priority; when two plugins claim the same bit, the higher-priority write wins and the lower-priority write is ignored.

## Installing and managing

### URL

Enable WiFi Internet, enter an HTTPS URL, and choose **Install**. Prefer a pinned, reviewable source. Do not install a URL that can silently change during a test.

### File or paste

Use **Install JSON** with a downloaded `.json` file or pasted text. This works without Internet. Review the parsed name and rules before enabling.

### Priority and persistence

Use enable/disable, move up/down, and remove controls. Plugins and their enabled state persist across reboot. A plugin that persists is not automatically appropriate for a different vehicle or bus.

## Example files

The repository contains a [duplicate-counter example](plugin%20examples/0x370-duplicate-counter.json) and a [Summon EU HW3 example](plugin%20examples/summon-eu-unlock-hw3.json). They demonstrate syntax, not universal vehicle compatibility. The maintained community collection is the [plugin repository](https://github.com/ev-open-can-tools/ev-open-can-tools-plugins).

Common IDs shown in examples include `0x370` (`880`), `0x399` (`921`), `0x3EE` (`1006`), `0x3F8` (`1016`), `0x3FD` (`1021`), and `0x7FF` (`2047`). Treat these as references only; bus, mux, firmware version, and payload layout still need proof.

## Troubleshooting

- **Invalid JSON:** validate quotes, commas, types, and the required `name`/`rules` fields.
- **Plugin installs but does not match:** check bus, ID representation, DLC, mux mask, and filter limits.
- **Plugin matches but does not send:** inspect `send`, plugin enablement, runtime gates, freshness, and vehicle mode.
- **Checksum or counter mismatch:** verify operation order and exact target layout; do not add a second counter or checksum “just in case.”
- **Unexpected behavior:** stop injection, disconnect the interface, save Support diagnostics, and report a sanitized capture. Never bypass a gate to make a plugin appear active.
