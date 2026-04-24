#!/usr/bin/env python3

import argparse
import re
from pathlib import Path


DRIVER_DEFINES = (
    "DRIVER_MCP2515",
    "DRIVER_SAME51",
    "DRIVER_TWAI",
    "DRIVER_ESP32_EXT_MCP2515",
)
VEHICLE_DEFINES = ("LEGACY", "HW3", "HW4")
OPTIONAL_DEFINES = (
    "ISA_SPEED_CHIME_SUPPRESS",
    "EMERGENCY_VEHICLE_DETECTION",
    "BYPASS_TLSSC_REQUIREMENT",
    "NAG_KILLER",
    "ENHANCED_AUTOPILOT",
    "INJECTION_AFTER_AP",
)
MANAGED_DEFINES = set(DRIVER_DEFINES + VEHICLE_DEFINES + OPTIONAL_DEFINES)
MANAGED_DEFINE_ORDER = DRIVER_DEFINES + VEHICLE_DEFINES + OPTIONAL_DEFINES
DEFINE_PATTERN = re.compile(
    r"^(?P<indent>\s*)(?P<comment>//\s*)?#define\s+(?P<name>[A-Z0-9_]+)(?P<rest>.*)$"
)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Toggle the shared PlatformIO profile board, vehicle, and optional feature defines."
    )
    parser.add_argument("--driver", choices=DRIVER_DEFINES, required=True)
    parser.add_argument("--vehicle", choices=VEHICLE_DEFINES, required=True)
    parser.add_argument(
        "--enable",
        choices=OPTIONAL_DEFINES,
        action="append",
        default=[],
        help="Optional feature define to enable. Can be passed multiple times.",
    )
    parser.add_argument(
        "--file",
        default="platformio_profile.h",
        help="Path to the shared PlatformIO profile file to update.",
    )
    return parser.parse_args()


def rewrite_define(line, should_enable):
    match = DEFINE_PATTERN.match(line)
    if not match:
        return line

    name = match.group("name")
    if name not in MANAGED_DEFINES:
        return line

    indent = match.group("indent")
    rest = match.group("rest")
    newline = line[len(line.rstrip("\r\n")) :]
    if should_enable:
        return f"{indent}#define {name}{rest}{newline}"
    return f"{indent}// #define {name}{rest}{newline}"


def main():
    args = parse_args()
    profile_path = Path(args.file)
    enabled = {args.driver, args.vehicle, *args.enable}
    vehicle_summary = f"default={args.vehicle}" if args.driver == "DRIVER_TWAI" else args.vehicle
    options_summary = ", ".join(args.enable) if args.enable else "none"

    lines = profile_path.read_text(encoding="utf-8").splitlines(keepends=True)
    updated_lines = []
    seen = set()

    for line in lines:
        match = DEFINE_PATTERN.match(line)
        if match and match.group("name") in MANAGED_DEFINES:
            name = match.group("name")
            seen.add(name)
            updated_lines.append(rewrite_define(line, name in enabled))
        else:
            updated_lines.append(line)

    missing = [name for name in MANAGED_DEFINE_ORDER if name not in seen]
    if missing:
        if updated_lines and not updated_lines[-1].endswith(("\n", "\r")):
            updated_lines[-1] = f"{updated_lines[-1]}\n"
        for name in missing:
            updated_lines.append(rewrite_define(f"#define {name}\n", name in enabled))

    profile_path.write_text("".join(updated_lines), encoding="utf-8")

    print(
        f"Updated {profile_path}: driver={args.driver}, vehicle={vehicle_summary}, "
        f"options={options_summary}"
    )


if __name__ == "__main__":
    main()
