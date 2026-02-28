#include "Effects.h"
#include <Arduino.h>
#include "fx/1d/demoreel100.h"

#define MAX_LEDS_PER_STRIP 337
#define FRAMES_PER_SECOND  120

CRGB Wheel(short WheelPos) {
  if(WheelPos < 85) {
      return CRGB(
        (WheelPos * 3),
        (255 - WheelPos * 3),
        0);
  } else if(WheelPos < 170) {
      WheelPos -= 85;
      return CRGB (
        (255 - WheelPos * 3),
        0,
        (WheelPos * 3));
  } else {
      WheelPos -= 170;
      return CRGB(
        0,
        (WheelPos * 3),
        (255 - WheelPos * 3));
  }
}

void DoubleRainbowForever(int num_leds, CRGB* leds)
{
    int ob = 0;

    while (1) {
        uint16_t i, j, k;
        k = 256;
        bool a = false;

        for(j=0; j<256; j++)
        {
            a = false;
            k--;

            for(i = 0; i < num_leds; i += 1)
            {
                a = !a;
                if(a)
                  leds[i] = Wheel(((i * 256 / num_leds) + j) & 255);
                else
                  leds[i] = Wheel(((i * 256 / num_leds) + k) & 255);
            }

            FastLED.show();
            delay(20);

            // if(j % 10 == 0)
            // {
            //     onboard[0] = Wheel(ob);
            //     FastLED.show();

            //     ob+=1;
            //     if(ob > 255)
            //         ob = 0;
            // }
        }
    }
}

void ColorOrderTest(int num_leds, CRGB* leds)
{
  static bool test1 = true;
  if(test1)
  {
    leds[0] = CRGB::White;

    leds[1] = CRGB::Red;

    leds[2] = CRGB::Green;
    leds[3] = CRGB::Green;

    leds[4] = CRGB::Blue;
    leds[5] = CRGB::Blue;
    leds[6] = CRGB::Blue;

    leds[7] = CRGB::White;

    for(int i = 8; i < num_leds; i += 1)
    {
      leds[i] = CRGB::Black;
    }

    FastLED.show();
  }
  else
  {
    for(int i = 0; i < num_leds; i += 1)
    {
      leds[i] = CRGB::White;
    }

    FastLED.show();
  }
  test1 = !test1;
}

void FastLEDDemoLoop(fl::DemoReel100Ptr demoReel, int num_leds, CRGB* leds)
{
  //Run the DemoReel100 draw function
  demoReel->draw(fl::Fx::DrawContext(millis(), leds));

  // send the 'leds' array out to the actual LED strip
  FastLED.show();
}

