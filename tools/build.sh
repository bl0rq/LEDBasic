#!/usr/bin/env bash
# Configure and build host CLI + TUI. Linux/macOS equivalent of build.ps1.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake not found. Install CMake (e.g. apt install cmake)." >&2
    exit 1
fi

echo "Configuring host tools..."
cmake -B tools/build -S tools -DLEDBASIC_BUILD_TUI=ON

echo "Building..."
cmake --build tools/build

tui="$root/tools/build/tui/ledbasic-tui"
if [ -f "$tui" ]; then
    echo "TUI: $tui"
fi
