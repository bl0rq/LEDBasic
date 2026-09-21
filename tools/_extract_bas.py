import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
src = root / "include" / "BasicExamples"
dst = root / "examples" / "bas"
dst.mkdir(parents=True, exist_ok=True)

for p in sorted(src.glob("*.h")):
    text = p.read_text(encoding="utf-8")
    m = re.search(r'R"\((.*)\)"', text, re.S)
    if not m:
        raise SystemExit(f"no R-string in {p}")
    body = m.group(1)
    if body.startswith("\n"):
        body = body[1:]
    name = p.stem
    out = dst / f"{name}.bas"
    header = (
        f"# {name} — copied from include/BasicExamples/{p.name}\n"
        "# Firmware header remains the source of truth for sketches.\n\n"
    )
    out.write_text(header + body, encoding="utf-8", newline="\n")
    print("wrote", out)
