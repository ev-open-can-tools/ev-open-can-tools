import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "build_pages_onboarding.py"


class PagesOnboardingPreviewTest(unittest.TestCase):
    def test_builds_standalone_interactive_preview(self):
        with tempfile.TemporaryDirectory() as temporary_directory:
            output = Path(temporary_directory) / "onboarding" / "index.html"
            subprocess.run(
                [sys.executable, str(SCRIPT), "--output", str(output)],
                cwd=ROOT,
                check=True,
            )
            page = output.read_text(encoding="utf-8")

        self.assertIn("EV Open CAN onboarding preview", page)
        self.assertIn("Standalone onboarding preview", page)
        self.assertIn("window.__EV_OPEN_CAN_PAGES_PREVIEW__ = true", page)
        self.assertIn("#/onboarding", page)
        self.assertIn("Vehicle profile", page)
        self.assertIn("Simulate scan", page)
        self.assertIn("CAN GPIO pins", page)
        self.assertIn("Injection after setup: Stopped", page)
        self.assertNotIn("window.fetch", page)
        self.assertNotIn("mcp2515_dashboard_ui", SCRIPT.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
