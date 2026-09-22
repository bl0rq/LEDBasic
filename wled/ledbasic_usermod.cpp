#include "wled.h"
#include "ledbasic_http.h"
#include "ledbasic_runtime.h"

static void mode_static_fallback() {
  SEGMENT.fill(SEGCOLOR(0));
}

#define FX_FALLBACK_STATIC { mode_static_fallback(); return; }

class LedBasicUsermod;

static LedBasicUsermod* gLedBasicUm = nullptr;

class LedBasicUsermod : public Usermod {
private:
  bool enabled = true;
  bool initDone = false;
  int programIndex = 0;
  char programName[LEDBASIC_NAME_LEN];

  static const char _name[];
  static const char _enabled[];
  static const char _programName[];

  void rememberActive(bool persist) {
    const char* live = ledbasicWledActiveName();
    strncpy(programName, live ? live : "Rainbow", sizeof(programName) - 1);
    programName[sizeof(programName) - 1] = 0;
    int idx = ledbasicBuiltinIndex(programName);
    programIndex = idx;
    if (persist) configNeedsWrite = true;
  }

  bool selectName(const char* name, bool persist) {
    char previous[LEDBASIC_NAME_LEN];
    strncpy(previous, ledbasicWledActiveName(), sizeof(previous) - 1);
    previous[sizeof(previous) - 1] = 0;
    if (!ledbasicWledSetActiveName(name)) return false;
    rememberActive(persist && strcmp(previous, ledbasicWledActiveName()) != 0);
    return true;
  }

  const char* activeName() const {
    if (initDone) return ledbasicWledActiveName();
    return programName[0] ? programName : "Rainbow";
  }

public:
  LedBasicUsermod() {
    strncpy(programName, "Rainbow", sizeof(programName) - 1);
    programName[sizeof(programName) - 1] = 0;
  }

  bool isEnabled() const { return enabled; }
  int program() const { return programIndex; }

  void setup() override {
    gLedBasicUm = this;
    ledbasicInstallDeviceFs();
    if (!selectName(programName, false)) selectName("Rainbow", false);
    strip.addEffect(255, &LedBasicUsermod::mode_ledbasic, _data_FX_MODE_LEDBASIC);
    initDone = true;
  }

  void loop() override { ledbasicHttpRegister(); }

