# LEDBasic

A **BASIC language interpreter** for programming LED animations with [FastLED](https://github.com/FastLED/FastLED). Features virtual strip layering with blend modes, runtime parameter discovery, serial command interface, and 63 ready-to-use example programs. Designed as a PlatformIO library for easy integration into LED projects.

## Features

- **BASIC Interpreter** — Write LED animation programs in a simple BASIC-like language with `setup`/`loop` structure
- **Parameter Discovery** — Programs declare tunable parameters (`param speed number(20, 5, 100, 1)`) that are automatically discovered and adjustable at runtime
- **Virtual Strip Manager** — Layer multiple BASIC programs on overlapping regions of a physical LED strip with Z-ordering and blend modes (replace, add, subtract, multiply, screen). Replace treats black as transparent so lower layers show through.
- **Serial Command Handler** — Built-in serial interface for switching programs and adjusting parameters at runtime
- **Standalone Effects** — Optional extras (`#include "Effects.h"`) for color-wheel / demo-reel tests. Not pulled in by `LEDBasic.h`.
- **Example Programs** — 63 ready-to-use BASIC programs in `examples/bas`, each available as `BasicExamples/<Name>.h` (Rainbow, Fire, Pacifica, Plasma, Twinkle, ...)

## Installation

**Compiler requirement:** the `BasicExamples/*.h` headers load their `.bas` files with C23 `#embed`, which needs **GCC 15+ or Clang 19+**. The stock `espressif32` PlatformIO platform ships an older GCC; the firmware in `firmware/` uses the [Tasmota espressif32 platform](https://github.com/tasmota/platform-espressif32) (GCC 15.2). The interpreter itself needs only C++17, so a project that doesn't include `BasicExamples/` builds on older compilers.

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

- [FastLED](https://github.com/FastLED/FastLED) ^3.10.0
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

**From a shell:** CMake + GCC 15+ or Clang 19+ for `#embed` (MSYS2 `g++` on Windows; `g++`/`clang++` on Linux/macOS):

```
pwsh -File tools/build.ps1   # Windows
tools/build.sh               # Linux/macOS
tools/build/ledbasic run examples/bas/Rainbow.bas --frames 5 --dump-ascii
tools/build/tui/ledbasic-tui
```

`ledbasic-tui` is a QBasic-inspired TUI: editor, live LED strip, F5 run, F8 step, F9 breakpoints, Immediate window.

## Device firmware

`firmware/` is a PlatformIO image for a classic ESP32. It runs LEDBasic on one strip and serves a setup page and JSON API over Wi-Fi.

A new board opens an access point named `LEDBasic-XXXX` with password `ledbasic`. The page is at http://192.168.4.1. After the board joins a network it is at http://ledbasic.local as well as its station IP. Hold the GPIO17 button for five seconds to forget the network and open the access point again.

From `firmware/`, to flash or update the firmware:

```
pio run -e esp32dev -t upload
```

That is all a routine update needs. Stored programs and settings are kept.

On a brand-new board, or to wipe a board whose storage is corrupt, also flash the filesystem image once:

```
pio run -e esp32dev -t uploadfs
```

**`uploadfs` replaces the whole LittleFS partition with an empty image and erases every stored user program.** Download any programs you want to keep first (Device > Programs in the TUI, or `GET /api/program?name=<name>`).

Factory LED settings are WS2812, color order GRB, GPIO16, 60 LEDs. Change type, order, length, and pin on the page or with `PUT /api/led`. The 63 programs in `examples/bas` are built in. User programs are stored on the device.

**Security note:** the web UI and JSON API have no authentication or CSRF
protection. Anyone on the same Wi-Fi network can change settings, upload or
activate programs, or restart the device. Only run this on a trusted home
network — do not port-forward or otherwise expose it to the internet.

## License

MIT
