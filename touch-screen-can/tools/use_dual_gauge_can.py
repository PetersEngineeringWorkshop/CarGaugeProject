"""Build the same CAN transport and use the same decoder as Dual Gauge CAN."""

from pathlib import Path

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
reference = project.parent / "dual-gauge-can"
version = (project / "VERSION").read_text().strip()
env.Append(CPPDEFINES=[("TOUCH_CAN_VERSION", env.StringifyMacro(version))])
env.Prepend(CPPPATH=[str(project / "include"), str(reference / "include")])
env.BuildSources("$BUILD_DIR/dual-gauge-can", str(reference / "src/can"))
