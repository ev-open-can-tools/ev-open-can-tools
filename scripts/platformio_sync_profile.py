import os
import subprocess
import sys
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
)
DASHBOARD_OPTION_DEFINES = ("INJECTION_AFTER_AP", "DASH_INJECTION_AFTER_AP")
# Synced for dashboard builds only (defines with literal values).
DASHBOARD_VALUE_DEFINES = ("PLUGIN_GTW_UDS_KEY_READY",)
CREDENTIAL_DEFINES = ("DASH_SSID", "DASH_PASS", "DASH_OTA_USER", "DASH_OTA_PASS")
FIRMWARE_ARTIFACTS = {
    "esp32_twai": "firmware-esp32.bin",
    "esp32s2_twai": "firmware-esp32s2.bin",
    "esp32c6_twai": "firmware-esp32c6.bin",
    "lilygo_tcan485_hw3": "firmware-lilygo-tcan485-hw3.bin",
    "lilygo_t2can": "firmware-lilygo-t2can.bin",
    "m5stack-atomic-can-base": "firmware-m5stack.bin",
    "m5stack-atoms3-mini-can-base": "firmware-m5stack-atoms3-mini.bin",
    "esp32_feather_v2_mcp2515": "firmware-esp32-featherwing.bin",
    "esp32_ext_mcp2515": "firmware-esp32-ext-mcp2515.bin",
    "waveshare_ESP32_S3_RS485_CAN": "firmware-waveshare-esp32-s3.bin",
}
CONFIG_RELATIVE_PATH = Path("platformio_profile.h")
EXAMPLE_CONFIG_RELATIVE_PATH = Path("platformio_profile.example.h")


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
    """Parse #define NAME "value" lines from the local build config."""
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


def _literal_define_values(text, names):
    """Parse #define NAME value lines from the local build config."""
    result = {}
    for line in text.splitlines():
        stripped = line.lstrip()
        if not stripped.startswith("#define"):
            continue
        parts = stripped.split(None, 2)
        if len(parts) >= 3 and parts[1] in names:
            value = parts[2].split("//", 1)[0].strip()
            if value:
                result[parts[1]] = value
    return result


def _pick_one(active, choices, label):
    selected = [name for name in choices if name in active]
    if len(selected) != 1:
        raise UserError(
            f"{CONFIG_RELATIVE_PATH.as_posix()} must enable exactly one {label}: {', '.join(choices)}."
        )
    return selected[0]


def _pick_dashboard_default(active, choices, default_choice):
    selected = [name for name in choices if name in active]
    if len(selected) == 1:
        return selected[0]
    return default_choice


def _normalize_cppdefines(raw_defines):
    normalized = set()
    for item in raw_defines or []:
        if isinstance(item, (tuple, list)):
            if item:
                normalized.add(item[0])
        else:
            normalized.add(item)
    return normalized


def _set_cppdefine(env_obj, name, value):
    cppdefines = list(env_obj.get("CPPDEFINES") or [])
    updated = []
    replaced = False
    for item in cppdefines:
        item_name = item[0] if isinstance(item, (tuple, list)) and item else item
        if item_name == name:
            if not replaced:
                updated.append((name, str(value)))
                replaced = True
            continue
        updated.append(item)
    if not replaced:
        updated.append((name, str(value)))
    env_obj.Replace(CPPDEFINES=updated)


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


def _regenerate_dashboard(project_dir):
    script = project_dir / "scripts" / "minify_dashboard.py"
    src = project_dir / "include" / "web" / "mcp2515_dashboard_ui.src.h"
    if not script.exists() or not src.exists():
        return
    try:
        subprocess.run([sys.executable, str(script)], cwd=str(project_dir), check=True)
    except subprocess.CalledProcessError as exc:
        raise UserError(f"Dashboard UI generation failed: {exc}") from exc


def _sync_sdkconfig_flash_size(env_obj, project_dir):
    """Keep ESP-IDF image header aligned with board and partition size."""
    flash_size = str(env_obj.BoardConfig().get("upload.flash_size", "4MB")).upper()
    supported = {"1MB", "2MB", "4MB", "8MB", "16MB", "32MB", "64MB", "128MB"}
    if flash_size not in supported:
        raise UserError(f"Unsupported board flash size: {flash_size}")

    sdkconfig_path = project_dir / f"sdkconfig.{env_obj['PIOENV']}"
    lines = []
    if sdkconfig_path.exists():
        lines = sdkconfig_path.read_text(encoding="utf-8").splitlines()
    lines = [
        line
        for line in lines
        if "CONFIG_ESPTOOLPY_FLASHSIZE_" not in line
        and not line.startswith("CONFIG_ESPTOOLPY_FLASHSIZE=")
    ]
    lines.append(f"CONFIG_ESPTOOLPY_FLASHSIZE_{flash_size}=y")
    updated = "\n".join(lines) + "\n"
    current = sdkconfig_path.read_text(encoding="utf-8") if sdkconfig_path.exists() else ""
    if updated != current:
        sdkconfig_path.write_text(updated, encoding="utf-8")


project_dir = Path(env["PROJECT_DIR"])
_sync_sdkconfig_flash_size(env, project_dir)
config_path = Path(
    env.GetProjectOption("custom_profile_path", CONFIG_RELATIVE_PATH.as_posix())
)
if not config_path.is_absolute():
    config_path = project_dir / config_path
