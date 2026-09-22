#include "wled.h"
#include "ledbasic_http.h"
#include "ledbasic_page.h"
#include "ledbasic_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kDir = "/ledbasic/";

static bool scriptPath(const char* name, char* out, size_t cap) {
  if (!ledbasicValidUserName(name) || cap < 16) return false;
  int n = snprintf(out, cap, "%s%s.bas", kDir, name);
  return n > 0 && (size_t)n < cap;
}

static void scriptBaseName(const char* path, char* out, size_t cap) {
  const char* base = path ? path : "";
  for (const char* p = base; *p; ++p) {
    if (*p == '/') base = p + 1;
  }
  size_t n = strlen(base);
  if (n >= 4 && strcmp(base + n - 4, ".bas") == 0) n -= 4;
  if (n >= cap) n = cap ? cap - 1 : 0;
  if (cap) {
    memcpy(out, base, n);
    out[n] = 0;
  }
}

static bool fsList(void*, LedBasicVisitFn visit, void* visitUser) {
  if (!WLED_FS.exists("/ledbasic")) return true;
  File dir = WLED_FS.open("/ledbasic", "r");
  if (!dir) return false;
  if (!dir.isDirectory()) {
    dir.close();
    return false;
  }
  for (;;) {
    File file = dir.openNextFile();
    if (!file) break;
    char name[LEDBASIC_NAME_LEN];
    scriptBaseName(file.name(), name, sizeof(name));
    int bytes = (int)file.size();
    file.close();
    if (name[0] && !visit(name, bytes, visitUser)) break;
  }
  dir.close();
  return true;
}

static int fsRead(void*, const char* name, char* buf, int cap) {
  char path[48];
  if (!scriptPath(name, path, sizeof(path))) return -1;
  if (!WLED_FS.exists(path)) return -1;
  File file = WLED_FS.open(path, "r");
  if (!file) return -1;
  int len = (int)file.size();
  if (!buf) {
    file.close();
    return len;
  }
  if (cap < len) {
    file.close();
    return -2;
  }
  int got = len > 0 ? (int)file.read((uint8_t*)buf, (size_t)len) : 0;
  file.close();
  return got;
}

static bool fsWrite(void*, const char* name, const char* data, int len) {
  if (!WLED_FS.exists("/ledbasic") && !WLED_FS.mkdir("/ledbasic")) return false;
  char path[48];
  if (!scriptPath(name, path, sizeof(path))) return false;
  File file = WLED_FS.open(path, "w");
  if (!file) return false;
  size_t wrote = len > 0 ? file.write((const uint8_t*)data, (size_t)len) : 0;
  file.close();
  return (int)wrote == len;
}

static bool fsRemove(void*, const char* name) {
  char path[48];
  if (!scriptPath(name, path, sizeof(path))) return false;
  return WLED_FS.remove(path);
}

void ledbasicInstallDeviceFs() {
  static LedBasicFs fs = { nullptr, fsList, fsRead, fsWrite, fsRemove };
  static bool ready = false;
  if (ready) return;
  if (!WLED_FS.exists("/ledbasic")) WLED_FS.mkdir("/ledbasic");
  ledbasicWledSetFs(&fs);
  ready = true;
}

static bool writeAllowed() {
  return correctPIN || settingsPIN[0] == '\0';
}

static void sendJson(AsyncWebServerRequest* request, int code, const String& body) {
  request->send(code, F("application/json"), body);
}

static void sendError(AsyncWebServerRequest* request, int code, const char* message) {
  String body = F("{\"error\":\"");
  body += message ? message : "error";
  body += F("\"}");
  sendJson(request, code, body);
}

static bool copyQueryName(AsyncWebServerRequest* request, char* out, size_t cap) {
  if (!request->hasParam(F("name"))) return false;
  String value = request->getParam(F("name"))->value();
  if (value.length() == 0 || value.length() >= cap) return false;
  strncpy(out, value.c_str(), cap - 1);
  out[cap - 1] = 0;
  return true;
}

static void appendEscaped(String& out, const char* text) {
  if (!text) return;
  for (const char* p = text; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\') {
      out += '\\';
      out += (char)c;
    } else if (c == '\n') {
      out += F("\\n");
    } else if (c >= 0x20) {
      out += (char)c;
    }
  }
}

static void base64Encode(const uint8_t* in, int len, char* out) {
  static const char tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int o = 0;
  int i = 0;
  while (i + 2 < len) {
    uint32_t n = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
    out[o++] = tab[(n >> 18) & 63];
    out[o++] = tab[(n >> 12) & 63];
    out[o++] = tab[(n >> 6) & 63];
    out[o++] = tab[n & 63];
    i += 3;
  }
  out[o] = 0;
}

