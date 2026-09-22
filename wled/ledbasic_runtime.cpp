#ifndef LEDBASIC_NO_FASTLED
#define LEDBASIC_NO_FASTLED
#endif

#include "ledbasic_runtime.h"
#include "../include/BasicInterpreter.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <new>

struct Session {
  BasicLEDController* controller;
  CRGB* leds;
  int numLeds;
  char programName[LEDBASIC_NAME_LEN];
  uint32_t loadedGen;
  bool loadFailed;
  unsigned long nowMs;
};

static Session gSessions[LEDBASIC_WLED_MAX_SEG];
static char gLastError[160] = "";
static bool gInited = false;
static const LedBasicFs* gFs = nullptr;
static std::mutex gNameMu;
static std::mutex gFsMu;
static std::recursive_mutex gSessionMu;
static std::atomic<uint32_t> gReloadGen{1};
static char gActiveName[LEDBASIC_NAME_LEN] = "Rainbow";
static char gSourceBuf[LEDBASIC_MAX_SOURCE + 1];

static void setError(const char* msg) {
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
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
  for (int i = 0; i < LEDBASIC_WLED_MAX_SEG; i++) {
    gSessions[i].programName[0] = 0;
    gSessions[i].loadedGen = 0;
    gSessions[i].loadFailed = false;
  }
  gInited = true;
}

static void destroySession(Session& s) {
  delete s.controller;
  delete[] s.leds;
  s.controller = nullptr;
  s.leds = nullptr;
  s.numLeds = 0;
  s.programName[0] = 0;
  s.loadedGen = 0;
  s.loadFailed = false;
  s.nowMs = 0;
}

int ledbasicWledProgramCount() { return ledbasicBuiltinCount(); }

const char* ledbasicWledProgramName(int index) { return ledbasicBuiltinName(index); }

void ledbasicWledSetFs(const LedBasicFs* fs) { gFs = fs; }

const LedBasicFs* ledbasicWledFs() { return gFs; }

const char* ledbasicWledActiveName() {
  thread_local char buf[LEDBASIC_NAME_LEN];
  std::lock_guard<std::mutex> lock(gNameMu);
  std::strncpy(buf, gActiveName, LEDBASIC_NAME_LEN - 1);
  buf[LEDBASIC_NAME_LEN - 1] = 0;
  return buf;
}

void ledbasicWledRequestReload() {
  gReloadGen.fetch_add(1, std::memory_order_release);
}

bool ledbasicWledSetActiveName(const char* name) {
  if (!ledbasicValidUserName(name)) return false;
  std::lock_guard<std::mutex> nameLock(gNameMu);
  std::lock_guard<std::mutex> fsLock(gFsMu);
  if (!ledbasicProgramExists(gFs, name)) return false;
  if (std::strcmp(gActiveName, name) != 0) {
    std::strncpy(gActiveName, name, LEDBASIC_NAME_LEN - 1);
    gActiveName[LEDBASIC_NAME_LEN - 1] = 0;
    ledbasicWledRequestReload();
  }
  return true;
}

int ledbasicWledListPrograms(LedBasicProgramInfo* out, int maxOut) {
  std::lock_guard<std::mutex> lock(gFsMu);
  return ledbasicListPrograms(gFs, out, maxOut);
}

int ledbasicWledWriteUser(const char* name, const char* source, int len) {
  std::lock_guard<std::mutex> nameLock(gNameMu);
  std::lock_guard<std::mutex> fsLock(gFsMu);
  int st = ledbasicUserWrite(gFs, name, source, len);
  if (st == LEDBASIC_STORE_OK && name && std::strcmp(name, gActiveName) == 0) {
    gReloadGen.fetch_add(1, std::memory_order_release);
  }
  return st;
}

int ledbasicWledReadUser(const char* name, char* buf, int cap, int* outLen) {
  std::lock_guard<std::mutex> lock(gFsMu);
  return ledbasicUserRead(gFs, name, buf, cap, outLen);
}

int ledbasicWledRemoveUser(const char* name) {
  std::lock_guard<std::mutex> nameLock(gNameMu);
  std::lock_guard<std::mutex> fsLock(gFsMu);
  int st = ledbasicUserRemove(gFs, name);
  if (st == LEDBASIC_STORE_OK && name && std::strcmp(name, gActiveName) == 0) {
    std::strncpy(gActiveName, "Rainbow", LEDBASIC_NAME_LEN - 1);
    gActiveName[LEDBASIC_NAME_LEN - 1] = 0;
    gReloadGen.fetch_add(1, std::memory_order_release);
  }
  return st;
}

void ledbasicWledLastError(char* buf, size_t len) {
  if (!buf || len == 0) return;
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
  strncpy(buf, gLastError, len - 1);
  buf[len - 1] = 0;
}

void ledbasicWledDestroy(uint8_t segId) {
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
  initSessions();
  if (segId >= LEDBASIC_WLED_MAX_SEG) return;
  destroySession(gSessions[segId]);
}