example_config_path = Path(
    env.GetProjectOption(
        "custom_example_profile_path", EXAMPLE_CONFIG_RELATIVE_PATH.as_posix()
    )
)
if not example_config_path.is_absolute():
    example_config_path = project_dir / example_config_path
version_path = Path(env.GetProjectOption("custom_version_path", "VERSION"))
if not version_path.is_absolute():
    version_path = project_dir / version_path
display_config_path = (
    config_path.relative_to(project_dir)
    if config_path.is_relative_to(project_dir)
    else config_path
)
display_example_path = (
    example_config_path.relative_to(project_dir)
    if example_config_path.is_relative_to(project_dir)
    else example_config_path
)
if not config_path.exists():
    raise UserError(
        f"Missing {display_config_path.as_posix()}. Copy "
        f"{display_example_path.as_posix()} to {display_config_path.as_posix()}, "
        "then edit the local build config for your board and vehicle."
    )
config_text = config_path.read_text(encoding="utf-8")
active = _active_defines(config_text)
project_defines = _project_option_defines(env)
uses_dashboard = "ESP32_DASHBOARD" in project_defines
if uses_dashboard:
    _regenerate_dashboard(project_dir)

_DASH_HW_MAP = {"LEGACY": 0, "HW3": 1, "HW4": 2}

profile_driver = _pick_one(active, DRIVER_DEFINES, "driver define")
uses_dashboard_hw = uses_dashboard
if uses_dashboard_hw:
    selected_vehicle = _pick_dashboard_default(active, VEHICLE_DEFINES, "HW3")
else:
    selected_vehicle = _pick_one(active, VEHICLE_DEFINES, "vehicle define")
selected_options = (
    [] if uses_dashboard_hw else [name for name in OPTIONAL_DEFINES if name in active]
)
dashboard_flag_options = (
    [name for name in DASHBOARD_OPTION_DEFINES if name in active]
    if uses_dashboard_hw
    else []
)
dashboard_values = (
    _literal_define_values(config_text, DASHBOARD_VALUE_DEFINES)
    if uses_dashboard_hw
    else {}
)
dashboard_options = []
if uses_dashboard_hw:
    for name in DASHBOARD_VALUE_DEFINES:
        if name not in active:
            continue
        if name in project_defines:
            continue
        if name not in dashboard_values:
            raise UserError(
                f"{CONFIG_RELATIVE_PATH.as_posix()} must define {name} with a byte value, "
                f"for example: #define {name} 0x12."
            )
        dashboard_options.append((name, dashboard_values[name]))

env_defines = _normalize_cppdefines(env.get("CPPDEFINES"))
env_driver = [name for name in DRIVER_DEFINES if name in project_defines]
env_vehicle = [name for name in VEHICLE_DEFINES if name in env_defines]

if len(env_driver) != 1:
    raise UserError(
        f"PlatformIO env '{env['PIOENV']}' must define exactly one CAN driver: "
        f"{', '.join(DRIVER_DEFINES)}."
    )

if not uses_dashboard_hw and env_vehicle and env_vehicle != [selected_vehicle]:
    raise UserError(
        f"PlatformIO env '{env['PIOENV']}' already defines {env_vehicle[0]}, but "
        f"{display_config_path.as_posix()} selects {selected_vehicle}. Remove the conflicting build flag."
    )

if uses_dashboard_hw:
    dash_hw_val = _DASH_HW_MAP[selected_vehicle]
    _set_cppdefine(env, "DASH_DEFAULT_HW", dash_hw_val)
    sync_defines = dashboard_flag_options + dashboard_options
else:
    sync_defines = [selected_vehicle] + selected_options

missing_defines = []
for item in sync_defines:
    name = item[0] if isinstance(item, (tuple, list)) and item else item
    if name not in env_defines:
        missing_defines.append(item)
if missing_defines:
    env.Append(CPPDEFINES=missing_defines)

# Make platformio_profile.h resolvable via #include "platformio_profile.h"
env.Append(CPPPATH=[str(config_path.parent)])

# Dashboard credential sync and placeholder check
uses_dashboard = "ESP32_DASHBOARD" in project_defines
if uses_dashboard:
    credentials = _string_define_values(config_text, CREDENTIAL_DEFINES)

    # Default credentials ("changeme") are allowed. Users change them via the
    # dashboard WiFi Hotspot card at runtime (persisted in NVS).
    for cred_name in CREDENTIAL_DEFINES:
        if cred_name in credentials:
            env.Append(CPPDEFINES=[(cred_name, f'\\"{credentials[cred_name]}\\"')])

    artifact = FIRMWARE_ARTIFACTS.get(env["PIOENV"])
    if artifact is None:
        raise UserError(
            f"PlatformIO env '{env['PIOENV']}' has no OTA release artifact mapping."
        )
    env.Append(CPPDEFINES=[("FIRMWARE_ARTIFACT", f'\\"{artifact}\\"')])

# Inject firmware version from VERSION file
if version_path.exists():
    fw_version = version_path.read_text(encoding="utf-8").strip()
    env.Append(CPPDEFINES=[("FIRMWARE_VERSION", f'\\"{fw_version}\\"')])
