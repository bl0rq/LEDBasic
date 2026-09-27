#!/usr/bin/env bash
# Run the TUI. Linux/macOS equivalent of run-tui.ps1 (which opens a new
# console window on Windows; here we just run in the current terminal).
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exe="$root/tools/build/tui/ledbasic-tui"

if [ ! -f "$exe" ]; then
    echo "ledbasic-tui not found. Build first: tools/build.sh" >&2
    exit 1
fi

cd "$root"
exec "$exe" "$@"