void ledbasicWledResetAll() {
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
  initSessions();
  for (int i = 0; i < LEDBASIC_WLED_MAX_SEG; i++) destroySession(gSessions[i]);
}

static void copyDiag(const String& message, const char* fallback) {
  const char* msg = message.c_str();
  if (!msg || !msg[0]) msg = fallback;
  setError(msg);
}

static void markLoadFailed(Session& s, int numLeds, const char* programName, uint32_t gen) {
  destroySession(s);
  s.numLeds = numLeds;
  s.loadedGen = gen;
  s.loadFailed = true;
  if (programName) {
    std::strncpy(s.programName, programName, LEDBASIC_NAME_LEN - 1);
    s.programName[LEDBASIC_NAME_LEN - 1] = 0;
  }
}

bool ledbasicWledEnsure(uint8_t segId, int numLeds, const char* programName) {
  std::lock_guard<std::recursive_mutex> sessionLock(gSessionMu);
  initSessions();
  if (segId >= LEDBASIC_WLED_MAX_SEG) {
    setError("segment id out of range");
    return false;
  }
  if (numLeds <= 0) {
    setError("segment has no LEDs");
    return false;
  }
  if (!programName || !programName[0]) {
    setError("unknown program");
    return false;
  }

  uint32_t gen = gReloadGen.load(std::memory_order_acquire);
  Session& s = gSessions[segId];
  if (s.numLeds == numLeds && s.loadedGen == gen && std::strcmp(s.programName, programName) == 0) {
    if (s.controller) return true;
    if (s.loadFailed) return false;
  }

  const char* builtin = nullptr;
  int userLen = 0;
  bool resolved = false;
  {
    std::lock_guard<std::mutex> lock(gFsMu);
    resolved = ledbasicResolveSource(gFs, programName, &builtin, gSourceBuf,
                                     LEDBASIC_MAX_SOURCE + 1, &userLen);
  }
  if (!resolved) {
    setError("unknown program");
    markLoadFailed(s, numLeds, programName, gen);
    return false;
  }

  String source(builtin ? builtin : gSourceBuf);
  destroySession(s);

  s.leds = new (std::nothrow) CRGB[numLeds];
  if (!s.leds) {
    setError("LED buffer alloc failed");
    markLoadFailed(s, numLeds, programName, gen);
    return false;
  }
  s.controller = new (std::nothrow) BasicLEDController(s.leds, numLeds);
  if (!s.controller) {
    delete[] s.leds;
    s.leds = nullptr;
    setError("controller alloc failed");
    markLoadFailed(s, numLeds, programName, gen);
    return false;
  }

  s.controller->setOwnsPhysicalOutput(false);
  s.controller->setAutoShow(false);
  s.controller->setClock(sessionMillis, sessionDelay, &s);

  if (!s.controller->loadProgram(source)) {
    auto diags = s.controller->getDiagnostics();
    if (!diags.empty()) copyDiag(diags[0].message, "program load failed");
    else setError("program load failed");
    markLoadFailed(s, numLeds, programName, gen);
    return false;
  }

  s.controller->runSetup();
  if (s.controller->hasRuntimeError()) {
    auto diags = s.controller->getDiagnostics();
    if (!diags.empty()) copyDiag(diags[0].message, "setup runtime error");
    else setError("setup runtime error");
    markLoadFailed(s, numLeds, programName, gen);
    return false;
  }

  s.numLeds = numLeds;
  s.loadedGen = gen;
  s.loadFailed = false;
  std::strncpy(s.programName, programName, LEDBASIC_NAME_LEN - 1);
  s.programName[LEDBASIC_NAME_LEN - 1] = 0;
  gLastError[0] = 0;
  return true;
}

bool ledbasicWledEnsure(uint8_t segId, int numLeds, int programIndex) {
  const char* name = ledbasicBuiltinName(programIndex);
  if (!name || !name[0]) {
    setError("unknown program");
    return false;
  }
  return ledbasicWledEnsure(segId, numLeds, name);
}

void ledbasicWledApplySliders(uint8_t segId, uint8_t sx, uint8_t ix, uint8_t c1, uint8_t c2,
                              bool o1, bool o2, bool o3) {
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
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
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
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
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
  if (segId >= LEDBASIC_WLED_MAX_SEG || !gSessions[segId].controller) return 0;
  return gSessions[segId].numLeds;
}

uint8_t ledbasicWledBrightness(uint8_t segId) {
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
  if (segId >= LEDBASIC_WLED_MAX_SEG || !gSessions[segId].controller) return 255;
  return gSessions[segId].controller->getOutputBrightness();
}

bool ledbasicWledGetPixel(uint8_t segId, int i, uint8_t* r, uint8_t* g, uint8_t* b) {
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
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
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
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
  std::lock_guard<std::recursive_mutex> lock(gSessionMu);
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
    out[i].stepV = params[i].stepValue;
  }
  return n;
}
