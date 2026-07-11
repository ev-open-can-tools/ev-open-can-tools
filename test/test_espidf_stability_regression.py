import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class EspIdfStabilityRegressionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
        cls.app = (ROOT / "include/app.h").read_text(encoding="utf-8")
        cls.runtime = (ROOT / "include/runtime_diagnostics.h").read_text(encoding="utf-8")
        cls.twai = (ROOT / "include/drivers/twai_driver.h").read_text(encoding="utf-8")
        cls.handlers = (ROOT / "include/handlers.h").read_text(encoding="utf-8")
        cls.dashboard = (ROOT / "include/web/mcp2515_dashboard.h").read_text(encoding="utf-8")
        cls.ui = (ROOT / "include/web/mcp2515_dashboard_ui.src.h").read_text(encoding="utf-8")
        cls.gvret = (ROOT / "include/gvret_serial.h").read_text(encoding="utf-8")

    def test_dashboard_starts_before_twai_and_wake_delay(self) -> None:
        twai_branch = self.main[self.main.index("#elif defined(DRIVER_TWAI)") :]
        self.assertLess(twai_branch.index("mcpDashboardSetup"), twai_branch.index("delay(DRIVER_WAKE_DELAY_MS)"))
        self.assertLess(twai_branch.index("delay(DRIVER_WAKE_DELAY_MS)"), twai_branch.index("appStartDriver<TWAIDriver>"))

    def test_injection_uses_actual_can_time_and_live_frames(self) -> None:
        self.assertIn("DRIVER_WAKE_DELAY_MS = 10000", self.runtime)
        self.assertIn("INJECTION_DELAY_MS = 15000", self.runtime)
        self.assertIn("CAN_LIVE_FRAME_THRESHOLD = 1000", self.runtime)
        self.assertIn("now - initialized >= INJECTION_DELAY_MS", self.runtime)
        self.assertIn("canFrames.load(std::memory_order_relaxed) > CAN_LIVE_FRAME_THRESHOLD", self.runtime)
        self.assertIn("appDriver->allowSendFrame = appCanTransmitAllowed", self.app)

    def test_twai_recovery_and_state_contract(self) -> None:
        self.assertIn("twai_initiate_recovery()", self.twai)
        self.assertIn("twai_transmit(&msg, pdMS_TO_TICKS(2))", self.twai)
        self.assertIn("now - lastTxFailLogMs_ >= 2000", self.twai)
        for state in ("TWAI_STATE_STOPPED", "TWAI_STATE_RUNNING", "TWAI_STATE_BUS_OFF", "TWAI_STATE_RECOVERING"):
            self.assertIn(state, self.twai)

    def test_nag_hard_limits_and_full_torque_compare(self) -> None:
        self.assertIn("TORQUE_NM_MAX = +1.80f", self.handlers)
        self.assertIn("TORQUE_NM_MIN = -1.80f", self.handlers)
        self.assertIn("TORQUE_RAW_MAX = 0x08B6", self.handlers)
        self.assertIn("TORQUE_RAW_MIN = 0x074E", self.handlers)
        self.assertIn("torqueRaw == TORQUE_RAW_MAX", self.handlers)
        self.assertIn("frame.dlc < 8", self.handlers)

    def test_only_last_write_check_remains_from_removed_diagnostics(self) -> None:
        self.assertIn("Last Write Check", self.ui)
        for removed in ("CAN Sniffer", "CAN Recorder", "CAN Controller", "Live Log", "Rule Test", "Plugin Editor"):
            self.assertNotIn(removed, self.ui)
        for route in ('"/frames"', '"/log"', '"/rec_start"', '"/plugin_test"', '"/reset_stats"'):
            self.assertNotIn(route, self.dashboard)

    def test_gvret_is_bounded_read_only_and_nonblocking(self) -> None:
        self.assertIn("kMaxRunMs = 10UL * 60UL * 1000UL", self.gvret)
        self.assertIn("kClientIdleMs = 15000", self.gvret)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(10))", self.gvret)
        self.assertNotIn("twai_transmit", self.gvret)
        self.assertNotIn("sendMessage", self.gvret)


if __name__ == "__main__":
    unittest.main()
