# LEDBasic host tools

Run and debug LEDBasic programs on the host without a microcontroller.

## VS Code (recommended)

Workspace files in `.vscode/` wire the TUI into the editor:

| Action | How |
|---|---|
| Build | `Ctrl+Shift+B` (task **Build host tools**) |
| Run TUI | `F5` — opens a **new console window** (Windows Terminal if installed) |
| Run current `.bas` | launch config **LEDBasic TUI (open file)** |
| Run inside VS Code | launch config **LEDBasic TUI (VS Code terminal)**, then click the **Terminal** panel (not Debug Console) |
| Debug the TUI itself | config **Debug LEDBasic TUI (gdb)** |

`F5` does not open a GUI window — LEDBasic is a terminal app. The default config pops a separate console so it is not buried under Debug Console. If a previous F5 left the status bar orange, press `Shift+F5` first to stop that session.

Needs CMake and a C++17 compiler (`g++` from MSYS2 on Windows is fine). `tools/build.ps1` prepends `C:\msys64\ucrt64\bin` the same way the native tests do.

## CLI build

```
cmake -B tools/build -S tools
cmake --build tools/build
```

Or: `pwsh -File tools/build.ps1`

## Headless runner

```
tools/build/ledbasic run examples/bas/Rainbow.bas --leds 60 --frames 5 --dump-ascii
```

- `--leds N` — strip length (1–300, default 60)
- `--frames N` — BASIC `loop` iterations after `setup`
- `--dt MS` — simulated milliseconds per frame (default 16)
- `--dump-ascii` — luminance characters instead of hex RGB

Exit 0 if any pixel lit, 2 if the strip stayed black, 1 on load/runtime error.

## Examples

`examples/bas/*.bas` are copies of `include/BasicExamples/*.h`. The headers remain the firmware source of truth. Refresh the copies with:

```
C:\msys64\ucrt64\bin\python.exe tools/_extract_bas.py
```

## TUI

Built by default (`-DLEDBASIC_BUILD_TUI=ON`):

```
tools/build/tui/ledbasic-tui
tools/build/tui/ledbasic-tui examples/bas/Breathing.bas --leds 120
```

Run from the repo root so example paths resolve. Windows Terminal (or any UTF-8 truecolor terminal) is recommended.

## Manual TUI checklist

1. Open `examples/bas/Rainbow.bas` (default)
2. F5 — LEDs animate
3. Esc — stop, then F9 on a `sethsv` line, F5 — hits the breakpoint
4. Immediate `? i` — prints the loop index
5. F8 — advances one statement
6. Focus the params list (click it or Tab), Left/Right — effect updates
7. F1 — help overlay
