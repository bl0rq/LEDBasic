#pragma once

#include <stdint.h>

// FastLED's 16-color palettes (MIT), plus the same lookup idea WLED uses:
// an index from 0 to 255 blends between the 16 entries. Some palettes wrap
// the last color back to the first. Heat, Lava, and Forest do not.

struct PaletteRgb {
    uint8_t r, g, b;
};

int paletteCount();
const char* paletteName(int index);
int findPalette(const char* name);
bool paletteWraps(int index);
PaletteRgb paletteColor(int index, int position, int brightness);
