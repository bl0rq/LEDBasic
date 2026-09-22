#ifndef LEDBASIC_NO_FASTLED
#define LEDBASIC_NO_FASTLED
#endif

#include "ledbasic_runtime.h"
#include "../include/BasicInterpreter.h"
#include "BasicExamples/Rainbow.h"
#include "BasicExamples/Breathing.h"
#include "BasicExamples/SineWave.h"
#include "BasicExamples/DoubleRainbow.h"
#include "BasicExamples/Matrix.h"
#include "BasicExamples/BackgroundStars.h"
#include "BasicExamples/MovingComets.h"
#include "BasicExamples/PulsingCenter.h"
#include "BasicExamples/BikeParked.h"
#include "BasicExamples/BikeRolling.h"

#include <new>
#include <string.h>

struct ProgramEntry {
  const char* name;
  const char* source;
};

static const ProgramEntry kPrograms[] = {
  { "Rainbow", Rainbow::program },
  { "Breathing", Breathing::program },
  { "SineWave", SineWave::program },
  { "DoubleRainbow", DoubleRainbow::program },
  { "Matrix", Matrix::program },
  { "BackgroundStars", BackgroundStars::program },
  { "MovingComets", MovingComets::program },
  { "PulsingCenter", PulsingCenter::program },
  { "BikeParked", BikeParked::program },
  { "BikeRolling", BikeRolling::program }
};

static const int kProgramCount = (int)(sizeof(kPrograms) / sizeof(kPrograms[0]));

struct Session {
  BasicLEDController* controller;
  CRGB* leds;
  int numLeds;
  int programIndex;
  unsigned long nowMs;
};

static Session gSessions[LEDBASIC_WLED_MAX_SEG];
static char gLastError[160] = "";
static bool gInited = false;

static void setError(const char* msg) {
  strncpy(gLastError, msg ? msg : "", sizeof(gLastError) - 1);
  gLastError[sizeof(gLastError) - 1] = 0;
}

static unsigned long sessionMillis(void* user) {
  Session* s = static_cast<Session*>(user);
  return s ? s->nowMs : 0;
}

static void sessionDelay(int, void*) {}

static void initSessions() {
  if (gInited) return;
  memset(gSessions, 0, sizeof(gSessions));
  for (int i = 0; i < LEDBASIC_WLED_MAX_SEG; i++) gSessions[i].programIndex = -1;
  gInited = true;
}

static void destroySession(Session& s) {
  delete s.controller;
  delete[] s.leds;
  s.controller = nullptr;
  s.leds = nullptr;
  s.numLeds = 0;
  s.programIndex = -1;
  s.nowMs = 0;
}

int ledbasicWledProgramCount() { return kProgramCount; }

const char* ledbasicWledProgramName(int index) {
  if (index < 0 || index >= kProgramCount) return "";
  return kPrograms[index].name;
}

void ledbasicWledLastError(char* buf, size_t len) {
  if (!buf || len == 0) return;
  strncpy(buf, gLastError, len - 1);
  buf[len - 1] = 0;
}

void ledbasicWledDestroy(uint8_t segId) {
  initSessions();
  if (segId >= LEDBASIC_WLED_MAX_SEG) return;
  destroySession(gSessions[segId]);
}

void ledbasicWledResetAll() {
  initSessions();
  for (int i = 0; i < LEDBASIC_WLED_MAX_SEG; i++) destroySession(gSessions[i]);
}

bool ledbasicWledEnsure(uint8_t segId, int numLeds, int programIndex) {
  initSessions();
  if (segId >= LEDBASIC_WLED_MAX_SEG) {
    setError("segment id out of range");
    return false;
  }
  if (numLeds <= 0) {
    setError("segment has no LEDs");
    return false;
  }
  if (programIndex < 0 || programIndex >= kProgramCount) {
    setError("unknown program");
    return false;
  }

  Session& s = gSessions[segId];
  if (s.controller && s.numLeds == numLeds && s.programIndex == programIndex) return true;

  destroySession(s);

  s.leds = new (std::nothrow) CRGB[numLeds];
  if (!s.leds) {
    setError("LED buffer alloc failed");
    return false;
  }
  s.controller = new (std::nothrow) BasicLEDController(s.leds, numLeds);
  if (!s.controller) {
    delete[] s.leds;
    s.leds = nullptr;
    setError("controller alloc failed");
    return false;
  }

  s.controller->setOwnsPhysicalOutput(false);
  s.controller->setAutoShow(false);
  s.controller->setClock(sessionMillis, sessionDelay, &s);

  if (!s.controller->loadProgram(String(kPrograms[programIndex].source))) {
    auto diags = s.controller->getDiagnostics();
    if (!diags.empty()) {
      strncpy(gLastError, diags[0].message.c_str(), sizeof(gLastError) - 1);
      gLastError[sizeof(gLastError) - 1] = 0;
    } else {
      setError("program load failed");
    }
    destroySession(s);
    return false;
  }

  s.controller->runSetup();
  if (s.controller->hasRuntimeError()) {
    auto diags = s.controller->getDiagnostics();
    if (!diags.empty()) {
      strncpy(gLastError, diags[0].message.c_str(), sizeof(gLastError) - 1);
      gLastError[sizeof(gLastError) - 1] = 0;
    } else {
      setError("setup runtime error");
    }
    destroySession(s);
    return false;
  }

  s.numLeds = numLeds;
  s.programIndex = programIndex;
  gLastError[0] = 0;
  return true;
}

