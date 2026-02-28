# ESP32 BASIC LED Animation Language

This project implements a simple BASIC-like programming language specifically designed for creating LED animations on ESP32 microcontrollers using the FastLED library.

## Features

### Language Features
- **Variables**: Numeric and string variables
- **Math Operations**: `+`, `-`, `*`, `/`, `%` (modulo), `**` (power)
- **Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Logic**: `and`, `or`, `not`
- **Control Flow**: `if/else`, `while`, `for/to/step/next`
- **Functions**: Built-in math and LED control functions

### Math Functions
- `sin(x)`, `cos(x)`, `tan(x)` - Trigonometric functions
- `sqrt(x)` - Square root
- `pow(x, y)` - Power function
- `log(x)` - Base-10 logarithm
- `ln(x)` - Natural logarithm
- `abs(x)` - Absolute value
- `floor(x)`, `ceil(x)`, `round(x)` - Rounding functions
- `min(x, y)`, `max(x, y)` - Minimum and maximum
- `random()` - Random number 0.0 to 1.0 (uses ESP32 hardware RNG)
- `random(max)` - Random integer 0 to max-1

### LED Control Functions
- `setled(index, r, g, b)` - Set individual LED color
- `sethsv(index, h, s, v)` - Set LED using HSV color (h: 0-360, s,v: 0-255)
- `clear()` - Turn off all LEDs
- `fill(r, g, b)` - Fill all LEDs with a color
- `show()` - Update the LED strip (call after changes)
- `brightness(value)` - Set global brightness (0-255)
- `numled()` - Get number of LEDs in strip
- `rgb(r, g, b)` - Create RGB color
- `hsv(h, s, v)` - Create HSV color (h: 0-360, s,v: 0-255)

### Built-in Constants
- `PI` - 3.14159...
- `E` - 2.71828...

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

1. Connect your LED strip to pin 5 (or modify `strip_pin` in main.cpp)
2. Connect onboard LED indicator to pin 48 (ESP32-S3) or 21 (ESP32-S3-NANO)
3. Power your LED strip appropriately for your number of LEDs

## Usage

### Loading Programs via Serial Monitor

1. Open Serial Monitor at 115200 baud
2. Send one of these commands:
   - `0` - Load rainbow program
   - `1` - Load sine wave program
   - `2` - Load breathing program
   - `3` - Load double rainbow program
   - `4` - Load Matrix digital rain program
   - `5` - Run layered virtual-strip demo
   - `6` - Run overlapping regions demo
   - `7` - Run four-strip segmented demo

### Four-Strip Segment Demo

The `setupFourStripDemo()` function divides each of the four physical strips into
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

To run the demo, send `0:7` over the Serial Monitor.

### Programming Your Own Animations

1. Modify the program strings in `main.cpp`
2. Or implement a way to load programs from SD card, WiFi, etc.
3. Use the example programs in `examples/BasicPrograms.txt` as templates

## Language Syntax

### Variables
```basic
x = 42
name = "Hello"
result = sin(x) + cos(x)
```

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

### Comments
```basic
// This is a single-line comment
x = 42  // Comment at end of line
```

## Performance Notes

- The interpreter is designed for simplicity, not maximum performance
- Complex programs may affect frame rate
- Use `show()` strategically - calling it too often can slow things down
- The interpreter automatically calls `show()` at the end of each loop if you don't call it explicitly

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

## Troubleshooting

### Program Won't Load
- Check syntax - missing `end` statements are common
- Verify all parentheses are balanced
- Check that all variables are defined before use

### LEDs Not Responding
- Ensure `show()` is called after LED changes
- Check LED strip connections and power
- Verify `numled()` matches your actual LED count

### Performance Issues
- Reduce complexity in loop section
- Use fewer mathematical operations per frame
- Consider optimizing color calculations
