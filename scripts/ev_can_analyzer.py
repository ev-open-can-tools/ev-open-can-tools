#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable


GTW_AUTOPILOT_ENUM = {
    0: "NONE",
    1: "HIGHWAY",
    2: "ENHANCED",
    3: "SELF_DRIVING",
    4: "BASIC",
}
GTW_AUTOPILOT_OVERRIDE_STATE_ENUM = {
    0: "BASE",
    1: "SUBSCRIPTION",
    2: "TRIAL",
    3: "TIMEBOUND_SUBSCRIPTION",
    4: "TIMEBOUND_TRIAL",
    5: "OPTION_CODE",
    6: "OPTION_OVERRIDE",
    7: "VEHICLE_MANAGED",
    8: "UNKNOWN",
}
GTW_AUTOPILOT_CONFIG_SOURCE_ENUM = {
    0: "GATEWAY",
    1: "AUTOPILOT",
    2: "AUTOPILOT_NOT_UPDATED",
}

UDS_SERVICE_NAMES = {
    0x10: "DiagnosticSessionControl",
    0x11: "ECUReset",
    0x22: "ReadDataByIdentifier",
    0x23: "ReadMemoryByAddress",
    0x27: "SecurityAccess",
    0x2E: "WriteDataByIdentifier",
    0x2F: "InputOutputControlByIdentifier",
    0x31: "RoutineControl",
    0x3E: "TesterPresent",
}
UDS_NEGATIVE_RESPONSE_CODES = {
    0x10: "GeneralReject",
    0x11: "ServiceNotSupported",
    0x12: "SubFunctionNotSupported",
    0x13: "IncorrectMessageLengthOrInvalidFormat",
    0x21: "BusyRepeatRequest",
    0x22: "ConditionsNotCorrect",
    0x24: "RequestSequenceError",
    0x31: "RequestOutOfRange",
    0x33: "SecurityAccessDenied",
    0x35: "InvalidKey",
    0x36: "ExceededNumberOfAttempts",
    0x37: "RequiredTimeDelayNotExpired",
    0x70: "UploadDownloadNotAccepted",
    0x71: "TransferDataSuspended",
    0x72: "GeneralProgrammingFailure",
    0x73: "WrongBlockSequenceCounter",
    0x78: "ResponsePending",
    0x7E: "SubFunctionNotSupportedInActiveSession",
    0x7F: "ServiceNotSupportedInActiveSession",
}

OPTIONAL_OVERRIDE_FIELDS = (
    "GTW_autopilotConfig",
    "GTW_autopilotOverrideConfig",
    "GTW_autopilotConfigSource",
    "GTW_autopilotOverrideState",
)
OPTIONAL_OVERRIDE_NOTE = (
    "The DBC data defines GTW_autopilotOverride on 0x346, but the repo data "
    "does not define vendor-specific gateway write result strings such as "
    "SIGNATURE_INVALID or INSECURE_OK."
)

HEX_ID_RE = r"(?:0x)?[0-9A-Fa-f]{1,8}"
COMPACT_PATTERNS = (
    re.compile(
        rf"^\((?P<timestamp>[^)]+)\)\s+(?P<channel>\S+)\s+"
        rf"(?P<can_id>{HEX_ID_RE})#(?P<data>[0-9A-Fa-f]*)\s*$"
    ),
    re.compile(
        rf"^(?P<timestamp>\d+(?:\.\d+)?)\s+(?P<channel>\S+)\s+"
        rf"(?P<can_id>{HEX_ID_RE})#(?P<data>[0-9A-Fa-f]*)\s*$"
    ),
    re.compile(
        rf"^(?P<channel>\S+)\s+(?P<can_id>{HEX_ID_RE})#(?P<data>[0-9A-Fa-f]*)\s*$"
    ),
)
SPACED_PATTERNS = (
    re.compile(
        rf"^\((?P<timestamp>[^)]+)\)\s+(?P<channel>\S+)\s+"
        rf"(?P<can_id>{HEX_ID_RE})\s+\[(?P<dlc>\d+)\]\s*(?P<data>(?:[0-9A-Fa-f]{{2}}\s*)*)$"
    ),
    re.compile(
        rf"^(?P<timestamp>\d+(?:\.\d+)?)\s+(?P<channel>\S+)\s+"
        rf"(?P<can_id>{HEX_ID_RE})\s+\[(?P<dlc>\d+)\]\s*(?P<data>(?:[0-9A-Fa-f]{{2}}\s*)*)$"
    ),
    re.compile(
        rf"^(?P<channel>\S+)\s+(?P<can_id>{HEX_ID_RE})\s+\[(?P<dlc>\d+)\]\s*"
        rf"(?P<data>(?:[0-9A-Fa-f]{{2}}\s*)*)$"
    ),
)


