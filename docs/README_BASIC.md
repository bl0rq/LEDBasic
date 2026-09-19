# ESP32 BASIC LED Animation Language

This project implements a simple BASIC-like programming language specifically designed for creating LED animations on ESP32 microcontrollers using the FastLED library.

Keywords are **lowercase**. `sin`/`cos`/`tan` take **radians**. Programs are loaded as strings into `BasicLEDController` or a `VirtualStrip` — this repo is a PlatformIO library, not a single firmware sketch. A host TUI/CLI in `tools/` runs the same interpreter without a microcontroller.

## Features

### Language Features
- **Variables**: Numeric and string variables
- **Colors**: Packed RGB values from `rgb()`, `hsv()`, `wheel()`, and `hsv_to_rgb()`
- **Math Operations**: `+`, `-`, `*`, `/`, `%` (modulo), `**` (power; higher precedence than `*`, right-associative)
- **Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Logic**: `and`, `or`, `not`
- **Control Flow**: `if/else`, `while`, `for/to/step/next`
- **Parameters**: `param name type(...)` declarations, discovered at load time
- **Functions**: Built-in math and LED control functions

### Math Functions
- `sin(x)`, `cos(x)`, `tan(x)` - Trigonometric functions (radians)
- `sqrt(x)` - Square root (`0` if `x < 0`)
- `pow(x, y)` - Power function
- `log(x)` - Base-10 logarithm (`0` if `x <= 0`)
- `ln(x)` - Natural logarithm (`0` if `x <= 0`)
- `abs(x)` - Absolute value
- `floor(x)`, `ceil(x)`, `round(x)` - Rounding functions
- `min(x, y)`, `max(x, y)` - Minimum and maximum
- `map(v, in0, in1, out0, out1)` - Linear map (`out0` if the input range is empty)
- `random()` - Random number 0.0 to 1.0 (uses ESP32 hardware RNG when available)
- `random(max)` - Random integer 0 to max-1 (`0` if `max <= 0`)
- `millis()` - Milliseconds since boot
- `delay(ms)` - Blocks the chip; avoid in `loop`

### LED Control Functions
- `setled(index, r, g, b)` - Set individual LED color
- `setled(index, color)` / `setcolor(index, color)` - Set LED from a packed color (`hsv(...)`, `rgb(...)`, `wheel(...)`)
- `sethsv(index, h, s, v)` - Set LED using HSV color (h: 0-360, wraps including negatives; s,v: 0-255)
- `clear()` - Turn off all LEDs
- `fill(r, g, b)` / `fill(color)` - Fill all LEDs with a color
- `show()` - Commit the buffer. On a physical `BasicLEDController` this calls `FastLED.show()`. On a virtual strip it only marks the frame done; the manager composites and the sketch should `FastLED.show()` once
- `brightness(value)` - 0-255. Physical: `FastLED.setBrightness` (global). Virtual: scales that layer at composite time
- `numled()` - Get number of LEDs in this controller or virtual strip
- `rgb(r, g, b)` - Create RGB color
- `hsv(h, s, v)` - Create HSV color (h: 0-360, s,v: 0-255)
- `wheel(pos)` - Rainbow color (pos: 0-255)
- `hsv_to_rgb(h, s, v)` - HSV to packed color. If s and v are both ≤ 100 they are treated as percent; otherwise 0-255
- `get_led_r(index)`, `get_led_g(index)`, `get_led_b(index)` - Read the current buffer

Aliases: `set_led`, `set_all`, `get_led_count`.

### Built-in Constants
- `PI` - 3.14159...
- `E` - 2.71828...
- `true` / `false` - 1 and 0

Function names and keywords (`if`, `for`, `end`, `sin`, `random`, …) cannot be used as variable names.

## Program Structure

Every BASIC LED program must have two main sections:

### Setup Section
```basic
setup
  // Initialization code here
  brightness(128)
  clear()
end
```

### Loop Section
```basic
loop(time)
  // Animation code here
  // 'time' parameter contains current milliseconds
  for i = 0 to numled()-1
    setled(i, 255, 0, 0)  // Set LED to red
  next
  show()
end
```

## Example Programs

Ready-to-run copies with `param` declarations live in `include/BasicExamples/`. The listings below are the same effects in minimal form.

### 1. Rainbow Animation
```basic
setup
  brightness(128)
  clear()
end

loop(time)
  for i = 0 to numled()-1
    h = (i * 360 / numled() + time / 20) % 360
    setled(i, hsv(h, 255, 255))
  next
  show()
end
```

### 2. Sine Wave Effect
```basic
setup
  brightness(128)
  clear()
end

loop(time)
  for i = 0 to numled()-1
    wave = sin(i * PI / 8 + time / 200) * 127 + 128
    setled(i, wave, 0, 255 - wave)
  next
  show()
end
```

### 3. Breathing Effect
```basic
setup
  brightness(128)
  clear()
end

loop(time)
  breath = (sin(time / 1000) + 1) * 127
  fill(breath, 0, 255 - breath)
  show()
end
```

### 4. Matrix Digital Rain
```basic
setup
  brightness(180)
  clear()
end

loop(time)
  // Fade existing pixels
  for i = 0 to numled()-1
    fadeAmount = (sin(i * 0.7 + time / 100) + 1) * 8 + 5
    currentGreen = max(0, 200 - fadeAmount)
    if currentGreen < 20
      currentGreen = 0
    end
    setled(i, 0, currentGreen, 0)
  next

  // Add random drops
  for i = 0 to 5
    if random(1000) < 15
      pos = floor(random(numled()))
      setled(pos, 100, 255, 100)
    end
  next

  show()
end
```

## Hardware Setup

The bundled sketch is `examples/VirtualStripDemo/VirtualStripDemo.ino` (not a `main.cpp` in this repo).

