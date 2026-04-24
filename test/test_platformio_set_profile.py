import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PlatformioSetProfileTest(unittest.TestCase):
    def test_missing_optional_define_is_added(self) -> None:
        source = ROOT / "platformio_profile.h"
        with tempfile.TemporaryDirectory() as temp_dir:
            profile = Path(temp_dir) / "platformio_profile.h"
            profile.write_text(
                "\n".join(
                    line
                    for line in source.read_text(encoding="utf-8").splitlines()
                    if "INJECTION_AFTER_AP" not in line
                )
                + "\n",
                encoding="utf-8",
            )

            subprocess.run(
                [
                    "python3",
                    str(ROOT / "scripts" / "platformio_set_profile.py"),
                    "--driver",
                    "DRIVER_TWAI",
                    "--vehicle",
                    "HW3",
                    "--enable",
                    "INJECTION_AFTER_AP",
                    "--file",
                    str(profile),
                ],
                cwd=ROOT,
                check=True,
                capture_output=True,
                text=True,
            )

            self.assertIn(
                "#define INJECTION_AFTER_AP",
                profile.read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
