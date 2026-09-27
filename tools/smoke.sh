#!/usr/bin/env bash
# Linux/macOS equivalent of smoke.ps1.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exe="$root/tools/build/ledbasic"

if [ ! -f "$exe" ]; then
    echo "Build the host tools first: cmake -B tools/build -S tools && cmake --build tools/build" >&2
    exit 1
fi

bas="$root/examples/bas/Rainbow.bas"
"$exe" run "$bas" --leds 60 --frames 3 --dump-ascii
echo "smoke ok"
