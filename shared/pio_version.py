"""Expose the firmware version from VERSION."""

from pathlib import Path

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
version = (project / "VERSION").read_text().strip()
env.Append(CPPDEFINES=[("GAUGE_FIRMWARE_VERSION", env.StringifyMacro(version))])
