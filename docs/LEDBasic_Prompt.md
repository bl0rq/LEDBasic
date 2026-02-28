# LED Animation BASIC Language - Effect Programming Guide

## Overview
You are tasked with writing LED animation effects using a BASIC-like programming language designed for ESP32 microcontrollers with FastLED-compatible LED strips. This language provides a simple yet powerful way to create dynamic lighting effects.

## Language Specification

### Program Structure
Every LED animation program must have two main functions:
```basic
SETUP:
  ' Initialization code - runs once at startup
END

LOOP:
  ' Animation code - runs continuously
END
```

### Available Functions

#### LED Control Functions
- `SET_LED(index, r, g, b)` - Set LED at index to RGB color (0-255 each)
- `SET_ALL(r, g, b)` - Set all LEDs to the same color
- `CLEAR()` - Turn off all LEDs (set to black)
- `SHOW()` - Update the physical LED strip with current values
- `GET_LED_COUNT()` - Returns the total number of LEDs in the strip
- `GET_LED_R(index)` - Get the red component (0-255) of LED at index
- `GET_LED_G(index)` - Get the green component (0-255) of LED at index
- `GET_LED_B(index)` - Get the blue component (0-255) of LED at index

#### Mathematical Functions
- `SIN(angle)` - Sine function (angle in degrees, returns -1 to 1)
- `COS(angle)` - Cosine function (angle in degrees, returns -1 to 1)
- `ABS(value)` - Absolute value
- `MIN(a, b)` - Returns smaller of two values
- `MAX(a, b)` - Returns larger of two values
- `MAP(value, fromLow, fromHigh, toLow, toHigh)` - Map value from one range to another
- `RANDOM(max)` - Random integer from 0 to max-1
- `SQRT(value)` - Square root

#### Color Utilities
- `HSV_TO_RGB(h, s, v)` - Convert HSV to RGB (h: 0-360, s,v: 0-100), returns as RGB values
- `WHEEL(pos)` - Rainbow color wheel (pos: 0-255), returns RGB values

#### Timing Functions
- `MILLIS()` - Current time in milliseconds since startup
- `DELAY(ms)` - Pause execution for specified milliseconds

#### System Variables (Read-only)
- `TIME` - Current time in milliseconds (equivalent to MILLIS())
- `FRAME_COUNT` - Number of animation frames since program start

### Virtual Strip System
The language supports virtual strips - logical groupings of LEDs that can overlap and blend:

#### Virtual Strip Functions
- `CREATE_STRIP(name, start, length, z_order)` - Create a virtual strip
- `SET_VIRTUAL_LED(strip_name, index, r, g, b)` - Set LED in virtual strip
- `SET_BLEND_MODE(strip_name, mode)` - Set blending mode (0=REPLACE, 1=ADD, 2=SUBTRACT, 3=MULTIPLY, 4=SCREEN)
- `GET_VIRTUAL_LED_R(strip_name, index)` - Get red component of LED in virtual strip
- `GET_VIRTUAL_LED_G(strip_name, index)` - Get green component of LED in virtual strip
- `GET_VIRTUAL_LED_B(strip_name, index)` - Get blue component of LED in virtual strip

### Parameter System
Programs can declare configurable parameters that users can adjust at runtime:

#### Parameter Declaration
```basic
PARAM speed NUMBER 1 100 10 5    ' name, type, min, max, default, step
PARAM enabled BOOLEAN TRUE       ' name, type, default
PARAM mode ENUM "rainbow,solid,fade" 0  ' name, type, options, default_index
```

#### Parameter Types
- `NUMBER` - Numeric parameter with min/max range and step size
- `BOOLEAN` - True/false parameter  
- `ENUM` - Selection from predefined string options

### Control Flow
- `IF condition THEN ... END` - Conditional execution
- `FOR variable = start TO end ... NEXT` - Loop with counter
- `WHILE condition ... WEND` - Loop while condition is true

### Variables
- Declare with `DIM variable_name`
- Assign with `variable_name = value`
- Support for numbers and basic arithmetic (+, -, *, /, MOD)

## Example Effects

### 1. Rainbow Wave
```basic
PARAM speed NUMBER 1 50 10 1
PARAM brightness NUMBER 10 255 128 5

SETUP:
  DIM offset
  offset = 0
END

LOOP:
  FOR i = 0 TO GET_LED_COUNT() - 1
    DIM hue
    hue = (i * 360 / GET_LED_COUNT() + offset) MOD 360
    DIM r, g, b
    HSV_TO_RGB(hue, 100, brightness)
    SET_LED(i, r, g, b)
  NEXT
  
  SHOW()
  offset = (offset + speed) MOD 360
  DELAY(50)
END
```