  void connected() override { ledbasicHttpRegister(); }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray arr = user.createNestedArray(FPSTR(_name));
    arr.add(activeName());
    char err[160];
    ledbasicWledLastError(err, sizeof(err));
    if (err[0]) arr.add(err);
    else arr.add(F("ok"));
  }

  void addToJsonState(JsonObject& root) override {
    if (!initDone || !enabled) return;
    JsonObject usermod = root[FPSTR(_name)];
    if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));
    const char* live = activeName();
    usermod[F("programName")] = live;
    usermod[F("program")] = ledbasicBuiltinIndex(live);
    JsonObject params = usermod.createNestedObject("params");
    LedBasicWledParam list[LEDBASIC_WLED_MAX_PARAMS];
    int n = ledbasicWledGetParams(0, list, LEDBASIC_WLED_MAX_PARAMS);
    for (int i = 0; i < n; i++) {
      params[list[i].name] = list[i].value;
    }
  }

  void readFromJsonState(JsonObject& root) override {
    if (!initDone) return;
    JsonObject usermod = root[FPSTR(_name)];
    if (usermod.isNull()) return;

    bool named = false;
    if (!usermod[F("programName")].isNull()) {
      const char* wanted = usermod[F("programName")] | "";
      if (wanted[0]) {
        named = true;
        selectName(wanted, true);
      }
    }
    if (!named && !usermod[F("program")].isNull()) {
      int p = usermod[F("program")] | -1;
      if (p >= 0 && p < ledbasicWledProgramCount()) selectName(ledbasicBuiltinName(p), true);
    }

    JsonObject params = usermod["params"];
    if (!params.isNull()) {
      for (JsonPair kv : params) {
        ledbasicWledSetNamedParam(kv.key().c_str(), kv.value().as<float>());
      }
    }
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top[FPSTR(_programName)] = activeName();
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[FPSTR(_name)];
    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, true);

    char parsed[LEDBASIC_NAME_LEN];
    parsed[0] = 0;
    bool haveName = false;
    if (!top[FPSTR(_programName)].isNull()) {
      const char* raw = top[FPSTR(_programName)] | "";
      strncpy(parsed, raw, sizeof(parsed) - 1);
      parsed[sizeof(parsed) - 1] = 0;
      haveName = parsed[0] != 0;
    }
    configComplete &= haveName;
    if (!haveName) {
      int legacy = 0;
      getJsonValue(top[F("program")], legacy, 0);
      const char* builtin = ledbasicBuiltinName(legacy);
      strncpy(parsed, (builtin && builtin[0]) ? builtin : "Rainbow", sizeof(parsed) - 1);
      parsed[sizeof(parsed) - 1] = 0;
    }
    if (!ledbasicValidUserName(parsed)) {
      strncpy(parsed, "Rainbow", sizeof(parsed) - 1);
      parsed[sizeof(parsed) - 1] = 0;
    }
    strncpy(programName, parsed, sizeof(programName) - 1);
    programName[sizeof(programName) - 1] = 0;
    programIndex = ledbasicBuiltinIndex(programName);
    if (initDone && !selectName(programName, false)) selectName("Rainbow", false);
    return configComplete;
  }

  void appendConfigData() override {
    oappend(F("dd=addDropdown('LEDBasic','programName');"));
    LedBasicProgramInfo info[LEDBASIC_MAX_USER + 16];
    int n = ledbasicWledListPrograms(info, LEDBASIC_MAX_USER + 16);
    for (int i = 0; i < n; i++) {
      oappend(F("addOption(dd,'"));
      oappend(info[i].name);
      oappend(F("','"));
      oappend(info[i].name);
      oappend(F("');"));
    }
    oappend(F("addInfo('LEDBasic:programName',0,'<a href=\"/ledbasic\">Open editor</a>');"));
  }

  static void mode_ledbasic();
  static const char _data_FX_MODE_LEDBASIC[];
};

const char LedBasicUsermod::_name[] PROGMEM = "LEDBasic";
const char LedBasicUsermod::_enabled[] PROGMEM = "enabled";
const char LedBasicUsermod::_programName[] PROGMEM = "programName";
const char LedBasicUsermod::_data_FX_MODE_LEDBASIC[] PROGMEM =
    "LEDBasic@!,Intensity,Custom 1,Custom 2;;!;1";

void LedBasicUsermod::mode_ledbasic() {
  if (!gLedBasicUm || !gLedBasicUm->isEnabled()) FX_FALLBACK_STATIC;
  if (SEGLEN < 1) FX_FALLBACK_STATIC;

  const uint8_t seg = strip.getCurrSegmentId();
  if (!ledbasicWledEnsure(seg, (int)SEGLEN, ledbasicWledActiveName())) FX_FALLBACK_STATIC;

  ledbasicWledApplySliders(seg, SEGMENT.speed, SEGMENT.intensity, SEGMENT.custom1, SEGMENT.custom2,
                           SEGMENT.check1, SEGMENT.check2, SEGMENT.check3);

  if (!ledbasicWledRun(seg, strip.now)) FX_FALLBACK_STATIC;

  const int n = ledbasicWledNumLeds(seg);
  const uint8_t bri = ledbasicWledBrightness(seg);
  for (int i = 0; i < n; i++) {
    uint8_t r, g, b;
    if (!ledbasicWledGetPixel(seg, i, &r, &g, &b)) break;
    if (bri != 255) {
      r = (uint8_t)((r * bri) / 255);
      g = (uint8_t)((g * bri) / 255);
      b = (uint8_t)((b * bri) / 255);
    }
    SEGMENT.setPixelColor(i, RGBW32(r, g, b, 0));
  }
}

static LedBasicUsermod ledbasic_um;
REGISTER_USERMOD(ledbasic_um);