static int storeHttpStatus(int status) {
  switch (status) {
    case LEDBASIC_STORE_BAD_NAME:
    case LEDBASIC_STORE_BUILTIN:
    case LEDBASIC_STORE_TOO_MANY:
    case LEDBASIC_STORE_TOO_BIG:
      return 400;
    case LEDBASIC_STORE_MISSING:
      return 404;
    case LEDBASIC_STORE_OK:
      return 200;
    default:
      return 500;
  }
}

// Request::_tempObject is freed with free() by AsyncWebServerRequest.
struct PutMem {
  uint8_t overflow;
  size_t len;
  char data[1];
};

static void handlePrograms(AsyncWebServerRequest* request) {
  LedBasicProgramInfo info[LEDBASIC_MAX_USER + 16];
  int n = ledbasicWledListPrograms(info, LEDBASIC_MAX_USER + 16);
  String body;
  body.reserve(768);
  body += F("{\"active\":\"");
  appendEscaped(body, ledbasicWledActiveName());
  body += F("\",\"programs\":[");
  for (int i = 0; i < n; i++) {
    if (i) body += ',';
    body += F("{\"name\":\"");
    appendEscaped(body, info[i].name);
    body += F("\",\"origin\":\"");
    body += info[i].origin == LEDBASIC_ORIGIN_USER ? F("user") : F("builtin");
    body += F("\",\"bytes\":");
    body += String(info[i].bytes);
    body += '}';
  }
  body += F("]}");
  sendJson(request, 200, body);
}

static void handleGetProgram(AsyncWebServerRequest* request) {
  char name[LEDBASIC_NAME_LEN];
  if (!copyQueryName(request, name, sizeof(name))) {
    sendError(request, 400, "bad name");
    return;
  }
  int builtin = ledbasicBuiltinIndex(name);
  if (builtin >= 0) {
    request->send(200, F("text/plain"), ledbasicBuiltinSource(builtin));
    return;
  }
  if (!writeAllowed()) {
    sendError(request, 401, "denied");
    return;
  }
  char* buf = (char*)malloc(LEDBASIC_MAX_SOURCE + 1);
  if (!buf) {
    sendError(request, 500, "out of memory");
    return;
  }
  int got = 0;
  int status = ledbasicWledReadUser(name, buf, LEDBASIC_MAX_SOURCE + 1, &got);
  if (status != LEDBASIC_STORE_OK) {
    free(buf);
    sendError(request, storeHttpStatus(status), ledbasicStoreStatusText(status));
    return;
  }
  String body(buf);
  free(buf);
  request->send(200, F("text/plain"), body);
}

static void handleDeleteProgram(AsyncWebServerRequest* request) {
  if (!writeAllowed()) {
    sendError(request, 401, "denied");
    return;
  }
  char name[LEDBASIC_NAME_LEN];
  if (!copyQueryName(request, name, sizeof(name))) {
    sendError(request, 400, "bad name");
    return;
  }
  char previous[LEDBASIC_NAME_LEN];
  strncpy(previous, ledbasicWledActiveName(), sizeof(previous) - 1);
  previous[sizeof(previous) - 1] = 0;
  int status = ledbasicWledRemoveUser(name);
  if (status != LEDBASIC_STORE_OK) {
    sendError(request, storeHttpStatus(status), ledbasicStoreStatusText(status));
    return;
  }
  if (strcmp(previous, ledbasicWledActiveName()) != 0) configNeedsWrite = true;
  sendJson(request, 200, F("{\"deleted\":true}"));
}

static void handlePutProgram(AsyncWebServerRequest* request) {
  if (!writeAllowed()) {
    sendError(request, 401, "denied");
    return;
  }
  char name[LEDBASIC_NAME_LEN];
  if (!copyQueryName(request, name, sizeof(name))) {
    sendError(request, 400, "bad name");
    return;
  }
  PutMem* body = (PutMem*)request->_tempObject;
  if (body && body->overflow) {
    sendError(request, 400, "script is longer than 8192 bytes");
    return;
  }
  const char* bytes = body ? body->data : "";
  int len = body ? (int)body->len : 0;
  int status = ledbasicWledWriteUser(name, bytes, len);
  if (status != LEDBASIC_STORE_OK) {
    sendError(request, storeHttpStatus(status), ledbasicStoreStatusText(status));
    return;
  }
  sendJson(request, 202, F("{\"stored\":true}"));
}

