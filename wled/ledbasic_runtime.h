#ifndef LEDBASIC_RUNTIME_H
#define LEDBASIC_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

// C++ API used by the WLED usermod. Does not include LEDBasic or wled headers
// so CRGB types never collide.

static const int LEDBASIC_WLED_MAX_SEG = 32;
static const int LEDBASIC_WLED_MAX_PARAMS = 16;
static const int LEDBASIC_WLED_NAME_LEN = 24;

struct LedBasicWledParam {
  char name[LEDBASIC_WLED_NAME_LEN];
  uint8_t type; // 0 boolean, 1 number, 2 enum
  float value;
  float minV;
  float maxV;
};

int ledbasicWledProgramCount();
const char* ledbasicWledProgramName(int index);

bool ledbasicWledEnsure(uint8_t segId, int numLeds, int programIndex);
void ledbasicWledApplySliders(uint8_t segId, uint8_t sx, uint8_t ix, uint8_t c1, uint8_t c2,
                              bool o1, bool o2, bool o3);
bool ledbasicWledRun(uint8_t segId, unsigned long nowMs);
int ledbasicWledNumLeds(uint8_t segId);
uint8_t ledbasicWledBrightness(uint8_t segId);
bool ledbasicWledGetPixel(uint8_t segId, int i, uint8_t* r, uint8_t* g, uint8_t* b);
void ledbasicWledDestroy(uint8_t segId);
void ledbasicWledResetAll();

bool ledbasicWledSetNamedParam(const char* name, float value);
int ledbasicWledGetParams(uint8_t segId, LedBasicWledParam* out, int maxOut);
void ledbasicWledLastError(char* buf, size_t len);

#endif
