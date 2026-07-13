# Dashboard onboarding

The dashboard includes a lightweight guided setup at `#/onboarding`. It opens automatically on the first visit in a browser and can always be reopened with the **Setup** button in the header.

The wizard configures the vehicle hardware generation, speed profile, AP injection gate, optional internet WiFi, and CAN GPIO pins. It reuses the existing dashboard endpoints and does not add a background task or additional polling.

## Safety behavior

The wizard never enables CAN injection. After setup, injection remains stopped until it is explicitly armed from the main dashboard after live CAN traffic has been validated. Changing CAN GPIO pins requires a device reboot.

The completion marker is stored in browser `localStorage`. This prevents repeated prompts in the same browser without adding another NVS write. Opening `#/onboarding` bypasses the marker. WiFi credentials are submitted directly to the device and are not stored in browser storage by the wizard.
