#ifndef LEDBASIC_PROGRAMS_H
#define LEDBASIC_PROGRAMS_H

#include <stddef.h>
#include <stdint.h>

// Name buffers are LEDBASIC_NAME_LEN bytes including the NUL.
// A legal name is [A-Za-z][A-Za-z0-9_]{0,22}.
static const int LEDBASIC_NAME_LEN = 24;
static const int LEDBASIC_MAX_USER = 24;
static const int LEDBASIC_MAX_SOURCE = 8192;

enum LedBasicOrigin : uint8_t {
  LEDBASIC_ORIGIN_BUILTIN = 0,
  LEDBASIC_ORIGIN_USER = 1
};

enum LedBasicStoreStatus : int {
  LEDBASIC_STORE_OK = 0,
  LEDBASIC_STORE_BAD_NAME = 1,
  LEDBASIC_STORE_BUILTIN = 2,
  LEDBASIC_STORE_TOO_MANY = 3,
  LEDBASIC_STORE_TOO_BIG = 4,
  LEDBASIC_STORE_FS = 5,
  LEDBASIC_STORE_MISSING = 6
};

struct LedBasicProgramInfo {
  char name[LEDBASIC_NAME_LEN];
  uint8_t origin;
  int bytes;
};

// visit returns false to stop listing.
typedef bool (*LedBasicVisitFn)(const char* name, int bytes, void* visitUser);

// read: copy the file into buf when buf != nullptr and cap >= length.
// Returns the byte length when the file exists and either buf is nullptr
// or cap >= length. Returns -1 when missing, -2 when buf is set and cap < length.
struct LedBasicFs {
  void* user;
  bool (*list)(void* user, LedBasicVisitFn visit, void* visitUser);
  int (*read)(void* user, const char* name, char* buf, int cap);
  bool (*write)(void* user, const char* name, const char* data, int len);
  bool (*remove)(void* user, const char* name);
};

bool ledbasicValidUserName(const char* name);
int ledbasicBuiltinCount();
const char* ledbasicBuiltinName(int index);
const char* ledbasicBuiltinSource(int index);
int ledbasicBuiltinBytes(int index);
int ledbasicBuiltinIndex(const char* name);

bool ledbasicProgramExists(const LedBasicFs* fs, const char* name);
int ledbasicUserWrite(const LedBasicFs* fs, const char* name, const char* source, int len);
int ledbasicUserRead(const LedBasicFs* fs, const char* name, char* buf, int cap, int* outLen);
int ledbasicUserRemove(const LedBasicFs* fs, const char* name);
int ledbasicListPrograms(const LedBasicFs* fs, LedBasicProgramInfo* out, int maxOut);

// On a built-in, *builtinOut points at the flash source and *userLen is 0.
// On a user script, bytes are copied into userBuf (NUL terminated) and *builtinOut is null.
bool ledbasicResolveSource(const LedBasicFs* fs, const char* name, const char** builtinOut,
                           char* userBuf, int userCap, int* userLen);

const char* ledbasicStoreStatusText(int status);

#endif
