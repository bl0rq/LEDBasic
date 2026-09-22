Import("env")

from pathlib import Path
import inspect

try:
    usermod_dir = Path(inspect.getfile(inspect.currentframe())).resolve().parent
except Exception:
    usermod_dir = Path(".").resolve()
include_dir = usermod_dir.parent / "include"
env.Append(CPPPATH=[str(include_dir)])
env.Append(CPPDEFINES=["LEDBASIC_NO_FASTLED"])
