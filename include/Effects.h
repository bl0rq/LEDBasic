#ifndef EFFECTS_H
#define EFFECTS_H

// Opt-in extras. Do not include from LEDBasic.h — FastLED demo-reel internals
// pull extra flash, and DoubleRainbowForever() never returns (infinite loop).

#include <FastLED.h>
#include "fx/1d/demoreel100.h"

CRGB Wheel(short WheelPos);
void DoubleRainbowForever(int num_leds, CRGB* leds);
void ColorOrderTest(int num_leds, CRGB* leds);
void FastLEDDemoLoop(fl::DemoReel100Ptr demoReel, int num_leds, CRGB* leds);

#endif // EFFECTS_H
