#include "ledbasic_programs.h"

#include "BasicExamples/BackgroundStars.h"
#include "BasicExamples/BikeParked.h"
#include "BasicExamples/BikeRolling.h"
#include "BasicExamples/Breathing.h"
#include "BasicExamples/DoubleRainbow.h"
#include "BasicExamples/Matrix.h"
#include "BasicExamples/MovingComets.h"
#include "BasicExamples/PulsingCenter.h"
#include "BasicExamples/Rainbow.h"
#include "BasicExamples/SineWave.h"

#include <cstring>

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

static bool isNameChar(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

bool ledbasicValidUserName(const char* name) {
  if (!name || !name[0]) return false;
  if (!((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z'))) return false;
  int n = 0;
  for (const char* p = name; *p; ++p, ++n) {
    if (n >= LEDBASIC_NAME_LEN - 1) return false;
    if (!isNameChar(*p)) return false;
  }
  return true;
}

int ledbasicBuiltinCount() { return kProgramCount; }

const char* ledbasicBuiltinName(int index) {
  if (index < 0 || index >= kProgramCount) return "";
  return kPrograms[index].name;
}

const char* ledbasicBuiltinSource(int index) {
  if (index < 0 || index >= kProgramCount) return "";
  return kPrograms[index].source;
}

int ledbasicBuiltinBytes(int index) {
  const char* s = ledbasicBuiltinSource(index);
  if (!s || !s[0]) return 0;
  return (int)std::strlen(s);
}

int ledbasicBuiltinIndex(const char* name) {
  if (!name || !name[0]) return -1;
  for (int i = 0; i < kProgramCount; i++) {
    if (std::strcmp(kPrograms[i].name, name) == 0) return i;
  }
  return -1;
}

const char* ledbasicStoreStatusText(int status) {
  switch (status) {
    case LEDBASIC_STORE_OK: return "ok";
    case LEDBASIC_STORE_BAD_NAME: return "bad name";
    case LEDBASIC_STORE_BUILTIN: return "built-in names cannot be overwritten";
    case LEDBASIC_STORE_TOO_MANY: return "too many scripts";
    case LEDBASIC_STORE_TOO_BIG: return "script is longer than 8192 bytes";
    case LEDBASIC_STORE_FS: return "filesystem error";
    case LEDBASIC_STORE_MISSING: return "missing script";
    default: return "error";
  }
}

bool ledbasicProgramExists(const LedBasicFs* fs, const char* name) {
  if (ledbasicBuiltinIndex(name) >= 0) return true;
  if (!name || !fs || !fs->read) return false;
  return fs->read(fs->user, name, nullptr, 0) >= 0;
}

struct CountVisit {
  const char* name;
  int count;
  bool replacing;
};

static bool countUser(const char* name, int, void* user) {
  CountVisit* c = static_cast<CountVisit*>(user);
  if (!ledbasicValidUserName(name)) return true;
  if (ledbasicBuiltinIndex(name) >= 0) return true;
  c->count++;
  if (c->name && std::strcmp(c->name, name) == 0) c->replacing = true;
  return true;
}

int ledbasicUserWrite(const LedBasicFs* fs, const char* name, const char* source, int len) {
  if (!ledbasicValidUserName(name)) return LEDBASIC_STORE_BAD_NAME;
  if (ledbasicBuiltinIndex(name) >= 0) return LEDBASIC_STORE_BUILTIN;
  if (!source || len < 0 || len > LEDBASIC_MAX_SOURCE) return LEDBASIC_STORE_TOO_BIG;
  if (!fs || !fs->write || !fs->list) return LEDBASIC_STORE_FS;

  CountVisit count = { name, 0, false };
  if (!fs->list(fs->user, countUser, &count)) return LEDBASIC_STORE_FS;
  if (!count.replacing && count.count >= LEDBASIC_MAX_USER) return LEDBASIC_STORE_TOO_MANY;
  if (!fs->write(fs->user, name, source, len)) return LEDBASIC_STORE_FS;
  return LEDBASIC_STORE_OK;
}

int ledbasicUserRead(const LedBasicFs* fs, const char* name, char* buf, int cap, int* outLen) {
  if (outLen) *outLen = 0;
  if (!ledbasicValidUserName(name)) return LEDBASIC_STORE_BAD_NAME;
  if (ledbasicBuiltinIndex(name) >= 0) return LEDBASIC_STORE_BUILTIN;
  if (!fs || !fs->read || !buf || cap <= 0) return LEDBASIC_STORE_FS;
  int n = fs->read(fs->user, name, buf, cap);
  if (n == -1) return LEDBASIC_STORE_MISSING;
  if (n < 0) return LEDBASIC_STORE_TOO_BIG;
  if (n < cap) buf[n] = 0;
  if (outLen) *outLen = n;
  return LEDBASIC_STORE_OK;
}

int ledbasicUserRemove(const LedBasicFs* fs, const char* name) {
  if (!ledbasicValidUserName(name)) return LEDBASIC_STORE_BAD_NAME;
  if (ledbasicBuiltinIndex(name) >= 0) return LEDBASIC_STORE_BUILTIN;
  if (!fs || !fs->remove || !fs->read) return LEDBASIC_STORE_FS;
  if (fs->read(fs->user, name, nullptr, 0) < 0) return LEDBASIC_STORE_MISSING;
  if (!fs->remove(fs->user, name)) return LEDBASIC_STORE_FS;
  return LEDBASIC_STORE_OK;
}

struct ListVisit {
  LedBasicProgramInfo* out;
  int n;
  int maxOut;
};

static bool listUser(const char* name, int bytes, void* user) {
  ListVisit* c = static_cast<ListVisit*>(user);
  if (c->n >= c->maxOut) return false;
  if (!ledbasicValidUserName(name)) return true;
  if (ledbasicBuiltinIndex(name) >= 0) return true;
  std::strncpy(c->out[c->n].name, name, LEDBASIC_NAME_LEN - 1);
  c->out[c->n].name[LEDBASIC_NAME_LEN - 1] = 0;
  c->out[c->n].origin = LEDBASIC_ORIGIN_USER;
  c->out[c->n].bytes = bytes;
  c->n++;
  return true;
}

int ledbasicListPrograms(const LedBasicFs* fs, LedBasicProgramInfo* out, int maxOut) {
  if (!out || maxOut <= 0) return 0;
  int n = 0;
  int bc = ledbasicBuiltinCount();
  for (int i = 0; i < bc && n < maxOut; i++) {
    std::strncpy(out[n].name, ledbasicBuiltinName(i), LEDBASIC_NAME_LEN - 1);
    out[n].name[LEDBASIC_NAME_LEN - 1] = 0;
    out[n].origin = LEDBASIC_ORIGIN_BUILTIN;
    out[n].bytes = ledbasicBuiltinBytes(i);
    n++;
  }
  if (!fs || !fs->list || n >= maxOut) return n;
  ListVisit ctx = { out, n, maxOut };
  fs->list(fs->user, listUser, &ctx);
  return ctx.n;
}

bool ledbasicResolveSource(const LedBasicFs* fs, const char* name, const char** builtinOut,
                           char* userBuf, int userCap, int* userLen) {
  if (builtinOut) *builtinOut = nullptr;
  if (userLen) *userLen = 0;
  int idx = ledbasicBuiltinIndex(name);
  if (idx >= 0) {
    const char* src = ledbasicBuiltinSource(idx);
    if (!src || !src[0]) return false;
    if (builtinOut) *builtinOut = src;
    return true;
  }
  if (!ledbasicValidUserName(name) || !fs || !fs->read || !userBuf || userCap <= 1) return false;
  int n = fs->read(fs->user, name, userBuf, userCap - 1);
  if (n < 0) return false;
  userBuf[n] = 0;
  if (userLen) *userLen = n;
  return true;
}