static void handlePutBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  if (index == 0) {
    if (total > (size_t)LEDBASIC_MAX_SOURCE) {
      PutMem* mem = (PutMem*)malloc(sizeof(PutMem));
      if (mem) {
        mem->overflow = 1;
        mem->len = 0;
        mem->data[0] = 0;
        request->_tempObject = mem;
      }
      return;
    }
    PutMem* mem = (PutMem*)malloc(sizeof(PutMem) + total);
    if (!mem) return;
    mem->overflow = 0;
    mem->len = total;
    mem->data[total] = 0;
    request->_tempObject = mem;
  }
  PutMem* mem = (PutMem*)request->_tempObject;
  if (!mem || mem->overflow) return;
  if (index + len > mem->len) return;
  if (len) memcpy(mem->data + index, data, len);
  if (index + len == mem->len) mem->data[mem->len] = 0;
}

static void handleFrame(AsyncWebServerRequest* request) {
  int seg = -1;
  int count = 0;
  for (int i = 0; i < LEDBASIC_WLED_MAX_SEG; i++) {
    int n = ledbasicWledNumLeds((uint8_t)i);
    if (n > 0) {
      seg = i;
      count = n;
      break;
    }
  }
  int step = 1;
  int outN = count;
  if (count > 512) {
    step = (count + 511) / 512;
    outN = (count + step - 1) / step;
  }
  uint8_t* raw = nullptr;
  char* b64 = nullptr;
  if (outN > 0 && seg >= 0) {
    raw = (uint8_t*)malloc((size_t)outN * 3);
    b64 = (char*)malloc((size_t)outN * 4 + 8);
    if (raw && b64) {
      int written = 0;
      for (int i = 0; i < count && written < outN; i += step, ++written) {
        uint8_t r = 0, g = 0, b = 0;
        ledbasicWledGetPixel((uint8_t)seg, i, &r, &g, &b);
        raw[written * 3] = r;
        raw[written * 3 + 1] = g;
        raw[written * 3 + 2] = b;
      }
      outN = written;
      base64Encode(raw, outN * 3, b64);
    } else {
      free(raw);
      free(b64);
      raw = nullptr;
      b64 = nullptr;
      outN = 0;
    }
  }

  char err[160];
  ledbasicWledLastError(err, sizeof(err));
  String body;
  body.reserve(1024 + (b64 ? strlen(b64) : 0));
  body += F("{\"program\":\"");
  appendEscaped(body, ledbasicWledActiveName());
  body += F("\",\"n\":");
  body += String(outN);
  body += F(",\"bri\":");
  body += String(seg >= 0 ? ledbasicWledBrightness((uint8_t)seg) : 255);
  body += F(",\"error\":\"");
  appendEscaped(body, err);
  body += F("\",\"rgb\":\"");
  if (b64) body += b64;
  body += F("\",\"params\":[");
  if (seg >= 0) {
    LedBasicWledParam params[LEDBASIC_WLED_MAX_PARAMS];
    int pn = ledbasicWledGetParams((uint8_t)seg, params, LEDBASIC_WLED_MAX_PARAMS);
    for (int i = 0; i < pn; i++) {
      if (i) body += ',';
      char num[96];
      snprintf(num, sizeof(num),
               "{\"name\":\"");
      body += num;
      appendEscaped(body, params[i].name);
      snprintf(num, sizeof(num), "\",\"t\":%u,\"v\":%.4g,\"min\":%.4g,\"max\":%.4g,\"step\":%.4g}",
               (unsigned)params[i].type, params[i].value, params[i].minV, params[i].maxV,
               params[i].stepV == 0 ? 1.0 : params[i].stepV);
      body += num;
    }
  }
  body += F("]}");
  free(raw);
  free(b64);
  sendJson(request, 200, body);
}

void ledbasicHttpRegister() {
  static bool once = false;
  if (once) return;
  once = true;
  ledbasicInstallDeviceFs();

  server.on(F("/ledbasic"), HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, F("text/html"), LEDBASIC_PAGE);
  });
  server.on(F("/ledbasic/programs"), HTTP_GET, handlePrograms);
  server.on(F("/ledbasic/program"), HTTP_GET, handleGetProgram);
  server.on(F("/ledbasic/program"), HTTP_DELETE, handleDeleteProgram);
  server.on(F("/ledbasic/program"), HTTP_PUT, handlePutProgram, nullptr, handlePutBody);
  server.on(F("/ledbasic/frame"), HTTP_GET, handleFrame);
}