@dataclass(frozen=True)
class FrameRecord:
    line_number: int
    timestamp: str | None
    channel: str | None
    can_id: int
    data: bytes
    original_line: str

    @property
    def raw_payload_hex(self) -> str:
        return self.data.hex().upper()


@dataclass(frozen=True)
class Observation:
    frame: FrameRecord
    message_name: str
    mux_id: int | None
    fields: dict[str, dict[str, object]]
    notes: list[str]

    def to_dict(self) -> dict[str, object]:
        return {
            "line_number": self.frame.line_number,
            "timestamp": self.frame.timestamp,
            "channel": self.frame.channel,
            "can_id": self.frame.can_id,
            "can_id_hex": format_can_id(self.frame.can_id),
            "message": self.message_name,
            "mux_id": self.mux_id,
            "raw_payload_hex": self.frame.raw_payload_hex,
            "fields": self.fields,
            "notes": self.notes,
        }


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Read-only EV CAN analyzer for offline candump-like logs. "
            "Decodes GTW_carConfig observations and reports before/after changes "
            "without sending or modifying CAN frames."
        )
    )
    parser.add_argument("input", help="Path to a candump-like text log file.")
    parser.add_argument(
        "--json-out",
        help="Write a compact JSON report to this path.",
    )
    parser.add_argument(
        "--md-out",
        help="Write a Markdown evidence report to this path.",
    )
    return parser.parse_args(argv)


def normalize_can_id(value: str) -> int:
    return int(value, 16)


def parse_hex_payload(payload: str) -> bytes:
    compact = payload.replace(" ", "")
    if len(compact) % 2 != 0:
        raise ValueError("payload has an odd number of hex characters")
    return bytes.fromhex(compact)


def parse_log_line(line: str, line_number: int) -> FrameRecord | None:
    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
        return None

    for pattern in COMPACT_PATTERNS:
        match = pattern.match(stripped)
        if not match:
            continue
        data = parse_hex_payload(match.group("data"))
        return FrameRecord(
            line_number=line_number,
            timestamp=match.groupdict().get("timestamp"),
            channel=match.groupdict().get("channel"),
            can_id=normalize_can_id(match.group("can_id")),
            data=data,
            original_line=line.rstrip("\n"),
        )

    for pattern in SPACED_PATTERNS:
        match = pattern.match(stripped)
        if not match:
            continue
        data = parse_hex_payload(match.group("data"))
        dlc = int(match.group("dlc"), 10)
        if len(data) != dlc:
            raise ValueError(f"payload length {len(data)} does not match DLC {dlc}")
        return FrameRecord(
            line_number=line_number,
            timestamp=match.groupdict().get("timestamp"),
            channel=match.groupdict().get("channel"),
            can_id=normalize_can_id(match.group("can_id")),
            data=data,
            original_line=line.rstrip("\n"),
        )

    raise ValueError("unsupported log line format")


def decode_gtw_car_config(frame: FrameRecord) -> Observation:
    mux_id = frame.data[0] & 0x07 if frame.data else None
    fields: dict[str, dict[str, object]] = {}
    notes: list[str] = []

    if len(frame.data) < 6:
        notes.append(
            "GTW_carConfig frame shorter than 6 bytes; GTW_autopilot not decoded."
        )
    elif mux_id == 2:
        autopilot_value = (frame.data[5] >> 2) & 0x07
        fields["GTW_autopilot"] = {
            "value": autopilot_value,
            "label": GTW_AUTOPILOT_ENUM.get(autopilot_value, "UNKNOWN"),
            "bit_range": "42-44",
            "mux": "m2",
        }

    return Observation(
        frame=frame,
        message_name="GTW_carConfig",
        mux_id=mux_id,
        fields=fields,
        notes=notes,
    )


