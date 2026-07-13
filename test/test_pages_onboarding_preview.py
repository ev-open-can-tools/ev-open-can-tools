import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "build_pages_onboarding.py"


class PagesOnboardingPreviewTest(unittest.TestCase):
    def test_builds_newcomer_onboarding_journey(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "onboarding" / "index.html"
            subprocess.run(
                [sys.executable, str(SCRIPT), "--output", str(output)],
                cwd=ROOT,
                check=True,
            )
            page = output.read_text(encoding="utf-8")

        self.assertIn("Start without the information overload", page)
        self.assertIn("Choose the physical device", page)
        self.assertIn("Understand where the device belongs", page)
        self.assertIn("Bring it online in the safest order", page)
        self.assertIn("Your personal starting plan", page)
        self.assertIn("M5Stack AtomS3 Mini CAN Base", page)
        self.assertIn("Waveshare ESP32-S3 RS485/CAN", page)
        self.assertIn("m5stack-atoms3-mini-can-base", page)
        self.assertIn("Model 3 electrical reference", page)
        self.assertIn("https://ev-open-can-tools.github.io/ev-open-can-tools/docs/nag-killer.html", page)
        self.assertIn("EV OPEN CAN NEWCOMER PLAN", page)
        self.assertIn("downloadPlan()", page)
        self.assertIn("evOpenCanNewcomerPlanV2", page)
        self.assertIn("window.__EV_OPEN_CAN_NEWCOMER_ONBOARDING__=true", page)
        self.assertIn("The illustration is not a pinout", page)
        self.assertNotIn("Simulate scan", page)
        self.assertNotIn("CAN GPIO pins", page)
        self.assertNotIn("window.fetch", page)
        self.assertNotIn("mcp2515_dashboard_ui", SCRIPT.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
