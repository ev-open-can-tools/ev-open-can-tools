import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class LilygoT2CanBuildTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        cls.tests_workflow = (ROOT / ".github/workflows/tests.yml").read_text(
            encoding="utf-8"
        )
        cls.release_workflow = (ROOT / ".github/workflows/release.yml").read_text(
            encoding="utf-8"
        )
        cls.sync = (ROOT / "scripts/platformio_sync_profile.py").read_text(
            encoding="utf-8"
        )

    def test_board_environment_uses_reference_twai_pins(self) -> None:
        environment = self.platformio.split("[env:lilygo_t2can]", 1)[1].split(
            "[env:", 1
        )[0]
        self.assertIn("-DDRIVER_TWAI", environment)
        self.assertIn("-DTWAI_TX_PIN=GPIO_NUM_7", environment)
        self.assertIn("-DTWAI_RX_PIN=GPIO_NUM_6", environment)
        self.assertIn("partitions_16mb_ota_4096k_nvs64.csv", environment)

    def test_automatic_builds_include_board(self) -> None:
        self.assertIn("- env: lilygo_t2can", self.tests_workflow)
        self.assertIn("- env: lilygo_t2can", self.release_workflow)

    def test_release_artifact_is_board_specific(self) -> None:
        mapping = '"lilygo_t2can": "firmware-lilygo-t2can.bin"'
        self.assertIn(mapping, self.sync)
        self.assertIn("firmware-lilygo-t2can.bin", self.release_workflow)


if __name__ == "__main__":
    unittest.main()
