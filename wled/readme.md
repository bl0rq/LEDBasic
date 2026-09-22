# LEDBasic WLED usermod

Runs [LEDBasic](https://github.com/bl0rq/LEDBasic) programs as a WLED effect named **LEDBasic**. WLED keeps owning LED hardware, global brightness, and `show()`. ESP32 only.

## Enable (local build)

In `C:\Users\BradBuhrkuhl\Code\3rdParty\WLED\platformio_override.ini` (gitignored):

```ini
[platformio]
default_envs = esp32dev

[env:esp32dev]
custom_usermods =
  ${common.default_usermods}
  LEDBasicWled = symlink://C:/Users/BradBuhrkuhl/Code/LEDBasic/wled
```

Needs **Node.js 20+** on PATH (WLED’s build generates `html_*.h`) and **PlatformIO Core ≥ 6.1.18** (WLED’s ESP32 platform rejects older cores).

Then from the WLED tree:

```
npm ci
npm run build
pio run -e esp32dev
```

Do not add FastLED to WLED. This usermod compiles the interpreter with `LEDBASIC_NO_FASTLED`.

## Use

1. Flash the firmware.
2. In **Usermod settings**, pick a compiled-in program (default Rainbow).
3. Set the segment effect to **LEDBasic**.
4. The first number parameters of the program map to Speed, Intensity, Custom 1, and Custom 2 (0–255 scaled into each param’s min/max). Boolean params map to the three checkboxes.

JSON (`/json/state`):

```json
{ "LEDBasic": { "program": 0, "params": { "speed": 30 } } }
```

`program` is an index into the example list. Named `params` are applied to live sessions (useful for enums and extra numbers beyond the four sliders).

The Info page shows the current program name and the last load/runtime error, if any.

## Notes

- BASIC `delay()` is ignored so the WLED UI does not freeze.
- BASIC `brightness()` scales this effect’s pixels only; WLED’s global brightness still applies.
- `show()` does not drive the strip; WLED copies the interpreter buffer after each frame.
- Author programs in the LEDBasic host TUI. Loading `.bas` from LittleFS is a later step.
