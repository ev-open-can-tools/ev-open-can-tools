# Newcomer onboarding

The project publishes a standalone guided onboarding journey at [GitHub Pages](../onboarding/).

It is designed for people who are new to EV Open CAN and do not yet know which documentation, board, vehicle mode, harness, or first-start sequence applies to them. The page presents one decision at a time and creates a personal starting plan.

## What the journey covers

1. Choose a goal: observation, future plugin use, SavvyCAN logging, or bench research.
2. Select the Tesla model, build year, and confirmed Legacy, HW3, or HW4 vehicle mode.
3. Compare beginner-friendly integrated CAN boards with the advanced custom ESP32 path.
4. Build a parts checklist before opening vehicle trim.
5. View an annotated installation-area illustration and the areas that must remain clear.
6. Generate the matching PlatformIO environment and build commands.
7. Follow the safe bench-power, hotspot, dashboard, and CAN-observation order.
8. Download or print a personalized checklist.

The page stores progress only in browser `localStorage`. It does not contact an ESP32 or submit information to an external service.

## Installation visuals

The placement step uses annotated illustrations to show a general center-console or front-footwell installation area for Model 3 and Model Y, a model-specific warning for Model S or X, and a separate isolated bench layout.

These visuals are intentionally not connector photos or pinouts. Connectors and network assignments can differ by model, factory, and build date. Always use the exact Tesla service or electrical documentation to verify Party CAN, CAN H and CAN L, voltage, polarity, termination, and bitrate before connecting anything.

## Scope

This onboarding exists only on GitHub Pages. It is not part of the firmware and does not change the local dashboard served by the ESP32.

The page cannot configure WiFi, change CAN pins, install plugins, enable injection, or modify a real device. The ESP32 dashboard appears near the end only as part of the first-start instructions.
