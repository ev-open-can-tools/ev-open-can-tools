import os
from pathlib import Path

from SCons.Errors import UserError
from SCons.Script import Import

Import("env")


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
    "BENCH_LOOPBACK_SELFTEST",
)
CREDENTIAL_DEFINES = ("DASH_SSID", "DASH_PASS", "DASH_OTA_USER", "DASH_OTA_PASS")
CREDENTIAL_PLACEHOLDERS = {
    "DASH_PASS": "changeme",
    "DASH_OTA_PASS": "changeme",
}
CONFIG_RELATIVE_PATH = Path("platformio_profile.h")


def _active_defines(text):
    active = set()
    for line in text.splitlines():
        stripped = line.lstrip()
        if not stripped.startswith("#define"):
            continue
        parts = stripped.split(None, 2)
        if len(parts) >= 2:
            active.add(parts[1])
    return active


def _string_define_values(text, names):
    """Parse #define NAME "value" lines from the shared PlatformIO profile."""
    result = {}
    for line in text.splitlines():
        stripped = line.lstrip()
        if not stripped.startswith("#define"):
            continue
        parts = stripped.split(None, 2)
        if len(parts) >= 3 and parts[1] in names:
            rest = parts[2].strip()
            if rest.startswith('"'):
                end = rest.find('"', 1)
                if end != -1:
                    result[parts[1]] = rest[1:end]
    return result


def _pick_one(active, choices, label):
    selected = [name for name in choices if name in active]
    if len(selected) != 1:
        raise UserError(
            f"{CONFIG_RELATIVE_PATH.as_posix()} must enable exactly one {label}: {', '.join(choices)}."
        )
    return selected[0]


def _normalize_cppdefines(raw_defines):
    normalized = set()
    for item in raw_defines or []:
        if isinstance(item, (tuple, list)):
            if item:
                normalized.add(item[0])
        else:
            normalized.add(item)
    return normalized


def _project_option_defines(env_obj):
    define_sources = []

    build_flags_option = env_obj.GetProjectOption("build_flags", "")
    if isinstance(build_flags_option, str):
        define_sources.append(build_flags_option)
    else:
        define_sources.extend(build_flags_option)

    substituted_build_flags = env_obj.subst("$BUILD_FLAGS").strip()
    if substituted_build_flags:
        define_sources.append(substituted_build_flags)

    normalized = set()
    for source in define_sources:
        parsed = env_obj.ParseFlags(source)
        normalized.update(_normalize_cppdefines(parsed.get("CPPDEFINES")))
    return normalized


project_dir = Path(env["PROJECT_DIR"])
config_path = project_dir / CONFIG_RELATIVE_PATH
config_text = config_path.read_text(encoding="utf-8")
active = _active_defines(config_text)

_DASH_HW_MAP = {"LEGACY": 0, "HW3": 1, "HW4": 2}

selected_driver = _pick_one(active, DRIVER_DEFINES, "driver define")
uses_dashboard_hw = selected_driver == "DRIVER_ESP32_EXT_MCP2515"
selected_vehicle = _pick_one(active, VEHICLE_DEFINES, "vehicle define")
selected_options = [name for name in OPTIONAL_DEFINES if name in active]

env_defines = _normalize_cppdefines(env.get("CPPDEFINES"))
project_defines = _project_option_defines(env)
env_driver = [name for name in DRIVER_DEFINES if name in project_defines]
env_vehicle = [name for name in VEHICLE_DEFINES if name in env_defines]

if len(env_driver) != 1:
    raise UserError(
        f"PlatformIO env '{env['PIOENV']}' must define exactly one CAN driver: "
        f"{', '.join(DRIVER_DEFINES)}."
    )

if env_driver[0] != selected_driver:
    raise UserError(
        f"{CONFIG_RELATIVE_PATH.as_posix()} selects {selected_driver}, but PlatformIO env "
        f"'{env['PIOENV']}' is configured for {env_driver[0]}. Pick the matching "
        f"'pio run -e ...' environment or update {CONFIG_RELATIVE_PATH.as_posix()}."
    )

if not uses_dashboard_hw and env_vehicle and env_vehicle != [selected_vehicle]:
    raise UserError(
        f"PlatformIO env '{env['PIOENV']}' already defines {env_vehicle[0]}, but "
        f"{CONFIG_RELATIVE_PATH.as_posix()} selects {selected_vehicle}. Remove the conflicting build flag."
    )

if uses_dashboard_hw:
    dash_hw_val = _DASH_HW_MAP[selected_vehicle]
    env.Append(CPPDEFINES=[("DASH_DEFAULT_HW", str(dash_hw_val))])
    sync_defines = selected_options
else:
    sync_defines = [selected_vehicle] + selected_options

missing_defines = [name for name in sync_defines if name not in env_defines]
if missing_defines:
    env.Append(CPPDEFINES=missing_defines)

# Dashboard credential sync and placeholder check
uses_dashboard = "ESP32_DASHBOARD" in project_defines
if uses_dashboard:
    credentials = _string_define_values(config_text, CREDENTIAL_DEFINES)

    skip_check = os.getenv("SKIP_DASH_CREDENTIAL_CHECK", "").strip().lower() in {
        "1",
        "true",
        "yes",
        "on",
    }
    if not skip_check:
        errors = []
        for macro, placeholder in CREDENTIAL_PLACEHOLDERS.items():
            val = credentials.get(macro, "")
            if val == placeholder:
                errors.append(
                    f'  {macro} is still the default placeholder "{placeholder}"'
                )

        if errors:
            print("\n" + "=" * 60)
            print("BUILD ERROR: Default dashboard credentials detected.")
            print(
                f"Edit {CONFIG_RELATIVE_PATH.as_posix()} and change these values before building:"
            )
            for e in errors:
                print(e)
            print("=" * 60 + "\n")
            env.Exit(1)

    for cred_name in CREDENTIAL_DEFINES:
        if cred_name in credentials:
            env.Append(CPPDEFINES=[(cred_name, f'\\"{credentials[cred_name]}\\"')])

print(
    f"Synced {CONFIG_RELATIVE_PATH.as_posix()} defines for {env['PIOENV']}: "
    + (
        f"DASH_DEFAULT_HW={_DASH_HW_MAP[selected_vehicle]} ({selected_vehicle})"
        if uses_dashboard_hw
        else selected_vehicle
    )
    + (f", {', '.join(selected_options)}" if selected_options else "")
)