def decode_gtw_autopilot_override(frame: FrameRecord) -> Observation:
    fields: dict[str, dict[str, object]] = {}
    notes: list[str] = []

    if len(frame.data) < 8:
        notes.append(
            "GTW_autopilotOverride frame shorter than 8 bytes; fields not decoded."
        )
    else:
        override_state = frame.data[0] & 0x0F
        autopilot_config = (frame.data[0] >> 4) & 0x07
        override_config = frame.data[1] & 0x07
        config_source = (frame.data[1] >> 3) & 0x03
        expire_time = int.from_bytes(frame.data[4:8], byteorder="little", signed=False)

        fields["GTW_autopilotOverrideState"] = {
            "value": override_state,
            "label": GTW_AUTOPILOT_OVERRIDE_STATE_ENUM.get(override_state, "UNKNOWN"),
            "bit_range": "0-3",
        }
        fields["GTW_autopilotConfig"] = {
            "value": autopilot_config,
            "label": GTW_AUTOPILOT_ENUM.get(autopilot_config, "UNKNOWN"),
            "bit_range": "4-6",
        }
        fields["GTW_autopilotOverrideConfig"] = {
            "value": override_config,
            "label": GTW_AUTOPILOT_ENUM.get(override_config, "UNKNOWN"),
            "bit_range": "8-10",
        }
        fields["GTW_autopilotConfigSource"] = {
            "value": config_source,
            "label": GTW_AUTOPILOT_CONFIG_SOURCE_ENUM.get(config_source, "UNKNOWN"),
            "bit_range": "11-12",
        }
        fields["GTW_autopilotOverrideExpireTime"] = {
            "value": expire_time,
            "label": "raw",
            "bit_range": "32-63",
        }
        notes.append(OPTIONAL_OVERRIDE_NOTE)

    return Observation(
        frame=frame,
        message_name="GTW_autopilotOverride",
        mux_id=None,
        fields=fields,
        notes=notes,
    )


def is_probable_diagnostic_can_id(can_id: int) -> bool:
    return (
        can_id in (0x684, 0x685)
        or (0x600 <= can_id <= 0x7FF)
        or (0x18DA0000 <= can_id <= 0x18DAFFFF)
    )


def describe_uds_service(service_id: int) -> str:
    return UDS_SERVICE_NAMES.get(service_id, "UnknownService")


def describe_uds_nrc(code: int) -> str:
    return UDS_NEGATIVE_RESPONSE_CODES.get(code, "UnknownNegativeResponseCode")


