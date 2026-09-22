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
2. Set the segment effect to **LEDBasic**.
3. Open `http://<wled-ip>/ledbasic` on a phone or desktop browser. That page lists the built-in scripts, edits scripts stored on the device, and shows a live strip preview.
4. In **Usermod settings**, the program dropdown lists built-ins and saved scripts by name, and links to the same editor.

The first number parameters of the running script map to Speed, Intensity, Custom 1, and Custom 2 (0–255 scaled into each param’s min/max). Boolean params map to the three checkboxes. The phone page edits the real parameter values.

JSON (`/json/state`):

```json
{ "LEDBasic": { "programName": "Rainbow", "params": { "speed": 30 } } }
```

`program` is still accepted as an index into the built-in list (`0` is Rainbow). `programName` wins when both are present. Named `params` are applied to live sessions.

Saved scripts live in `/ledbasic/<Name>.bas` on the device filesystem. Names are `[A-Za-z][A-Za-z0-9_]{0,22}`. Built-in names cannot be overwritten; duplicate one under a new name. At most 24 saved scripts, 8 KiB each. A settings PIN blocks saving, deleting, and reading saved source. Built-in source, the script list, the preview, and `/json/state` stay as open as WLED’s state API.

| Method | Path | |
|---|---|---|
| GET | `/ledbasic` | Phone editor |
| GET | `/ledbasic/programs` | `{ "active", "programs": [ { "name", "origin", "bytes" } ] }` |
| GET | `/ledbasic/program?name=` | Script text |
| PUT | `/ledbasic/program?name=` | Raw script body. `202` after it is stored. Parse errors show up once that script is running. |
| DELETE | `/ledbasic/program?name=` | Delete a saved script. Deleting the active one falls back to Rainbow. |
| GET | `/ledbasic/frame` | `{ "program", "n", "bri", "error", "rgb", "params" }` with `rgb` base64 of `n*3` bytes, `n` capped at 512 |

The Info page shows the current program name and the last load/runtime error, if any.

The desktop TUI can push and pull the same catalog. Run it with `--device http://<wled-ip>`, or set the host from the Device menu. The address is remembered in `%APPDATA%\LEDBasic\device.url`. Stepping and breakpoints stay on the PC; the device runs the effect.

## Notes

- BASIC `delay()` is ignored so the WLED UI does not freeze.
- BASIC `brightness()` scales this effect’s pixels only; WLED’s global brightness still applies.
- `show()` does not drive the strip; WLED copies the interpreter buffer after each frame.
- Author scripts in the phone editor or the desktop TUI. Saving writes `/ledbasic/<Name>.bas` and the running script reloads on the next frame.