### 2. Breathing Effect
```basic
PARAM color_r NUMBER 0 255 255 1
PARAM color_g NUMBER 0 255 100 1  
PARAM color_b NUMBER 0 255 50 1
PARAM breath_speed NUMBER 1 20 5 1

SETUP:
  DIM phase
  phase = 0
END

LOOP:
  DIM brightness
  brightness = (SIN(phase) + 1) * 127.5
  
  DIM r, g, b
  r = color_r * brightness / 255
  g = color_g * brightness / 255
  b = color_b * brightness / 255
  
  SET_ALL(r, g, b)
  SHOW()
  
  phase = phase + breath_speed
  IF phase >= 360 THEN
    phase = 0
  END
  
  DELAY(50)
END
```

### 3. Matrix Rain Effect (Using Virtual Strips)
```basic
PARAM drop_chance NUMBER 1 20 5 1
PARAM fade_speed NUMBER 1 10 3 1

SETUP:
  CREATE_STRIP("background", 0, GET_LED_COUNT(), 0)
  CREATE_STRIP("drops", 0, GET_LED_COUNT(), 1)
  SET_BLEND_MODE("drops", 1)  ' ADD blending
END

LOOP:
  ' Fade background
  FOR i = 0 TO GET_LED_COUNT() - 1
    DIM current_brightness
    ' Get current brightness and fade
    SET_VIRTUAL_LED("background", i, 0, current_brightness - fade_speed, 0)
  NEXT
  
  ' Maybe spawn new drop
  IF RANDOM(100) < drop_chance THEN
    DIM spawn_pos
    spawn_pos = RANDOM(GET_LED_COUNT())
    SET_VIRTUAL_LED("drops", spawn_pos, 0, 255, 100)
  END
  
  SHOW()
  DELAY(100)
END
```

### 4. Color Smearing Effect
```basic
PARAM smear_amount NUMBER 1 10 3 1
PARAM decay NUMBER 1 20 5 1

SETUP:
  ' Start with a few random colored pixels
  FOR i = 0 TO 5
    DIM pos, r, g, b
    pos = RANDOM(GET_LED_COUNT())
    r = RANDOM(256)
    g = RANDOM(256) 
    b = RANDOM(256)
    SET_LED(pos, r, g, b)
  NEXT
  SHOW()
END

LOOP:
  ' Create new array to store next frame
  FOR i = 0 TO GET_LED_COUNT() - 1
    DIM current_r, current_g, current_b
    current_r = GET_LED_R(i)
    current_g = GET_LED_G(i)
    current_b = GET_LED_B(i)
    
    ' Sample neighboring pixels
    DIM left_idx, right_idx
    left_idx = (i - 1 + GET_LED_COUNT()) MOD GET_LED_COUNT()
    right_idx = (i + 1) MOD GET_LED_COUNT()
    
    DIM left_r, left_g, left_b
    DIM right_r, right_g, right_b
    left_r = GET_LED_R(left_idx)
    left_g = GET_LED_G(left_idx)
    left_b = GET_LED_B(left_idx)
    right_r = GET_LED_R(right_idx)
    right_g = GET_LED_G(right_idx)
    right_b = GET_LED_B(right_idx)
    
    ' Average with neighbors and apply decay
    DIM new_r, new_g, new_b
    new_r = (current_r + left_r + right_r) / 3 - decay
    new_g = (current_g + left_g + right_g) / 3 - decay
    new_b = (current_b + left_b + right_b) / 3 - decay
    
    ' Clamp to valid range
    new_r = MAX(0, new_r)
    new_g = MAX(0, new_g)
    new_b = MAX(0, new_b)
    
    SET_LED(i, new_r, new_g, new_b)
  NEXT
  
  ' Occasionally add new random colors
  IF RANDOM(100) < 2 THEN
    DIM pos, r, g, b
    pos = RANDOM(GET_LED_COUNT())
    r = RANDOM(256)
    g = RANDOM(256)
    b = RANDOM(256)
    SET_LED(pos, r, g, b)
  END
  
  SHOW()
  DELAY(100)
END
```

## Writing Guidelines

### Performance Tips
1. Use `DELAY()` to control animation speed and prevent overwhelming the microcontroller
2. Minimize complex calculations inside tight loops
3. Cache frequently used values in variables
4. Use virtual strips sparingly for complex blending effects

### Best Practices
1. Always call `SHOW()` after setting LED colors to update the display
2. Declare parameters to make effects customizable
3. Use meaningful variable names with `DIM`
4. Keep SETUP section minimal - use it only for initialization
5. Make effects scalable by using `GET_LED_COUNT()` instead of hardcoded values

### Color Management
1. RGB values range from 0-255
2. HSV hue ranges from 0-360 degrees, saturation and value from 0-100
3. Use the `WHEEL()` function for smooth rainbow transitions
4. Consider color temperature and brightness for pleasant viewing

### Debugging
1. Effects run on hardware, so test frequently
2. Use simple patterns first, then add complexity
3. Parameters allow real-time tuning without reprogramming

## Your Task
Write a creative LED animation effect using this BASIC-like language. Consider:
- What visual effect do you want to create?
- What parameters would make it customizable?
- How will it scale across different LED strip lengths?
- What timing and color schemes will make it visually appealing?

Remember to structure your code with proper SETUP and LOOP sections, use appropriate parameters, and follow the performance guidelines above.