def decode_diagnostic_frame(frame: FrameRecord) -> Observation | None:
    if not is_probable_diagnostic_can_id(frame.can_id) or not frame.data:
        return None

    pci_type = (frame.data[0] >> 4) & 0x0F
    pci_label = {
        0x0: "SingleFrame",
        0x1: "FirstFrame",
        0x2: "ConsecutiveFrame",
        0x3: "FlowControl",
    }.get(pci_type, "Unknown")

    fields: dict[str, dict[str, object]] = {
        "ISO_TP_PCI": {
            "value": pci_type,
            "label": pci_label,
        }
    }
    notes: list[str] = []

    message_name = "DiagnosticTransport"
    if frame.can_id == 0x684:
        message_name = "UDS_gtwRequest"
    elif frame.can_id == 0x685:
        message_name = "GTW_udsResponse"

    if pci_type == 0x0:
        payload_length = frame.data[0] & 0x0F
        if payload_length > len(frame.data) - 1:
            notes.append("ISO-TP single-frame length exceeds payload bytes.")
            return Observation(
                frame=frame,
                message_name=message_name,
                mux_id=None,
                fields=fields,
                notes=notes,
            )

        uds_payload = frame.data[1 : 1 + payload_length]
        fields["UDS_Payload"] = {
            "value": uds_payload.hex().upper(),
            "label": "hex",
        }
        if not uds_payload:
            notes.append("ISO-TP single-frame has empty UDS payload.")
            return Observation(
                frame=frame,
                message_name=message_name,
                mux_id=None,
                fields=fields,
                notes=notes,
            )

        service_id = uds_payload[0]
        if service_id == 0x7F and len(uds_payload) >= 3:
            request_service = uds_payload[1]
            nrc = uds_payload[2]
            fields["UDS_ResponseType"] = {
                "value": "negative_response",
                "label": "NegativeResponse",
            }
            fields["UDS_RequestService"] = {
                "value": request_service,
                "label": describe_uds_service(request_service),
            }
            fields["UDS_NRC"] = {
                "value": nrc,
                "label": describe_uds_nrc(nrc),
            }
            notes.append(
                "Passive diagnostic sniff only. Vendor-specific statuses such as "
                "SIGNATURE_INVALID / INSECURE_OK remain raw unless their payload "
                "encoding is verified."
            )
            return Observation(
                frame=frame,
                message_name=f"{message_name}:NegativeResponse",
                mux_id=None,
                fields=fields,
                notes=notes,
            )

        if service_id >= 0x40 and (service_id - 0x40) in UDS_SERVICE_NAMES:
            request_service = service_id - 0x40
            fields["UDS_ResponseType"] = {
                "value": "positive_response",
                "label": "PositiveResponse",
            }
            fields["UDS_RequestService"] = {
                "value": request_service,
                "label": describe_uds_service(request_service),
            }
            if len(uds_payload) >= 3 and request_service in (0x22, 0x2E):
                identifier = (uds_payload[1] << 8) | uds_payload[2]
                fields["UDS_Identifier"] = {
                    "value": identifier,
                    "label": f"0x{identifier:04X}",
                }
            notes.append(
                "Passive diagnostic sniff only. A positive response indicates the ECU "
                "accepted the request at this protocol layer, but vendor-specific "
                "write status bytes still need verified decoding."
            )
            return Observation(
                frame=frame,
                message_name=f"{message_name}:PositiveResponse",
                mux_id=None,
                fields=fields,
                notes=notes,
            )

        fields["UDS_Service"] = {
            "value": service_id,
            "label": describe_uds_service(service_id),
        }
        return Observation(
            frame=frame,
            message_name=message_name,
            mux_id=None,
            fields=fields,
            notes=notes,
        )

    if pci_type == 0x1:
        total_length = ((frame.data[0] & 0x0F) << 8) | frame.data[1]
        fields["ISO_TP_TotalLength"] = {
            "value": total_length,
            "label": "bytes",
        }
        if len(frame.data) >= 3:
            fields["UDS_Service"] = {
                "value": frame.data[2],
                "label": describe_uds_service(frame.data[2]),
            }
        notes.append(
            "Multi-frame diagnostic message detected; reassembly is not implemented."
        )
        return Observation(
            frame=frame,
            message_name=message_name,
            mux_id=None,
            fields=fields,
            notes=notes,
        )

    if pci_type in (0x2, 0x3):
        notes.append("ISO-TP transport frame detected; raw payload preserved.")
        return Observation(
            frame=frame,
            message_name=message_name,
            mux_id=None,
            fields=fields,
            notes=notes,
        )

    return None


def decode_observations(frames: Iterable[FrameRecord]) -> list[Observation]:
    observations: list[Observation] = []
    for frame in frames:
        if frame.can_id == 0x7FF:
            observations.append(decode_gtw_car_config(frame))
        elif frame.can_id == 0x346:
            observations.append(decode_gtw_autopilot_override(frame))
        else:
            diagnostic = decode_diagnostic_frame(frame)
            if diagnostic is not None:
                observations.append(diagnostic)
    return observations


def build_field_change(
    field_name: str,
    previous: tuple[Observation, int, str],
    current: tuple[Observation, int, str],
) -> dict[str, object]:
    previous_observation, previous_value, previous_label = previous
    current_observation, current_value, current_label = current
    transition = f"{previous_value} -> {current_value}"
    change = {
        "field": field_name,
        "message": current_observation.message_name,
        "transition": transition,
        "before": {
            "value": previous_value,
            "label": previous_label,
            "timestamp": previous_observation.frame.timestamp,
            "line_number": previous_observation.frame.line_number,
            "raw_payload_hex": previous_observation.frame.raw_payload_hex,
            "can_id_hex": format_can_id(previous_observation.frame.can_id),
        },
        "after": {
            "value": current_value,
            "label": current_label,
            "timestamp": current_observation.frame.timestamp,
            "line_number": current_observation.frame.line_number,
            "raw_payload_hex": current_observation.frame.raw_payload_hex,
            "can_id_hex": format_can_id(current_observation.frame.can_id),
        },
    }
    if field_name == "GTW_autopilot":
        change["explicit_notice"] = (
            f"GTW_autopilot changed: {transition} ({previous_label} -> {current_label})"
        )
    return change