1. Connect WS2812B (or compatible) strips to the pins in that sketch — defaults are pin 5 and pin 48 on an ESP32-S3
2. Set `SEGMENT_LENGTH` / `NUM_LEDS` to match each physical strip
3. Power the strips appropriately for your LED count

Change `STRIP_PIN_A`, `STRIP_PIN_B`, and the FastLED chipset/order in the sketch if your wiring differs.

## Usage

### Loading Programs via Serial Monitor

1. Open Serial Monitor at 115200 baud
2. Send newline-terminated commands of the form `strip:command`:
   - `0:0` - Load rainbow program on strip 0
   - `0:1` - Load breathing program
   - `0:2` - Load sine wave program
   - `0:3` - Load double rainbow program
   - `0:4` - Load Matrix digital rain program
   - `0:5` - Run layered virtual-strip demo
   - `0:6` - Run overlapping regions demo
   - `0:7` - Run four-strip segmented demo
   - `0:p` - List parameters for the program on strip 0
   - `0:speed=30` - Set a numeric parameter

The first number is the physical strip index (`0` or `1` in the demo). A bare `0` with no colon is ignored.

### Four-Strip Segment Demo

The `setupFourStripDemo()` function divides each of the physical strips into
four equal segments and assigns a different BASIC program to each segment:

1. Rainbow animation
2. Sine wave motion
3. Breathing effect
4. Double rainbow

Segments are created with the virtual strip API:

```cpp
VirtualStrip* seg = stripManager->createStrip(stripIndex, start, length, 0, BLEND_REPLACE);
seg->loadProgram(String(source));
```

Modify the `start` and `length` values or load alternative program strings to
map different effects to any portion of a strip. Additional segments can be
added by repeating the pattern, enabling custom layouts for varied animations.

To run the demo, send `0:7` over the Serial Monitor. `BLEND_REPLACE` treats
black as transparent, so lower Z-order layers show through where a program
leaves LEDs off.

### Programming Your Own Animations

1. Copy an example from `include/BasicExamples/` or [docs/BasicPrograms.txt](BasicPrograms.txt)
2. Load the source string with `controller->loadProgram(String(source))` then `runSetup()`
3. In `loop()`, call `runLoop(millis())` for physical controllers, or `runAllLoops` + `renderToPhysical` + `FastLED.show()` when using virtual strips
4. Optionally load programs from SD card, WiFi, etc. — that I/O is application code, not part of this library

See [LEDBasic_Prompt.md](LEDBasic_Prompt.md) for an LLM prompt that matches this grammar.

## Language Syntax

### Variables
```basic
x = 42
name = "Hello"
result = sin(x) + cos(x)
```

Assignment creates a variable. Strings use double quotes (`\"`, `\n`, `\t`, `\r`, `\\`). Arrays: `dim name(size)` then `name[i]`.

### Parameters
```basic
param enabled boolean(true)
param speed number(20.0, 5.0, 100.0, 1.0)   // default, min, max, step
param mode enum(["Rainbow", "Solid", "Fade"])
```

Declare these at the top of the program. They become variables (`speed`, `mode` as an index starting at 0). Negative numbers are allowed in `number(...)`. After `loadProgram()`, C++ can list and set them with `getAllParameters()` / `setParameterValue()`.

### Control Flow
```basic
if x > 10
  // do something
else
  // do something else
end

while x < 100
  x = x + 1
end

for i = 0 to 10 step 2
  // i will be 0, 2, 4, 6, 8, 10
next
```

`while` and `for` stop after 10,000 iterations per frame so a runaway loop cannot hang the chip. A `for` step of 0 is an error.

### Comments
```basic
// This is a single-line comment
# hash comments are also accepted
x = 42  // Comment at end of line
```

## Performance Notes

- The interpreter is designed for simplicity, not maximum performance
- Complex programs may affect frame rate
- Use `show()` strategically — calling it too often can slow things down
- On a **physical** controller, the interpreter automatically calls `FastLED.show()` at the end of each loop if you don't call it explicitly
- On a **virtual** strip, `show()` does not hit the wire; one `FastLED.show()` after `renderToPhysical()` is enough
- `delay()` inside `loop` blocks every strip, not just the current program

## Extending the Language

To add new functions:

1. Add token type to `TokenType` enum in `BasicInterpreter.h`
2. Add keyword to `initKeywords()` in `BasicInterpreter.cpp`
3. Implement function in `callLedFunction()` or `callMathFunction()`

## Memory Considerations

The interpreter uses dynamic memory allocation. On ESP32:
- Monitor heap usage with complex programs
- Consider using PSRAM for larger programs
- Simplify programs if you encounter memory issues
- The lexer/parser token vector is discarded after a successful `loadProgram()`; the retained AST still contains copies of token text used by its nodes

## Troubleshooting

### Program Won't Load
- Check syntax — missing `end` / `next` statements are common
- Verify all parentheses are balanced
- Keywords must be lowercase (`setup`, not `SETUP`)
- `loadProgram()` now returns `false` on parse errors; watch Serial for `Parse error:`
- Function names cannot be reused as variables

### LEDs Not Responding
- For a physical controller, call `runLoop(millis())` every frame (the demo used to skip this)
- For virtual strips, call `runAllLoops`, `renderToPhysical()`, then `FastLED.show()`
- `renderToPhysical()` only clears strips that currently have virtual layers, so a physical program is not wiped
- Ensure `show()` is called after LED changes, or rely on physical auto-show
- Check LED strip connections and power
- Verify `numled()` matches the controller/virtual strip length (not necessarily the whole physical strip)

### Performance Issues
- Reduce complexity in loop section
- Use fewer mathematical operations per frame
- Consider optimizing color calculations
- Avoid `delay()` in `loop`
