# Plugin System

[Project Home](../) | [Documentation](index.md) | [Dashboard Guide](dashboard.md) | [Build & Flash](building.md) | [Release Notes](../CHANGELOG.md) | [Plugin repo](https://github.com/ev-open-can-tools/ev-open-can-tools-plugins)

The plugin system allows you to create and share CAN frame modification rules as JSON files. Plugins are loaded at runtime on the ESP32 — no recompilation needed, and nothing has to be stored in this repository.

## How it works

1. You write a plugin as a `.json` file
2. You host it anywhere (GitHub, your own server, etc.)
3. Users install it via the dashboard — either by entering the URL or uploading the file
4. The ESP32 stores the plugin on SPIFFS and applies the rules to incoming CAN frames

## Dashboard workflow

- Use the **Plugins** card to install a plugin from URL, upload a `.json`, or paste JSON directly
- New installs start disabled so you can review conflicts and priority before enabling them
- Use the **Plugin Editor** to build a plugin from form fields instead of editing raw JSON by hand
- Load an installed plugin back into the editor when you want to adjust an existing rule set and reinstall it
- Use **Rule Test** to wait for the next matching live CAN frame, apply one editor rule to that frame, and send the result a chosen number of times

## Plugin JSON format

```json
{
  "name": "My Plugin",
  "version": "1.0",
  "author": "Your Name",
  "rules": [
    {
      "id": 921,
      "mux": -1,
      "ops": [
        { "type": "set_bit", "bit": 13, "val": 1 },
        { "type": "checksum" }
      ],
      "send": true
    }
  ]
}
```

### Top-level fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | yes | Plugin name (max 31 chars). Used as identifier — installing a plugin with the same name replaces the old one. |
| `version` | string | no | Version string, displayed in the dashboard. Defaults to `"1.0"`. |
| `author` | string | no | Author name, displayed in the dashboard. |
| `rules` | array | yes | Array of CAN rule objects. At least one rule is required. |

### Rule object

Each rule matches incoming CAN frames by ID, optional bus, and optional mux index, applies a sequence of operations, and optionally includes the result in the composed frame sent back on the bus.

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | integer | yes | CAN frame ID to match (decimal, e.g. `921` = `0x399`). |
| `bus` | string, integer, or array | no | Bus pin: `CH`, `VEH`, `PARTY`, a comma-separated string such as `"CH,VEH"`, a bitmask (`1=CH`, `2=VEH`, `4=PARTY`), or an array of names. Omit to match any bus. Frames with unknown bus still match pinned rules for backwards compatibility. Single-bus builds can enforce pins by defining `CAN_BUS_DEFAULT=CAN_BUS_CH`, `CAN_BUS_DEFAULT=CAN_BUS_VEH`, or `CAN_BUS_DEFAULT=CAN_BUS_PARTY`. |
| `mux` | integer | no | Mux value to match in byte 0. `-1` or omit to match any mux. Values `0..7` default to legacy low-3-bit matching. Values `8..255` default to full-byte matching. |
| `mux_mask` | integer | no | Byte-0 mask used with `mux`. Matching is `(byte0 & mux_mask) == (mux & mux_mask)`. Use `7` for low-3-bit muxes, `15` for low-nibble muxes, and `255` for full-byte DBC muxes. Alias: `muxMask`. |
| `match_byte` | integer | no | Optional byte index `0..7` for an additional byte-mask match. Alias: `matchByte`. |
| `match_mask` | integer | no | Optional mask for `match_byte`. `0` or omitted disables the extra byte match. Alias: `matchMask`. |
| `match_val` | integer | no | Optional value used with `match_byte`/`match_mask`. Matching is `(byte[match_byte] & match_mask) == (match_val & match_mask)`. Aliases: `match_value`, `matchValue`. |
| `ops` | array | yes | Array of operations to apply (see below). |
| `send` | boolean | no | Include this rule in the composed frame sent on the CAN bus. Defaults to `true`. |

Instead of flat `match_*` fields, you can use:

```json
"match": { "byte": 4, "mask": 192, "val": 0 }
```

### Operations

Operations are applied in priority order on a **copy** of the original frame. In dashboard builds, automatic CAN writes are limited to enabled plugin rules with `send: true`; built-in handlers only observe frames for dashboard status.

When multiple enabled plugin rules match the same incoming CAN ID and mux, the firmware composes one output frame and sends it once. For GTW 2047 (`0x7FF`), the dashboard's plugin replay count can repeat that modified frame immediately. Plugin priority decides overlapping writes: the highest-priority plugin owns a bit first, and lower-priority plugins cannot overwrite that same bit in the same frame cycle.

#### `set_bit` — Set or clear a single bit

```json
{ "type": "set_bit", "bit": 46, "val": 1 }
```

| Param | Type | Description |
|-------|------|-------------|
| `bit` | 0-63 | Bit position in the 8-byte data field. Bit 0 = byte 0 bit 0, bit 8 = byte 1 bit 0, etc. |
| `val` | `0`/`1` or `false`/`true` | Value to set. Defaults to `1`. |

#### `set_byte` — Set a byte value with optional mask

```json
{ "type": "set_byte", "byte": 3, "val": 26, "mask": 63 }
```

| Param | Type | Description |
|-------|------|-------------|
| `byte` | 0-7 | Byte index in the data field. |
| `val` | 0-255 | Value to write. |
| `mask` | 0-255 | Bitmask — only bits set in the mask are modified. Defaults to `255` (0xFF, full byte). |

The formula is: `data[byte] = (data[byte] & ~mask) | (val & mask)`

#### `or_byte` — Bitwise OR a byte

```json
{ "type": "or_byte", "byte": 1, "val": 32 }
```

Sets specific bits without clearing others. `data[byte] |= val`

#### `and_byte` — Bitwise AND a byte

```json
{ "type": "and_byte", "byte": 4, "val": 191 }
```

Clears specific bits without affecting others. `data[byte] &= val`

#### `counter` — Increment a rolling counter field

```json
{ "type": "counter", "byte": 0, "mask": 15, "step": 1 }
```

| Param | Type | Description |
|-------|------|-------------|
| `byte` | 0-7 | Byte index in the data field. |
| `mask` | 1-255 | Contiguous bitmask for the counter field. Defaults to `15` (0x0F, lower nibble). |
| `step` | 1-255 | Amount to add before wrapping inside the masked field. Defaults to `1`. |

The counter is read from the masked bits, incremented modulo the field width, and written back into the same bits. Place `counter` before `checksum` when the frame also uses the byte 7 vehicle checksum. Replayed GTW 2047 frames, repeated Rule Test sends, and periodic emits advance the counter again before each extra send.

#### `emit_periodic` — Periodically emit the cached GTW mux 3 frame

```json
{ "type": "emit_periodic", "interval": 100, "gtw_silent": true }
```

| Param | Type | Description |
|-------|------|-------------|
| `interval` | 10-5000 | Emit interval in milliseconds. Defaults to `100`. |
| `gtw_silent` | boolean | When `true`, runs the full UDS sequence against the gateway to suppress its native broadcasts. Defaults to `false`. |

This operation is only valid on GTW 2047 (`0x7FF`) rules with `mux: 3` and `send: true`. For the DBC-defined full-byte `GTW_carConfigMultiplexer`, use `"mux_mask": 255`. The first matching live mux 3 frame seeds the cache after all other operations have been applied. After that, the dashboard loop sends the cached modified frame at its own interval, even if no new GTW mux 3 frame arrives.

##### How `gtw_silent` actually works

GTW runs a UDS server on `0x684` (request) / `0x685` (response). A bare `TesterPresent` loop is not enough: Tesla gates `CommunicationControl` behind an extended diagnostic session **and** SecurityAccess. The firmware therefore drives a small state machine:

1. `0x10 0x03` — DiagnosticSessionControl → ExtendedSession
2. `0x27 0x01` — SecurityAccess requestSeed
3. `0x27 0x02 <key>` — SecurityAccess sendKey (key computed from the seed via `pluginGtwUdsComputeKey`)
4. `0x28 0x01 0x01` — CommunicationControl disable-Tx
5. `0x3E 0x00` — TesterPresent, repeated every 2 s to keep the session alive

Positive responses (`SID + 0x40`) advance the state. Negative responses (`0x7F <SID> <NRC>`) surface the NRC: `0x78 responsePending` extends the wait, anything else fails the sequence and schedules a retry after the back-off window. When the CAN filter is narrowed, `0x684` and `0x685` are automatically added so responses reach the state machine.

> **Key requirement.** Tesla's SecurityAccess seed → key algorithm is proprietary and is **not** included in this repository. Without `PLUGIN_GTW_UDS_CUSTOM_KEY`, `gtw_silent: true` is parsed as disabled: the periodic emit still works, but the firmware does not send the gateway UDS silencing sequence or add `0x684`/`0x685` filters. To make `gtw_silent` actually silence GTW, define `PLUGIN_GTW_UDS_CUSTOM_KEY` at build time and supply a working `pluginGtwUdsComputeKey` implementation.
>
> **HW3 EU TSCLL lesson.** In the MITM variant where TSCLL-style MCU-side config-state spoofing is used, an MCU restart after enabling `TSCLL-Activation.json` is treated as evidence that the MCU accepted/applied the changed config-state. For Track Mode car-config spoofing, `GTW_performancePackage` must be `1=PERFORMANCE`; `3` decodes as `BASE_PLUS`.

#### `checksum` — Recompute the vehicle checksum

```json
{ "type": "checksum" }
```

Computes the standard vehicle checksum and writes it to byte 7:
`byte[7] = (CAN_ID_low + CAN_ID_high + byte[0] + ... + byte[6]) & 0xFF`

Always place this as the **last** operation if the frame uses checksums.

## Limits

| Resource | Limit |
|----------|-------|
| Max plugins installed | 8 |
| Max rules per plugin | 16 |
| Max operations per rule | 16 |
| Max filter IDs per plugin | 32 |

## Examples

> **Note:** The following examples are for illustration purposes only and do not represent real, tested functionality. They demonstrate the plugin JSON syntax and available operations.

### Dashboard feature replacement examples

Example JSON files that match the dashboard features removed from the main Features card are stored in: https://github.com/ev-open-can-tools/ev-open-can-tools-plugins 

Use only the files that match your hardware and intended behavior. The firmware supports at most 8 installed plugins at a time.

### 0x370 duplicate-counter example

This rule listens for CAN ID `0x370` (`880`) and only matches frames where byte 4 bits 7:6 are both clear. It forces byte 3 to `0xB6`, sets byte 4 bit 6, increments the lower nibble of byte 6, recomputes byte 7 checksum, and sends the modified frame immediately.

```json
{
  "name": "0x370 Duplicate Counter",
  "version": "1.0",
  "rules": [
    {
      "id": 880,
      "match_byte": 4,
      "match_mask": 192,
      "match_val": 0,
      "ops": [
        { "type": "set_byte", "byte": 3, "val": 182 },
        { "type": "set_bit", "bit": 38, "val": 1 },
        { "type": "counter", "byte": 6, "mask": 15, "step": 1 },
        { "type": "checksum" }
      ],
      "send": true
    }
  ]
}
```

## Installing plugins

### Via URL (requires WiFi internet)

1. Open the dashboard at `192.168.4.1`
2. Scroll to the **Plugins** card
3. Click **Scan** to find available WiFi networks, then select yours — or type the SSID manually
4. Enter the WiFi password and click **Connect**
5. Optionally expand **Static IP** to configure a fixed IP address, gateway, subnet mask and DNS
6. Wait for the "Connected" status
7. Paste the plugin URL (e.g. a GitHub raw link) and click **Install**

### Via file upload

1. Download the plugin `.json` file to your phone or laptop
2. Open the dashboard at `192.168.4.1`
3. Scroll to the **Plugins** card
4. Click **Upload .json** and select the file

### Via paste JSON (offline)

No internet, no file picker — works completely offline:

1. Copy the plugin JSON content to your clipboard
2. Open the dashboard at `192.168.4.1`
3. Scroll to the **Plugins** card
4. Paste the JSON into the **Paste JSON (offline)** textarea
5. Click **Install from JSON**

The JSON is validated client-side before sending. If the JSON is invalid, an error message is shown immediately.

### Managing plugins

- **Enable/Disable**: Toggle the switch next to each plugin
- **Priority**: Use the priority selector next to each plugin to choose which plugin wins overlapping bit writes. `#1` is evaluated first.
- **Remove**: Click the **X** button
- Plugins persist across reboots (stored on SPIFFS)
- Enabled/disabled state and priority order are preserved

## Hosting plugins

Host your plugin JSON file anywhere accessible via HTTP/HTTPS:

- **GitHub**: Push the `.json` file to a repo and use the raw URL:
  `https://raw.githubusercontent.com/user/repo/main/my-plugin.json`
- **GitHub Gist**: Create a gist and use the raw URL
- **Any web server**: Just serve the `.json` file with the correct content type

## Plugin detail view

Click on any installed plugin name in the dashboard to expand its detail view. This shows:

- **CAN IDs** targeted by each rule (hex and decimal)
- **Bus pin and mux value/mask** if the rule is bus- or mux-specific
- **Operations** listed in execution order (e.g. `set_bit(46, true)`, `counter(0, mask=0xf, step=1)`, `emit_periodic(100 ms)`, `checksum(byte 7)`)

This lets you inspect exactly what a plugin does before enabling it.

## Conflict detection

When two enabled plugins target the same bit on the same CAN ID and mux, the dashboard shows a **Priority overlap** warning. The lower-priority plugin's overlapping bit is ignored at runtime, and the detail view shows which higher-priority plugin wins.

## Important notes

- Dashboard builds do not inject CAN frames from built-in handlers; enabled plugins are the automatic injection path.
- Enabled plugin rules for the same CAN ID, bus, and mux are merged into one injected frame per incoming frame; GTW 2047 can be repeated by the configured plugin replay count, and GTW mux 3 can also be kept alive with `emit_periodic`.
- If two plugins write the same bit, the lower-priority plugin's write is ignored for that bit. Default priority is install order, with the first installed plugin at `#1`.
- Plugin-required CAN IDs are automatically added to the hardware filter list.
- Rule Test is a manual dashboard action that sends the preview frame only when you start it.
- The ESP32 must be connected to the CAN bus for plugin rules to take effect.
- Incorrect CAN modifications can cause dangerous vehicle behavior. Test plugins carefully on a bench setup before using them in a vehicle.

## CAN ID reference

Common Tesla CAN IDs for reference:

| ID (dec) | ID (hex) | Name | Description |
|----------|----------|------|-------------|
| 69 | 0x045 | STW_ACTN_RQ | Steering wheel action request |
| 297 | 0x129 | — | Steering angle |
| 373 | 0x175 | — | Vehicle speed |
| 390 | 0x186 | — | Gear / drive state |
| 599 | 0x257 | — | State of charge |
| 659 | 0x293 | — | DAS control |
| 787 | 0x313 | EPAS_sysStatus | EPS system status |
| 801 | 0x321 | — | Autopilot state |
| 809 | 0x329 | UI_autopilot | UI autopilot info |
| 880 | 0x370 | EPAS3P_sysStatus | Hands-on-wheel nag |
| 921 | 0x399 | DAS_status | DAS status |
| 1000 | 0x3E8 | UI_driverAssistControl | Driver assist control |
| 1006 | 0x3EE | — | Legacy autopilot control |
| 1016 | 0x3F8 | DAS_steeringControl | Steering control (follow dist) |
| 1021 | 0x3FD | UI_autopilotControl | Autopilot control (mux 0/1/2) |
| 2047 | 0x7FF | GTW_autopilot | Gateway autopilot state |