def detect_changes(observations: Iterable[Observation]) -> list[dict[str, object]]:
    changes: list[dict[str, object]] = []
    last_seen: dict[str, tuple[Observation, int, str]] = {}

    for observation in observations:
        for field_name, payload in observation.fields.items():
            raw_value = payload.get("value")
            if not isinstance(raw_value, int):
                continue
            value = raw_value
            label = str(payload.get("label", value))
            previous = last_seen.get(field_name)
            current = (observation, value, label)
            if previous is not None and previous[1] != value:
                changes.append(build_field_change(field_name, previous, current))
            last_seen[field_name] = current

    return changes


def format_can_id(can_id: int) -> str:
    return f"0x{can_id:03X}"


def build_report(
    source: Path, frames: list[FrameRecord], observations: list[Observation]
) -> dict[str, object]:
    changes = detect_changes(observations)
    warnings = []
    if any(observation.frame.can_id == 0x346 for observation in observations):
        warnings.append(OPTIONAL_OVERRIDE_NOTE)

    report = {
        "report_type": "ev_can_analyzer",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "source": str(source),
        "definitions": {
            "GTW_carConfig": {
                "can_id": format_can_id(0x7FF),
                "decoded_fields": ["GTW_autopilot"],
            },
            "GTW_autopilotOverride": {
                "can_id": format_can_id(0x346),
                "decoded_fields": [
                    "GTW_autopilotConfig",
                    "GTW_autopilotOverrideConfig",
                    "GTW_autopilotConfigSource",
                    "GTW_autopilotOverrideState",
                    "GTW_autopilotOverrideExpireTime",
                ],
                "missing_fields": [],
                "available_in_repo": True,
                "note": OPTIONAL_OVERRIDE_NOTE,
            },
            "DiagnosticTransport": {
                "can_id_ranges": [
                    "0x684-0x685",
                    "0x600-0x7FF",
                    "0x18DA0000-0x18DAFFFF",
                ],
                "decoded_fields": [
                    "ISO_TP_PCI",
                    "UDS_Payload",
                    "UDS_ResponseType",
                    "UDS_RequestService",
                    "UDS_NRC",
                    "UDS_Identifier",
                ],
                "available_in_repo": True,
                "note": (
                    "Generic passive ISO-TP/UDS sniffing only. Vendor-specific gateway "
                    "write statuses remain raw until their payload encoding is verified."
                ),
            },
        },
        "summary": {
            "parsed_frames": len(frames),
            "relevant_observations": len(observations),
            "changes_detected": len(changes),
            "message_ids_observed": sorted(
                {format_can_id(obs.frame.can_id) for obs in observations}
            ),
            "optional_signal_definitions_missing": [],
            "vendor_specific_gateway_statuses_missing": [
                "SIGNATURE_INVALID",
                "INSECURE_OK",
            ],
        },
        "warnings": warnings,
        "observations": [observation.to_dict() for observation in observations],
        "changes": changes,
    }
    return report


