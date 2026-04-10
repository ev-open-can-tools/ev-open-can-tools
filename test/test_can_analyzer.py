import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import ev_can_analyzer as analyzer


class EvCanAnalyzerTests(unittest.TestCase):
    def test_parse_compact_candump_line(self) -> None:
        frame = analyzer.parse_log_line(
            "(1712345678.123456) can0 7FF#02000000000C0000",
            line_number=1,
        )
        assert frame is not None
        self.assertEqual("1712345678.123456", frame.timestamp)
        self.assertEqual("can0", frame.channel)
        self.assertEqual(0x7FF, frame.can_id)
        self.assertEqual(bytes.fromhex("02000000000C0000"), frame.data)

    def test_parse_spaced_candump_line(self) -> None:
        frame = analyzer.parse_log_line(
            "1712345678.200000 can0 7FF [8] 02 00 00 00 00 08 00 00",
            line_number=2,
        )
        assert frame is not None
        self.assertEqual("1712345678.200000", frame.timestamp)
        self.assertEqual(0x7FF, frame.can_id)
        self.assertEqual(bytes.fromhex("0200000000080000"), frame.data)

    def test_decode_gtw_autopilot_for_mux_2(self) -> None:
        frame = analyzer.FrameRecord(
            line_number=1,
            timestamp="1.0",
            channel="can0",
            can_id=0x7FF,
            data=bytes.fromhex("02000000000C0000"),
            original_line="",
        )
        observation = analyzer.decode_gtw_car_config(frame)

        self.assertEqual(2, observation.mux_id)
        self.assertEqual(3, observation.fields["GTW_autopilot"]["value"])
        self.assertEqual("SELF_DRIVING", observation.fields["GTW_autopilot"]["label"])

    def test_detect_gtw_autopilot_change(self) -> None:
        frames = [
            analyzer.FrameRecord(
                line_number=1,
                timestamp="1.0",
                channel="can0",
                can_id=0x7FF,
                data=bytes.fromhex("02000000000C0000"),
                original_line="",
            ),
            analyzer.FrameRecord(
                line_number=2,
                timestamp="2.0",
                channel="can0",
                can_id=0x7FF,
                data=bytes.fromhex("0200000000080000"),
                original_line="",
            ),
        ]

        changes = analyzer.detect_changes(analyzer.decode_observations(frames))

        self.assertEqual(1, len(changes))
        self.assertEqual("GTW_autopilot", changes[0]["field"])
        self.assertEqual("3 -> 2", changes[0]["transition"])
        self.assertIn("GTW_autopilot changed: 3 -> 2", changes[0]["explicit_notice"])
        self.assertEqual("02000000000C0000", changes[0]["before"]["raw_payload_hex"])
        self.assertEqual("0200000000080000", changes[0]["after"]["raw_payload_hex"])

    def test_override_frames_are_preserved_with_missing_definition_note(self) -> None:
        frame = analyzer.FrameRecord(
            line_number=1,
            timestamp="3.0",
            channel="can0",
            can_id=0x346,
            data=bytes.fromhex("650A000044332211"),
            original_line="",
        )

        observation = analyzer.decode_gtw_autopilot_override(frame)

        self.assertEqual("GTW_autopilotOverride", observation.message_name)
        self.assertEqual(5, observation.fields["GTW_autopilotOverrideState"]["value"])
        self.assertEqual(
            "OPTION_CODE", observation.fields["GTW_autopilotOverrideState"]["label"]
        )
        self.assertEqual(6, observation.fields["GTW_autopilotConfig"]["value"])
        self.assertEqual("UNKNOWN", observation.fields["GTW_autopilotConfig"]["label"])
        self.assertEqual(2, observation.fields["GTW_autopilotOverrideConfig"]["value"])
        self.assertEqual(
            "ENHANCED", observation.fields["GTW_autopilotOverrideConfig"]["label"]
        )
        self.assertEqual(1, observation.fields["GTW_autopilotConfigSource"]["value"])
        self.assertEqual(
            "AUTOPILOT", observation.fields["GTW_autopilotConfigSource"]["label"]
        )
        self.assertEqual(
            0x11223344, observation.fields["GTW_autopilotOverrideExpireTime"]["value"]
        )
        self.assertIn(
            "does not define vendor-specific gateway write result strings",
            observation.notes[0],
        )

    def test_decode_single_frame_uds_negative_response(self) -> None:
        frame = analyzer.FrameRecord(
            line_number=1,
            timestamp="4.0",
            channel="can0",
            can_id=0x18DAF10A,
            data=bytes.fromhex("037F2E31AAAAAA"),
            original_line="",
        )

        observation = analyzer.decode_diagnostic_frame(frame)

        assert observation is not None
        self.assertEqual(
            "DiagnosticTransport:NegativeResponse", observation.message_name
        )
        self.assertEqual(
            "negative_response", observation.fields["UDS_ResponseType"]["value"]
        )
        self.assertEqual(0x2E, observation.fields["UDS_RequestService"]["value"])
        self.assertEqual(
            "WriteDataByIdentifier", observation.fields["UDS_RequestService"]["label"]
        )
        self.assertEqual(0x31, observation.fields["UDS_NRC"]["value"])
        self.assertEqual("RequestOutOfRange", observation.fields["UDS_NRC"]["label"])

    def test_decode_single_frame_uds_positive_response(self) -> None:
        frame = analyzer.FrameRecord(
            line_number=1,
            timestamp="5.0",
            channel="can0",
            can_id=0x18DA0AF1,
            data=bytes.fromhex("036EF19000000000"),
            original_line="",
        )

        observation = analyzer.decode_diagnostic_frame(frame)

        assert observation is not None
        self.assertEqual(
            "DiagnosticTransport:PositiveResponse", observation.message_name
        )
        self.assertEqual(
            "positive_response", observation.fields["UDS_ResponseType"]["value"]
        )
        self.assertEqual(0x2E, observation.fields["UDS_RequestService"]["value"])
        self.assertEqual(0xF190, observation.fields["UDS_Identifier"]["value"])

    def test_decode_multiframe_diagnostic_transport_header(self) -> None:
        frame = analyzer.FrameRecord(
            line_number=1,
            timestamp="6.0",
            channel="can0",
            can_id=0x7E8,
            data=bytes.fromhex("101462F190010203"),
            original_line="",
        )

        observation = analyzer.decode_diagnostic_frame(frame)

        assert observation is not None
        self.assertEqual("DiagnosticTransport", observation.message_name)
        self.assertEqual(0x1, observation.fields["ISO_TP_PCI"]["value"])
        self.assertEqual(0x14, observation.fields["ISO_TP_TotalLength"]["value"])
        self.assertIn("reassembly is not implemented", observation.notes[0])

    def test_decode_gateway_uds_negative_response_uses_named_message(self) -> None:
        frame = analyzer.FrameRecord(
            line_number=1,
            timestamp="7.0",
            channel="can0",
            can_id=0x685,
            data=bytes.fromhex("037F2E3100000000"),
            original_line="",
        )

        observation = analyzer.decode_diagnostic_frame(frame)

        assert observation is not None
        self.assertEqual("GTW_udsResponse:NegativeResponse", observation.message_name)

    def test_detect_change_in_override_config(self) -> None:
        frames = [
            analyzer.FrameRecord(
                line_number=1,
                timestamp="8.0",
                channel="can0",
                can_id=0x346,
                data=bytes.fromhex("1101000000000000"),
                original_line="",
            ),
            analyzer.FrameRecord(
                line_number=2,
                timestamp="9.0",
                channel="can0",
                can_id=0x346,
                data=bytes.fromhex("2101000000000000"),
                original_line="",
            ),
        ]

        changes = analyzer.detect_changes(analyzer.decode_observations(frames))

        matching = [
            change for change in changes if change["field"] == "GTW_autopilotConfig"
        ]
        self.assertEqual(1, len(matching))
        self.assertEqual("1 -> 2", matching[0]["transition"])


if __name__ == "__main__":
    unittest.main()
