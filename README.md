# LEDBasic

A **BASIC language interpreter** for programming LED animations with [FastLED](https://github.com/FastLED/FastLED). Features virtual strip layering with blend modes, runtime parameter discovery, serial command interface, and 10 ready-to-use example programs. Designed as a PlatformIO library for easy integration into LED projects.

## Features

- **BASIC Interpreter** — Write LED animation programs in a simple BASIC-like language with `setup`/`loop` structure
- **Parameter Discovery** — Programs declare tunable parameters (`param speed number(20, 5, 100, 1)`) that are automatically discovered and adjustable at runtime
- **Virtual Strip Manager** — Layer multiple BASIC programs on overlapping regions of a physical LED strip with Z-ordering and blend modes (replace, add, subtract, multiply, screen). Replace treats black as transparent so lower layers show through.
- **Serial Command Handler** — Built-in serial interface for switching programs and adjusting parameters at runtime
- **Standalone Effects** — Optional extras (`#include "Effects.h"`) for color-wheel / demo-reel tests. Not pulled in by `LEDBasic.h`.
- **Example Programs** — 10 ready-to-use BASIC programs: Rainbow, Breathing, SineWave, DoubleRainbow, Matrix, BackgroundStars, MovingComets, PulsingCenter, BikeParked, BikeRolling

## Installation

### PlatformIO (recommended)

Add to your `platformio.ini`:

```ini
lib_deps =
    https://github.com/bradb/LEDBasic.git#main
```

### Manual

Clone this repo into your PlatformIO project's `lib/` directory.

## Quick Start

```cpp
#include <FastLED.h>
#include "LEDBasic.h"
#include "BasicExamples/Rainbow.h"

#define NUM_LEDS 60
CRGB leds[NUM_LEDS];

BasicLEDController* controller;

void setup() {
  FastLED.addLeds<WS2812B, 5, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(64);

  controller = new BasicLEDController(leds, NUM_LEDS);
  controller->loadProgram(String(Rainbow::program));
  controller->runSetup();
}

void loop() {
  controller->runLoop(millis());
  FastLED.show();
  delay(8);
}
```

## Writing BASIC Programs

```basic
param speed number(20.0, 5.0, 100.0, 1.0)
param brightness_level number(255.0, 10.0, 255.0, 5.0)

setup
  brightness(128)
  clear()
end

loop(time)
  for i = 0 to numled()-1
    h = (i * 360 / numled() + time / speed) % 360
    sethsv(i, h, 255, brightness_level)
  next
  show()
end
```

See [docs/README_BASIC.md](docs/README_BASIC.md) for the full language reference (radians, lowercase keywords, `hsv`/`setled` color values) and [docs/LEDBasic_Prompt.md](docs/LEDBasic_Prompt.md) for an LLM prompt that matches this grammar.

## Virtual Strip Layering

```cpp
VirtualStripManager* manager = new VirtualStripManager();
manager->addPhysicalStrip(leds, NUM_LEDS);

// Layer 1: background stars (full strip, Z=0, additive blend)
VirtualStrip* stars = manager->createStrip(0, 0, NUM_LEDS, 0, BLEND_ADD);
stars->loadProgram(String(BackgroundStars::program));

// Layer 2: comet overlay (first half, Z=10, additive blend)
VirtualStrip* comet = manager->createStrip(0, 0, NUM_LEDS/2, 10, BLEND_ADD);
comet->loadProgram(String(MovingComets::program));

manager->runAllSetups();

// In loop:
manager->runAllLoops(millis());
manager->renderToPhysical();
FastLED.show();
```

## Serial Command Handler

```cpp
#include "CommandHandler.h"

// Register your app's program loader and parameter display functions
initCommandHandler({
    .onLoadProgram = [](int strip, int prog) { /* load program */ },
    .onDisplayParameters = [](int strip) { /* show params */ }
});

// In loop:
processSerialInput(controllers, numStrips);
```

Commands (newline-terminated, no blocking read):
- `0:3` — Load program 3 on strip 0
- `0:p` — Display parameters for strip 0
- `0:speed=30` — Set parameter `speed` to 30 on strip 0

## Dependencies

- [FastLED](https://github.com/FastLED/FastLED) ^3.9.0
- Arduino framework
- ESP32 platform (ESP-IDF hardware RNG used when available, falls back to Arduino `random()`)

## Tests

Host tests mock Arduino/FastLED and cover parameter parsing, color values, hue wrap, `random(0)`, power precedence, program reload, and virtual-strip `show()` isolation:

```
pwsh test/native/run.ps1
```

`g++` or `clang++` is required (MSYS2 `C:\msys64\ucrt64\bin` is added automatically on Windows).

## Host editor and simulator

Write and debug programs in the terminal without a board.

**From VS Code:** `Ctrl+Shift+B` builds, `F5` builds and opens **LEDBasic TUI in a new console window** (Windows Terminal if you have it). It is a terminal app, not a GUI. If an old F5 left the status bar orange, `Shift+F5` stops that session first. Details: [tools/README.md](tools/README.md).

**From a shell:** CMake + a C++17 compiler (MSYS2 `g++` on Windows):

```
pwsh -File tools/build.ps1
tools/build/ledbasic run examples/bas/Rainbow.bas --frames 5 --dump-ascii
tools/build/tui/ledbasic-tui
```

`ledbasic-tui` is a QBasic-inspired TUI: editor, live LED strip, F5 run, F8 step, F9 breakpoints, Immediate window.

## License

MIT