def render_markdown(report: dict[str, object]) -> str:
    summary = report["summary"]
    lines = [
        "# EV CAN Analyzer Report",
        "",
        f"- Source: `{report['source']}`",
        f"- Generated: `{report['generated_at']}`",
        f"- Parsed frames: `{summary['parsed_frames']}`",
        f"- Relevant observations: `{summary['relevant_observations']}`",
        f"- Changes detected: `{summary['changes_detected']}`",
    ]

    definitions = report["definitions"]
    lines.extend(
        [
            "",
            "## Definitions",
            f"- `GTW_carConfig` (`{definitions['GTW_carConfig']['can_id']}`): decodes `GTW_autopilot` on mux `m2` bits `42-44`.",
            f"- `GTW_autopilotOverride` (`{definitions['GTW_autopilotOverride']['can_id']}`): decodes override state/config/source and expire time from the CH DBC data.",
            "- `DiagnosticTransport` (`0x684`, `0x685`, plus generic UDS ranges): passive ISO-TP/UDS sniffing for gateway request/response traffic.",
        ]
    )

    warnings = report.get("warnings") or []
    if warnings:
        lines.extend(["", "## Warnings"])
        for warning in warnings:
            lines.append(f"- {warning}")

    changes = report["changes"]
    lines.extend(["", "## Changes"])
    if changes:
        lines.append(
            "| Field | Transition | Before | After | Before TS | After TS | Before Payload | After Payload |"
        )
        lines.append("| --- | --- | --- | --- | --- | --- | --- | --- |")
        for change in changes:
            before = change["before"]
            after = change["after"]
            lines.append(
                "| {field} | `{transition}` | `{before_value}` ({before_label}) | "
                "`{after_value}` ({after_label}) | `{before_ts}` | `{after_ts}` | "
                "`{before_payload}` | `{after_payload}` |".format(
                    field=change["field"],
                    transition=change["transition"],
                    before_value=before["value"],
                    before_label=before["label"],
                    after_value=after["value"],
                    after_label=after["label"],
                    before_ts=before["timestamp"] or "n/a",
                    after_ts=after["timestamp"] or "n/a",
                    before_payload=before["raw_payload_hex"],
                    after_payload=after["raw_payload_hex"],
                )
            )
    else:
        lines.append("No field changes detected in the decoded observations.")

    lines.extend(["", "## Observations"])
    observations = report["observations"]
    if observations:
        lines.append(
            "| Line | Timestamp | CAN ID | Message | Mux | Raw Payload | Fields | Notes |"
        )
        lines.append("| --- | --- | --- | --- | --- | --- | --- | --- |")
        for observation in observations:
            note_text = (
                "; ".join(observation["notes"]) if observation["notes"] else "n/a"
            )
            lines.append(
                "| {line_number} | `{timestamp}` | `{can_id_hex}` | {message} | `{mux_id}` | "
                "`{raw_payload_hex}` | {fields} | {notes} |".format(
                    line_number=observation["line_number"],
                    timestamp=observation["timestamp"] or "n/a",
                    can_id_hex=observation["can_id_hex"],
                    message=observation["message"],
                    mux_id=observation["mux_id"]
                    if observation["mux_id"] is not None
                    else "n/a",
                    raw_payload_hex=observation["raw_payload_hex"],
                    fields=format_field_summary_dict(observation["fields"]),
                    notes=note_text,
                )
            )
    else:
        lines.append(
            "No relevant 0x7FF, 0x346, 0x684, or 0x685 frames were found in the input."
        )

    return "\n".join(lines) + "\n"


def format_field_summary_dict(fields: dict[str, dict[str, object]]) -> str:
    if not fields:
        return "n/a"
    parts = []
    for name, payload in fields.items():
        value = payload.get("value")
        label = payload.get("label")
        if label is None:
            parts.append(f"`{name}={value}`")
        else:
            parts.append(f"`{name}={value} ({label})`")
    return "; ".join(parts)


def read_frames(path: Path) -> tuple[list[FrameRecord], list[str]]:
    frames: list[FrameRecord] = []
    warnings: list[str] = []

    with path.open("r", encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, start=1):
            try:
                frame = parse_log_line(line, line_number)
            except ValueError as exc:
                warnings.append(f"line {line_number}: {exc}")
                continue
            if frame is not None:
                frames.append(frame)

    return frames, warnings


def print_summary(report: dict[str, object]) -> None:
    summary = report["summary"]
    print(f"Parsed frames: {summary['parsed_frames']}")
    print(f"Relevant observations: {summary['relevant_observations']}")
    print(f"Changes detected: {summary['changes_detected']}")

    if report["changes"]:
        for change in report["changes"]:
            notice = change.get("explicit_notice")
            if notice:
                after = change["after"]
                print(
                    f"- {notice} at {after['timestamp'] or 'n/a'} "
                    f"payload={after['raw_payload_hex']}"
                )
            else:
                print(f"- {change['field']}: {change['transition']}")
    else:
        print("No decoded field changes detected.")

    for warning in report.get("warnings") or []:
        print(f"Warning: {warning}", file=sys.stderr)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    input_path = Path(args.input)
    try:
        frames, parse_warnings = read_frames(input_path)
    except FileNotFoundError:
        print(f"Input file not found: {input_path}", file=sys.stderr)
        return 1
    observations = decode_observations(frames)
    report = build_report(input_path, frames, observations)
    if parse_warnings:
        report.setdefault("warnings", []).extend(parse_warnings)

    if args.json_out:
        json_path = Path(args.json_out)
        json_path.write_text(
            json.dumps(report, ensure_ascii=True, separators=(",", ":")) + "\n",
            encoding="utf-8",
        )
    if args.md_out:
        md_path = Path(args.md_out)
        md_path.write_text(render_markdown(report), encoding="utf-8")

    print_summary(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
