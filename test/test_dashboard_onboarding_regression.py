import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text(encoding="utf-8")


class DashboardOnboardingRegressionTest(unittest.TestCase):
    def test_onboarding_is_reopenable_and_first_visit_aware(self):
        self.assertIn("#/onboarding", SOURCE)
        self.assertIn("openOnboarding(true)", SOURCE)
        self.assertIn("evOpenCanOnboardingV1", SOURCE)

    def test_onboarding_reuses_existing_configuration_endpoints(self):
        for endpoint in ("/config", "/wifi_scan", "/wifi_config", "/can_pins"):
            self.assertIn(endpoint, SOURCE)

    def test_onboarding_does_not_arm_injection(self):
        match = re.search(r"async function saveOnboarding\(\).*?async function initOnboarding", SOURCE, re.DOTALL)
        self.assertIsNotNone(match)
        save_function = match.group(0)
        self.assertNotIn("can=1", save_function)
        self.assertNotIn("resumeInjection", save_function)
        self.assertIn("Injection after setup: Stopped", SOURCE)

    def test_wifi_password_is_not_written_to_browser_storage(self):
        self.assertNotRegex(SOURCE, r"localStorage\.setItem\([^,]+,\s*\$\('onboarding-wifi-pass'\)")


if __name__ == "__main__":
    unittest.main()
