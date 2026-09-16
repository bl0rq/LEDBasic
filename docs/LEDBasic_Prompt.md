# LEDBasic effect programming guide (for LLMs)

You are writing LED animation effects in **LEDBasic**, a small BASIC-like language interpreted on ESP32 with FastLED. Output a complete program using **only** the syntax below. Keywords are lowercase. `sin`/`cos`/`tan` take **radians**, not degrees.

## Required shape

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

- `setup` … `end` runs once.
- `loop(time)` … `end` runs every frame. `time` is milliseconds from `millis()`.
- Do not use `SETUP:`, `THEN`, `WEND`, or uppercase keywords.

## Parameters (optional, at the top)

```basic
param enabled boolean(true)
param speed number(default, min, max, step)
param mode enum(["Rainbow", "Solid"])
```

Parameters become variables. Enums compare as indices: `if mode == 0`.

## Control flow

```basic
if cond
  ...
else
  ...
end

while cond
  ...
end

for i = start to end step s
  ...
next
```

Comments: `//` or `#`.

## Math

`+ - * / % **`, `== != < > <= >=`, `and or not`.

`sin cos tan sqrt pow log ln abs floor ceil round min max map random millis delay`

- `random()` → 0.0..1.0
- `random(n)` → integer 0..n-1
- `map(v, in0, in1, out0, out1)`

## LEDs

`numled()` is the length of **this** strip (physical or virtual).

```basic
setled(i, r, g, b)
setled(i, hsv(h, 255, 255))
setled(i, rgb(r, g, b))
setled(i, wheel(pos))
sethsv(i, h, s, v)
fill(r, g, b)
fill(hsv(h, 255, 255))
clear()
show()
brightness(0..255)
get_led_r(i)  get_led_g(i)  get_led_b(i)
```

Hue is 0–360 (wraps). `s`/`v` for `hsv`/`sethsv` are 0–255. `wheel(pos)` uses 0–255.

Aliases: `set_led`, `set_all`, `get_led_count`, `setcolor`, `hsv_to_rgb`.

Do **not** call `CREATE_STRIP` or other virtual-strip functions from BASIC. Layering is done in C++.

## Rules for generated programs

1. Always include `setup`/`end` and `loop(time)`/`end`.
2. Prefer `sethsv` or `setled(i, hsv(...))` for color.
3. Do not use `delay()` inside `loop` unless the user asks.
4. Bound LED indices with `numled()`.
5. Keep the inner loop simple: one pass over LEDs per frame.
6. Declare tunable values with `param`, not magic numbers.
