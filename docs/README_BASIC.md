# LEDBasic language reference

LEDBasic is a small BASIC-like language for FastLED animations. Programs are loaded as strings into `BasicLEDController` (or a `VirtualStrip`) and interpreted on the device.

This document describes the language **as implemented**. Angles for `sin`/`cos`/`tan` are **radians**. Keywords are **lowercase**.

## Program structure

```basic
param speed number(20.0, 5.0, 100.0, 1.0)

setup
  brightness(128)
  clear()
end

loop(time)
  for i = 0 to numled()-1
    h = (i * 360 / numled() + time / speed) % 360
    sethsv(i, h, 255, 255)
  next
  show()
end
```

- `setup` … `end` runs once via `runSetup()`.
- `loop(time)` … `end` runs every `runLoop(millis())`. `time` is milliseconds.
- `param` declarations are discovered automatically when the program loads.

## Comments

```basic
// C++-style comment
# hash comment
x = 1  // trailing comment
```

## Types and variables

- Numbers are 32-bit floats.
- Strings use double quotes with `\"`, `\n`, `\t`, `\r`, `\\`.
- Colors are a packed RGB value returned by `rgb()`, `hsv()`, `wheel()`, and `hsv_to_rgb()`.
- Arrays: `dim name(size)` then `name[i] = value` / `name[i]`. Size is capped at 1024.
- `true` and `false` are 1 and 0.
- Built-ins: `PI`, `E`.
- Assignment creates a variable: `x = 42`.
- Reserved words cannot be variable names (`if`, `for`, `sin`, `random`, `end`, …).

## Operators

| Level | Operators | Associativity |
|-------|-----------|---------------|
| Unary | `-`, `not`, `!` | right |
| Power | `**` | right |
| Factor | `*`, `/`, `%` | left |
| Term | `+`, `-` | left |
| Compare | `<`, `>`, `<=`, `>=` | left |
| Equality | `==`, `!=` | left |
| And | `and` | left |
| Or | `or` | left |

`%` is floating modulo (`fmod`). Divide or modulo by zero yields 0.

## Control flow

```basic
if x > 10
  // then
else
  // else
end

while x < 100
  x = x + 1
end

for i = 0 to 10 step 2
  // 0, 2, 4, 6, 8, 10
next
```

`while` and `for` stop after 10,000 iterations per frame (protects the watchdog). A `for` step of 0 is an error.

## Parameters

```basic
param enabled boolean(true)
param speed number(20.0, 5.0, 100.0, 1.0)   // default, min, max, step
param mode enum(["Rainbow", "Solid", "Fade"])
```

Negative numbers are allowed in `number(...)`. Boolean defaults accept `true`/`false`/`1`/`0`. Enums default to index 0. Each parameter is also a variable in the program.

Serial (see the example sketch):

- `0:p` — list parameters on strip 0
- `0:speed=30` — set a number
- `0:enabled=true` — set a boolean (`true`/`1`/`on`)
- `0:mode=Solid` — set an enum by name or index

## Math (radians)

| Call | Result |
|------|--------|
| `sin(x)` `cos(x)` `tan(x)` | trig |
| `sqrt(x)` | 0 if `x < 0` |
| `pow(x, y)` / `x ** y` | power |
| `log(x)` | base 10; 0 if `x <= 0` |
| `ln(x)` | natural; 0 if `x <= 0` |
| `abs` `floor` `ceil` `round` | |
| `min(a, b)` `max(a, b)` | |
| `random()` | 0.0 … 1.0 |
| `random(max)` | integer `0 .. max-1`; `max <= 0` → 0 |
| `map(v, in0, in1, out0, out1)` | linear map; empty in-range → `out0` |
| `millis()` | milliseconds |
| `delay(ms)` | blocks the chip — avoid in `loop` |

## LED and color

Hue is 0–360 and wraps (including negatives). Saturation/value for `sethsv`/`hsv` are 0–255.

| Call | Result |
|------|--------|
| `numled()` / `get_led_count()` | LED count for this controller or virtual strip |
| `setled(i, r, g, b)` / `set_led(...)` | RGB 0–255 |
| `setled(i, color)` / `setcolor(i, color)` | packed color from `hsv`/`rgb`/`wheel` |
| `sethsv(i, h, s, v)` | HSV |
| `fill(r, g, b)` / `set_all(...)` / `fill(color)` | whole strip |
| `clear()` | all black |
| `show()` | commit. On a physical controller this calls `FastLED.show()`. On a virtual strip it only marks the frame complete; the manager composites and the sketch should `FastLED.show()` once. |
| `brightness(v)` | 0–255. Physical: `FastLED.setBrightness`. Virtual: scales that layer at composite time (does not change other strips). |
| `get_led_r/g/b(i)` | current buffer components |
| `rgb(r, g, b)` | color |
| `hsv(h, s, v)` | color, s/v 0–255 |
| `wheel(pos)` | rainbow color, pos 0–255 |
| `hsv_to_rgb(h, s, v)` | color. If s and v are both ≤ 100 they are treated as 0–100 (percent); otherwise 0–255. |

`show()` is optional: `runLoop` auto-shows a **physical** controller if the program did not call `show()`. Virtual strips never call `FastLED.show()` themselves.

## Virtual strips

Layering is a **C++** API (`VirtualStripManager`), not BASIC. Black pixels in `BLEND_REPLACE` are transparent so lower Z-order layers show through.

```cpp
VirtualStrip* layer = manager->createStrip(stripIndex, start, length, zOrder, BLEND_ADD);
layer->loadProgram(source);
```

Blend modes: `BLEND_REPLACE`, `BLEND_ADD`, `BLEND_SUBTRACT`, `BLEND_MULTIPLY`, `BLEND_SCREEN`, `BLEND_COLOR_SPACE`.

## Extending the language

1. Add a token to `TokenType` in `BasicInterpreter.h`
2. Add a keyword in `BasicLexer::initKeywords()`
3. Implement it in `callLedFunction()` or `callMathFunction()`

## Troubleshooting

- Program won't load: missing `end`/`next`, unbalanced `()`, or uppercase keywords (`SETUP` is not `setup`).
- LEDs stay black: the Arduino `loop()` must call `runLoop()` for physical programs, or `runAllLoops` + `renderToPhysical` + `FastLED.show()` for virtual strips. `renderToPhysical` only clears strips that have virtual layers.
- One layer changes every strip's brightness: that used to happen; virtual `brightness()` is now local to the layer.
- Frame hitches on serial: input is read byte-by-byte; send a full line ending in `\n`.
