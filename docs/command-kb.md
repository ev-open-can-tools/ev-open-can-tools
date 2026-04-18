# Command Knowledge Base

A place to document CAN commands we've found and tested — including ones that didn't work as expected. If you know what a signal actually does or where we got the bits wrong, please comment or open a PR.

Signal positions are from the DBC corresponding to FW 2026.8.6. Results are from HW4 (Model 3 Highland, Basic Autopilot).

**Checksum formula we used:**

```
byte[7] = (ID_low + ID_high + byte[0] + byte[1] + byte[2] + byte[3] + byte[4] + byte[5] + byte[6]) & 0xFF
```

Where `ID_low = CAN_ID & 0xFF` and `ID_high = (CAN_ID >> 8) & 0xFF`.

Example for 0x238 (ID 568): `ID_low = 0x38`, `ID_high = 0x02`.

Counter in byte[6] lower nibble: we incremented it by 1 each frame, wrapping at 15.

---

## 0x297 — ID 663

**Signal name in DBC:** `UI_adaptiveRideRequest` — bits[4:3] of byte[0]

| bits[4:3] value | Mode |
|-----------------|------|
| 0 | COMFORT |
| 1 | AUTO |
| 2 | SPORT |
| 3 | ADVANCED |

**Counter:** byte[6] lower nibble, incremented each frame  
**Checksum:** byte[7] per formula above

**What we did:** Set bits[4:3] of byte[0] to 0 (COMFORT). Incremented counter, recalculated checksum per formula above.

**Result:** No visible effect.

**Questions for the community:**
- Does this frame exist on Highland (2024+)?
- Does anyone have a CAN log showing this frame changing when switching suspension modes manually?

---

## 0x238 — ID 568 — Reject Bit Overrides

**Counter:** byte[6] lower nibble  
**Checksum:** byte[7] per formula above

We set the following bits to 0 individually. Signal names from DBC:

### bit 40 — `UI_rejectLeftLaneChange`

Set to 0. No visible result.

### bit 41 — `UI_rejectRightLaneChange`

Set to 0. No visible result.

### bit 43 — `UI_rejectNavLaneChange`

Set to 0. No visible result.

### bit 47 — `UI_rejectHandsOnLaneChange`

Set to 0. No visible result.

**Questions for the community:**
- Are these bits actually read by DAS on HW4?
- Are the bit positions correct for 2026 firmware?
- Has anyone seen these bits change in a live CAN log?

---

## 0x238 — ID 568 — Autosteer Road Restrictions

Signal names from DBC. Counter and checksum same as above.

### `UI_roadClass` — bits[5:3] of byte[0]

Set bits[5:3] to value 1 using `set_byte byte=0 val=8 mask=56`. No visible result.

### `UI_controlledAccess` — bit 37

Set to 1 using `set_bit bit=37 val=1`. No visible result.

### `UI_rejectAutosteer` — bit 46

Set to 0 using `set_bit bit=46 val=0`. No visible result.

### `UI_autosteerRestricted` — bit 49

Set to 0 using `set_bit bit=49 val=0`. No visible result.

**Note:** Country Spoof (same frame, byte[2-3]) does seem to get processed — Tesla checksum on this frame appears to work with our formula. So if these bits had no effect, it may be that the signal positions in the DBC don't match our FW version.

---

## 0x238 — ID 568 — Country Spoof

### byte[2] — `UI_countryCode` lower byte

Set byte[2] = 0x9A (154) using `set_byte byte=2 val=154 mask=255`.

### byte[3] — `UI_countryCode` upper bits

Set bits[1:0] of byte[3] = 0x01 using `set_byte byte=3 val=1 mask=3`.

Together these encode country code 410 (Korea) in bits[9:0] across bytes 2–3.

**Result:** Unknown — untested in isolation. Combined as one plugin with checksum, no confirmed visual result yet.

---

## 0x3F8 — ID 1016 — LC Stalk Confirm

**Signal:** `UI_ulcStalkConfirm` — bit 1

Set to 0 using `set_bit bit=1 val=0`.

**Result:** Untested.

**Questions:**
- Is bit 1 the correct position for this signal on Highland/HW4?
- What should change visually when this works?

---

## 0x3F8 — ID 1016 — Hands-On Requirement

**Signal:** `UI_handsOnRequirementDisable` — bit 14

Set to 1 using `set_bit bit=14 val=1`.

**Result:** Untested.

**Questions:**
- Is bit 14 correct on HW4?
- Is this enforced server-side or only via CAN?

---

## 0x3F8 — ID 1016 — ULC Speed Config

**Signal:** `UI_ulcSpeedConfig` — bits[3:2] of byte[6]

Applied `and_byte byte=6 val=0xF3` to clear bits 2–3.

**Result:** Untested as isolated change. Note: with ULC enabled the car already overtakes on its own — unclear if this bit changes that behavior or controls something else.

**Questions:**
- What does `UI_ulcSpeedConfig` actually control in practice?

---

## Contributing

If you've tested any of these and got a result — or know the correct bit positions — please open a Discussion or PR. Even negative results are useful.
