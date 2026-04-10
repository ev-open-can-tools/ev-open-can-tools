import platform
import subprocess
from pathlib import Path

from SCons.Script import Import

Import("env")


if platform.system() == "Darwin":
    sdk_root = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    libcxx_include = Path(sdk_root) / "usr" / "include" / "c++" / "v1"

    if libcxx_include.is_dir():
        env.Append(CPPPATH=[str(libcxx_include)])
        print(f"Configured macOS native test includes from {libcxx_include}")