void ledbasicWledApplySliders(uint8_t segId, uint8_t sx, uint8_t ix, uint8_t c1, uint8_t c2,
                              bool o1, bool o2, bool o3) {
  if (segId >= LEDBASIC_WLED_MAX_SEG) return;
  Session& s = gSessions[segId];
  if (!s.controller) return;

  const uint8_t sliders[4] = { sx, ix, c1, c2 };
  const bool checks[3] = { o1, o2, o3 };
  int nIdx = 0;
  int bIdx = 0;
  auto params = s.controller->getAllParameters();
  for (const Parameter& p : params) {
    if (p.type == PARAM_NUMBER && nIdx < 4) {
      float t = sliders[nIdx] / 255.0f;
      float v = p.minValue + t * (p.maxValue - p.minValue);
      s.controller->setParameterValue(p.name, Value(v));
      nIdx++;
    } else if (p.type == PARAM_BOOLEAN && bIdx < 3) {
      s.controller->setParameterValue(p.name, Value(checks[bIdx] ? 1.0f : 0.0f));
      bIdx++;
    }
  }
}

bool ledbasicWledRun(uint8_t segId, unsigned long nowMs) {
  if (segId >= LEDBASIC_WLED_MAX_SEG) return false;
  Session& s = gSessions[segId];
  if (!s.controller) return false;
  s.nowMs = nowMs;
  s.controller->runLoop(nowMs);
  if (s.controller->hasRuntimeError()) {
    auto diags = s.controller->getDiagnostics();
    if (!diags.empty()) {
      strncpy(gLastError, diags[0].message.c_str(), sizeof(gLastError) - 1);
      gLastError[sizeof(gLastError) - 1] = 0;
    } else {
      setError("loop runtime error");
    }
    return false;
  }
  return true;
}

int ledbasicWledNumLeds(uint8_t segId) {
  if (segId >= LEDBASIC_WLED_MAX_SEG) return 0;
  return gSessions[segId].numLeds;
}

uint8_t ledbasicWledBrightness(uint8_t segId) {
  if (segId >= LEDBASIC_WLED_MAX_SEG || !gSessions[segId].controller) return 255;
  return gSessions[segId].controller->getOutputBrightness();
}

bool ledbasicWledGetPixel(uint8_t segId, int i, uint8_t* r, uint8_t* g, uint8_t* b) {
  if (segId >= LEDBASIC_WLED_MAX_SEG || !gSessions[segId].controller) return false;
  if (i < 0 || i >= gSessions[segId].numLeds) return false;
  const CRGB& c = gSessions[segId].leds[i];
  if (r) *r = c.r;
  if (g) *g = c.g;
  if (b) *b = c.b;
  return true;
}

bool ledbasicWledSetNamedParam(const char* name, float value) {
  if (!name) return false;
  initSessions();
  bool any = false;
  for (int i = 0; i < LEDBASIC_WLED_MAX_SEG; i++) {
    if (!gSessions[i].controller) continue;
    gSessions[i].controller->setParameterValue(String(name), Value(value));
    any = true;
  }
  return any;
}

int ledbasicWledGetParams(uint8_t segId, LedBasicWledParam* out, int maxOut) {
  if (!out || maxOut <= 0) return 0;
  if (segId >= LEDBASIC_WLED_MAX_SEG || !gSessions[segId].controller) {
    for (int i = 0; i < LEDBASIC_WLED_MAX_SEG; i++) {
      if (gSessions[i].controller) {
        segId = (uint8_t)i;
        break;
      }
    }
  }
  if (segId >= LEDBASIC_WLED_MAX_SEG || !gSessions[segId].controller) return 0;

  auto params = gSessions[segId].controller->getAllParameters();
  int n = (int)params.size();
  if (n > maxOut) n = maxOut;
  for (int i = 0; i < n; i++) {
    strncpy(out[i].name, params[i].name.c_str(), LEDBASIC_WLED_NAME_LEN - 1);
    out[i].name[LEDBASIC_WLED_NAME_LEN - 1] = 0;
    out[i].type = (uint8_t)params[i].type;
    out[i].value = params[i].currentValue.asNumber();
    out[i].minV = params[i].minValue;
    out[i].maxV = params[i].maxValue;
  }
  return n;
}
