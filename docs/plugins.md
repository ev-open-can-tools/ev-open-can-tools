# Plugin System

The plugin system allows you to create and share CAN frame modification rules as JSON files. Plugins are loaded at runtime on the ESP32 — no recompilation needed, and nothing has to be stored in this repository.

## How it works

1. You write a plugin as a `.json` file
2. You host it anywhere (GitHub, your own server, etc.)
3. Users install it via the dashboard — either by entering the URL or uploading the file
4. The ESP32 stores the plugin on SPIFFS and applies the rules to incoming CAN frames

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

Each rule matches incoming CAN frames by ID (and optionally mux index), applies a sequence of operations, and optionally sends the modified frame back on the bus.

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | integer | yes | CAN frame ID to match (decimal, e.g. `921` = `0x399`). |
| `mux` | integer | no | Mux index to match (bits 0-2 of byte 0). `-1` or omit to match any mux. |
| `ops` | array | yes | Array of operations to apply (see below). |
| `send` | boolean | no | Send the modified frame on the CAN bus. Defaults to `true`. |

### Operations

Operations are applied in order on a **copy** of the original frame. The built-in handler processes frames first — plugin rules run after.

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
| Max operations per rule | 8 |

## Examples

> **Note:** The following examples are for illustration purposes only and do not represent real, tested functionality. They demonstrate the plugin JSON syntax and available operations.

### Example 1: ISA speed chime suppression

Suppresses the ISA speed warning chime on HW4 by setting bit 13 on CAN ID 921 (DAS_status).

```json
{
  "name": "ISA Chime Suppress",
  "version": "1.0",
  "author": "Community",
  "rules": [
    {
      "id": 921,
      "ops": [
        { "type": "or_byte", "byte": 1, "val": 32 },
        { "type": "checksum" }
      ]
    }
  ]
}
```

### Example 2: Track mode request

Sets the track mode request bit on CAN ID 787 (EPAS_sysStatus).

```json
{
  "name": "Track Mode",
  "version": "1.0",
  "rules": [
    {
      "id": 787,
      "ops": [
        { "type": "set_byte", "byte": 0, "val": 1, "mask": 3 },
        { "type": "checksum" }
      ]
    }
  ]
}
```

### Example 3: Mux-specific modification

Modifies only mux 0 of CAN ID 1021 (UI_autopilotControl) — sets bit 46 to enable AD.

```json
{
  "name": "AD Enable Mux0",
  "version": "1.0",
  "rules": [
    {
      "id": 1021,
      "mux": 0,
      "ops": [
        { "type": "set_bit", "bit": 46, "val": 1 }
      ]
    }
  ]
}
```

### Example 4: Multi-rule plugin

A plugin can contain multiple rules that target different CAN IDs or mux values.

```json
{
  "name": "Custom Profile",
  "version": "2.0",
  "author": "Dev",
  "rules": [
    {
      "id": 1021,
      "mux": 0,
      "ops": [
        { "type": "set_bit", "bit": 46, "val": 1 }
      ]
    },
    {
      "id": 1021,
      "mux": 1,
      "ops": [
        { "type": "set_bit", "bit": 19, "val": 0 },
        { "type": "set_bit", "bit": 46, "val": 1 }
      ]
    },
    {
      "id": 921,
      "ops": [
        { "type": "or_byte", "byte": 1, "val": 32 },
        { "type": "checksum" }
      ]
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
- **Remove**: Click the **X** button
- Plugins persist across reboots (stored on SPIFFS)
- Enabled/disabled state is preserved

## Hosting plugins

Host your plugin JSON file anywhere accessible via HTTP/HTTPS:

- **GitHub**: Push the `.json` file to a repo and use the raw URL:
  `https://raw.githubusercontent.com/user/repo/main/my-plugin.json`
- **GitHub Gist**: Create a gist and use the raw URL
- **Any web server**: Just serve the `.json` file with the correct content type

## Plugin detail view

Click on any installed plugin name in the dashboard to expand its detail view. This shows:

- **CAN IDs** targeted by each rule (hex and decimal)
- **Mux value** if the rule is mux-specific
- **Operations** listed in execution order (e.g. `set_bit(46, true)`, `checksum(byte 7)`)

This lets you inspect exactly what a plugin does before enabling it.

## Conflict detection

When a plugin targets a CAN ID that is also handled by the base firmware (e.g. 1021, 787, 880), the dashboard shows:

- A **warning icon** (⚠) next to the plugin name
- Per-rule **"Firmware overlap"** labels in the detail view
- An explanation box: plugin rules run **after** the original handler — both will send modified frames on the bus

This does not prevent the plugin from working. It is an informational warning so you understand that both the firmware and the plugin will independently modify and send frames for the same CAN ID.

## Important notes

- Plugin rules run **after** the built-in handler. If both a plugin and the handler modify the same CAN ID, both modifications are sent.
- Avoid creating plugin rules for CAN IDs that the built-in handler already manages (1016, 1021, 787, 880, 921, 2047) unless you understand the interaction.
- Plugin-required CAN IDs are automatically added to the hardware filter list.
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
