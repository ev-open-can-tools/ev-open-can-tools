# Community Wishlist

A place to collect mods we want to see. If you have ideas or know how to implement something on the list — open a PR or drop a comment in Discussions.

---

## Adaptive Suspension Manual Override (M3 Performance)

**Hardware:** Model 3 Performance (Highland), HW4  
**Status:** 🔬 Research needed

The adaptive suspension logic on the M3 Performance makes poor decisions — Sport engages at the wrong times, and there's no reliable way to lock it to a specific mode.

**What we want:** Force a specific suspension setting (e.g. always Comfort).

**What we tried:** Modifying `UI_adaptiveRideRequest` bits[4:3] in frame 0x297 (ID 663, `UI_suspensionControl`). Values: 0=COMFORT, 1=AUTO, 2=SPORT, 3=ADVANCED. The frame uses a counter in byte[6] lower nibble and a Tesla checksum in byte[7].

**Result:** No visible effect. Possible reasons:
- Frame 0x297 may not exist or be structured differently on Highland
- The correct frame might be elsewhere (chassis CAN?)
- Checksum or counter logic incorrect

If you know which frame/signal actually controls adaptive suspension on Highland/HW4, please share.

---

## More ideas welcome

Open a Discussion or PR to add to this list.
