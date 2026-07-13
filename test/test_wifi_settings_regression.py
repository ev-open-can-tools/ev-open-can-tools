import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI_FILE = ROOT / "include" / "web" / "mcp2515_dashboard_ui.h"
DASH_FILE = ROOT / "include" / "web" / "mcp2515_dashboard.h"
SYNC_FILE = ROOT / "scripts" / "platformio_sync_profile.py"
RUNTIME_FILE = ROOT / "src" / "espidf_runtime.cpp"


class WifiSettingsRegressionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ui = UI_FILE.read_text(encoding="utf-8")
        cls.dash = DASH_FILE.read_text(encoding="utf-8")
        cls.sync = SYNC_FILE.read_text(encoding="utf-8")
        cls.runtime = RUNTIME_FILE.read_text(encoding="utf-8")

    def assertHasUiId(self, element_id: str) -> None:
        pattern = rf'\bid=(?:"{re.escape(element_id)}"|{re.escape(element_id)}\b)'
        self.assertRegex(self.ui, pattern)

    def test_wifi_ui_has_all_expected_fields(self) -> None:
        required_ids = [
            "wifi-status",
            "wifi-ssid",
            "wifi-pass",
            "wifi-static",
            "wifi-ip",
            "wifi-gw",
            "wifi-mask",
            "wifi-dns",
            "wifi-nets",
            "scan-btn",
        ]

        for element_id in required_ids:
            with self.subTest(element_id=element_id):
                self.assertHasUiId(element_id)

    def test_wifi_ui_scripts_reference_existing_elements(self) -> None:
        referenced_ids = {
            match.group(1)
            for match in re.finditer(r"\$\('([^']+)'\)", self.ui)
            if match.group(1).startswith("wifi-")
        }
        declared_ids = set()
        for match in re.finditer(r'\bid=(?:"([^"]+)"|([A-Za-z0-9_-]+))', self.ui):
            element_id = match.group(1) or match.group(2)
            if element_id.startswith("wifi-"):
                declared_ids.add(element_id)

        missing = sorted(referenced_ids - declared_ids)
        self.assertEqual([], missing, f"Missing WiFi UI ids: {missing}")

    def test_wifi_backend_exposes_routes_and_preference_keys(self) -> None:
        required_routes = ["/wifi_scan", "/wifi_config", "/wifi_status"]
        required_pref_keys = [
            "wifi_ssid",
            "wifi_pass",
            "wifi_static",
            "wifi_ip",
            "wifi_gw",
            "wifi_mask",
            "wifi_dns",
        ]

        for route in required_routes:
            with self.subTest(route=route):
                self.assertIn(route, self.dash)

        for key in required_pref_keys:
            with self.subTest(pref_key=key):
                self.assertIn(f'"{key}"', self.dash)

    def test_wifi_status_payload_covers_connection_and_config_state(self) -> None:
        expected_fields = [
            '\\"connected\\"',
            '\\"ssid\\"',
            '\\"stored\\"',
            '\\"ip\\"',
            '\\"static\\"',
            '\\"cfg_ip\\"',
            '\\"cfg_gw\\"',
            '\\"cfg_mask\\"',
            '\\"cfg_dns\\"',
            '\\"connecting\\"',
        ]

        status_section = self.dash[self.dash.index("static void handleWifiStatus()") :]
        for field in expected_fields:
            with self.subTest(field=field):
                self.assertIn(field, status_section)

    def test_wifi_settings_backup_roundtrip_fields_exist(self) -> None:
        expected_export_import_fields = [
            '\\"wifi\\":{\\"networks\\":[',
            'doc["wifi"].is<JsonObject>()',
            'JsonArray networks = wifi["networks"]',
            'item["ssid"]',
            'item["pass"]',
            'item["static"]',
            'item["ip"]',
            'item["gw"]',
            'item["mask"]',
            'item["dns"]',
            '\\"dashboard\\":{\\"hw\\"',
            '\\"autoUpdate\\"',
            'doc["dashboard"].is<JsonObject>()',
            'doc["autoUpdate"]',
        ]

        for field in expected_export_import_fields:
            with self.subTest(field=field):
                self.assertIn(field, self.dash)

    def test_wifi_config_defers_reconnect_until_after_response(self) -> None:
        section = self.dash[
            self.dash.index("static void handleWifiConfig()") :
            self.dash.index("static void handleWifiDelete()")
        ]

        self.assertIn("dashPrepareStaReconnect();", section)
        self.assertIn("dashScheduleSTAConnect(1000);", section)
        self.assertNotIn("dashConnectSTA();", section)
        success_send = 'server.send(200, "application/json", "{\\"ok\\":true,\\"idx\\":"'
        self.assertLess(section.index(success_send), section.index("dashScheduleSTAConnect(1000);"))

    def test_wifi_scan_prepares_sta_mode_before_scan(self) -> None:
        section = self.dash[
            self.dash.index("static void handleWifiScan()") :
            self.dash.index("static void dashPersistWifiSlot")
        ]

        self.assertIn("dashPrepareWifiScan();", section)
        self.assertLess(section.index("dashPrepareWifiScan();"), section.index("WiFi.scanNetworks"))

    def test_espidf_wifi_logging_is_not_info_verbose(self) -> None:
        self.assertIn('esp_log_level_set("wifi", ESP_LOG_WARN);', self.runtime)
        self.assertIn('esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);', self.runtime)

    def test_ap_injection_gate_setting_is_persisted_and_exposed(self) -> None:
        expected_ui_ids = ["ap-gate-tgl"]
        expected_ui_fields = ["saveApGate()", "updateApGateControl(d)"]

        for element_id in expected_ui_ids:
            with self.subTest(ui_id=element_id):
                self.assertHasUiId(element_id)
        expected_backend_fields = [
            "INJECTION_AFTER_AP",
            '"ap_gate"',
            'server.hasArg("apg")',
            '\\"apGate\\"',
            '\\"ia\\"',
            'dashInjectionActive()',
            'doc["plugins"]["startAfterAp"]',
        ]

        for field in expected_ui_fields:
            with self.subTest(ui_field=field):
                self.assertIn(field, self.ui)

        for field in expected_backend_fields:
            with self.subTest(backend_field=field):
                self.assertIn(field, self.dash)

        self.assertIn("INJECTION_AFTER_AP", self.sync)

    def test_summon_only_setting_defaults_disabled_and_roundtrips_reboot(self) -> None:
        self.assertHasUiId("summon-only-tgl")
        for ui_field in ("Summon-only injection (beta)", "c.summonOnly", "smo:"):
            with self.subTest(ui_field=ui_field):
                self.assertIn(ui_field, self.ui)

        for backend_field in (
            "static Shared<bool> summonOnlyInjection{false}",
            'prefs.putBool("sum_only", summonOnlyInjection)',
            'prefs.getBool("sum_only", false)',
            'server.hasArg("smo")',
            '\"summonOnly\"',
            'doc["plugins"]["summonOnly"]',
            'p.putBool("sum_only"',
        ):
            with self.subTest(backend_field=backend_field):
                self.assertIn(backend_field, self.dash)

        self.assertIn("pluginResetPeriodicEmit()", self.dash)
        self.assertIn("clearPendingTransmit()", self.dash)

    def test_manual_ota_credentials_can_be_reset_from_dashboard(self) -> None:
        self.assertHasUiId("ota-reset-btn")
        self.assertIn("resetOtaCredentials()", self.ui)
        self.assertRegex(self.ui, r"localStorage\.removeItem\([\"']otaU[\"']\)")
        self.assertRegex(self.ui, r"localStorage\.removeItem\([\"']otaP[\"']\)")

    def test_each_espidf_environment_has_its_release_artifact(self) -> None:
        expected = {
            "esp32_twai": "firmware-esp32.bin",
            "esp32s2_twai": "firmware-esp32s2.bin",
            "esp32c6_twai": "firmware-esp32c6.bin",
            "lilygo_tcan485_hw3": "firmware-lilygo-tcan485-hw3.bin",
            "m5stack-atomic-can-base": "firmware-m5stack.bin",
            "m5stack-atoms3-mini-can-base": "firmware-m5stack-atoms3-mini.bin",
            "esp32_feather_v2_mcp2515": "firmware-esp32-featherwing.bin",
            "esp32_ext_mcp2515": "firmware-esp32-ext-mcp2515.bin",
            "waveshare_ESP32_S3_RS485_CAN": "firmware-waveshare-esp32-s3.bin",
        }
        for environment, artifact in expected.items():
            with self.subTest(environment=environment):
                self.assertIn(f'"{environment}": "{artifact}"', self.sync)

    def test_espidf_sdkconfig_flash_size_tracks_board(self) -> None:
        self.assertIn("def _sync_sdkconfig_flash_size", self.sync)
        self.assertIn('BoardConfig().get("upload.flash_size", "4MB")', self.sync)
        self.assertIn("CONFIG_ESPTOOLPY_FLASHSIZE_", self.sync)


if __name__ == "__main__":
    unittest.main()
