#ifndef LEDBASIC_PIXELS_H
#define LEDBASIC_PIXELS_H

// Pixel types for the interpreter.
// Default: FastLED's CRGB/CHSV.
// LEDBASIC_NO_FASTLED: built-in LedBasicCrgb.h (WLED usermod, compile checks).
// Do not mix this header with WLED's wled.h in the same translation unit.

#ifdef LEDBASIC_NO_FASTLED
#include "LedBasicCrgb.h"
#else
#include <FastLED.h>
#endif

#endif
