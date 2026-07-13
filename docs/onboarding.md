# GitHub Pages onboarding preview

The project publishes a standalone interactive onboarding preview at [GitHub Pages](../onboarding/).

The preview demonstrates a possible guided setup flow for vehicle hardware selection, speed profile, AP injection gate, optional WiFi, CAN GPIO pins, and a final safety review. All values are simulated in the browser.

## Scope

This onboarding exists only on GitHub Pages. It is not included in the ESP32 firmware and does not change the local dashboard served by the device.

The page cannot configure a real ESP32, submit WiFi credentials, change CAN pins, or enable injection. For real configuration, connect to the device hotspot and use the existing local dashboard controls directly.

## Safety behavior

The preview always shows CAN injection as stopped. It requires a safety acknowledgement before continuing and validates the example GPIO values, but it cannot verify real hardware, wiring, CAN traffic, counters, checksums, or vehicle compatibility.
