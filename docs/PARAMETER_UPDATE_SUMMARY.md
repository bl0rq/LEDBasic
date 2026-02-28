# LED Basic Parameter Management Update

## Summary

The parameter management system has been updated to declare parameters within the Basic programs themselves, rather than externally in the `loadExampleProgram` function. Parameters are now discovered automatically after `loadProgram` is called.

## Changes Made

### 1. Language Extensions

- Added `TOK_PARAM` token type for parameter declarations
- Added `NODE_PARAM_DECL` AST node type for parameter declarations  
- Added `param` keyword to the lexer's keyword map
- Added `parseParamDecl()` method to the parser

### 2. Parser Updates

- Modified `parseProgram()` to handle parameter declarations before setup/loop
- Implemented `parseParamDecl()` to parse parameter syntax:
  ```basic
  param name type(attributes...)
  ```

### 3. Interpreter Updates

- Added `processParameterDeclaration()` method to BasicInterpreter
- Modified `run()` method to process parameter declarations during AST traversal
- Parameters are automatically added to the interpreter and variables created

### 4. Parameter Syntax

The new syntax supports three parameter types:

#### Boolean Parameters
```basic
param flag boolean(default_value)
```

#### Number Parameters  
```basic
param speed number(default, min, max, step)
```

#### Enum Parameters
```basic
param color_scheme enum(["Option1", "Option2", "Option3"])
```

### 5. Updated Example Programs

All Basic example programs now include parameter declarations:

#### Rainbow.h
```basic
param speed number(20.0, 5.0, 100.0, 1.0)
param saturation number(255.0, 0.0, 255.0, 5.0)
param brightness_level number(255.0, 10.0, 255.0, 5.0)
```

#### Breathing.h
```basic
param speed number(1000.0, 200.0, 5000.0, 100.0)
param intensity number(127.0, 50.0, 255.0, 5.0)
param color_scheme enum(["Blue-Red", "Green-Red", "Yellow"])
```

#### SineWave.h
```basic
param speed number(200.0, 50.0, 1000.0, 10.0)
param frequency number(0.39, 0.1, 2.0, 0.05)
param amplitude number(127.0, 50.0, 255.0, 5.0)
param color_mode enum(["Blue-Red", "Green-Red"])
```

#### DoubleRainbow.h
```basic
param speed number(30.0, 10.0, 100.0, 1.0)
param hue_spread number(180.0, 90.0, 360.0, 10.0)
param saturation number(255.0, 100.0, 255.0, 5.0)
param brightness_level number(255.0, 50.0, 255.0, 5.0)
```

#### Matrix.h
```basic
param speed number(3.0, 1.0, 10.0, 1.0)
param density number(30.0, 5.0, 80.0, 5.0)
param tail_length number(4.0, 2.0, 10.0, 1.0)
param spawn_rate number(2000.0, 500.0, 5000.0, 100.0)
```

### 6. Main.cpp Updates

- Removed all manual parameter setup from `loadExampleProgram()`
- Parameters are now automatically discovered when `loadProgram()` is called
- Cleaner, more maintainable code

## Benefits

1. **Self-Documenting**: Programs declare their own parameters, making them easier to understand
2. **Maintainable**: No need to maintain parameter definitions in two places
3. **Discoverable**: Parameters are automatically available after loading a program
4. **Consistent**: All programs follow the same parameter declaration pattern
5. **Extensible**: Easy to add new parameter types in the future

## Usage

After loading a program, parameters are automatically available:

```cpp
// Load program (parameters are automatically parsed)
controller.loadProgram(programString);

// Get all discovered parameters
std::vector<Parameter> params = controller.getAllParameters();

// Access individual parameters
Parameter* speedParam = controller.getParameter("speed");
```

The parameters maintain the same functionality as before - they can be modified at runtime and are available as variables within the Basic programs.
