#include "wled.h"
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
  int lastProgramIndex = 0;

  static const char _name[];
  static const char _enabled[];
  static const char _program[];

public:
  bool isEnabled() const { return enabled; }
  int program() const { return programIndex; }

  void setup() override {
    gLedBasicUm = this;
    strip.addEffect(255, &LedBasicUsermod::mode_ledbasic, _data_FX_MODE_LEDBASIC);
    initDone = true;
  }

  void loop() override {}

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray arr = user.createNestedArray(FPSTR(_name));
    arr.add(ledbasicWledProgramName(programIndex));
    char err[160];
    ledbasicWledLastError(err, sizeof(err));
    if (err[0]) arr.add(err);
    else arr.add(F("ok"));
  }

  void addToJsonState(JsonObject& root) override {
    if (!initDone || !enabled) return;
    JsonObject usermod = root[FPSTR(_name)];
    if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));
    usermod["program"] = programIndex;
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

    if (!usermod["program"].isNull()) {
      int p = usermod["program"] | programIndex;
      int maxP = ledbasicWledProgramCount() - 1;
      if (p < 0) p = 0;
      if (p > maxP) p = maxP;
      if (p != programIndex) {
        programIndex = p;
        ledbasicWledResetAll();
      }
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
    top[FPSTR(_program)] = programIndex;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[FPSTR(_name)];
    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, true);
    configComplete &= getJsonValue(top[FPSTR(_program)], programIndex, 0);
    int maxP = ledbasicWledProgramCount() - 1;
    if (programIndex < 0) programIndex = 0;
    if (programIndex > maxP) programIndex = maxP;
    if (initDone && programIndex != lastProgramIndex) {
      ledbasicWledResetAll();
    }
    lastProgramIndex = programIndex;
    return configComplete;
  }

  void appendConfigData() override {
    oappend(F("dd=addDropdown('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F("','"));
    oappend(String(FPSTR(_program)).c_str());
    oappend(F("');"));
    int n = ledbasicWledProgramCount();
    for (int i = 0; i < n; i++) {
      oappend(F("addOption(dd,'"));
      oappend(ledbasicWledProgramName(i));
      oappend(F("',"));
      oappend(String(i).c_str());
      oappend(F(");"));
    }
  }

  static void mode_ledbasic();
  static const char _data_FX_MODE_LEDBASIC[];
};

const char LedBasicUsermod::_name[] PROGMEM = "LEDBasic";
const char LedBasicUsermod::_enabled[] PROGMEM = "enabled";
const char LedBasicUsermod::_program[] PROGMEM = "program";
const char LedBasicUsermod::_data_FX_MODE_LEDBASIC[] PROGMEM =
    "LEDBasic@!,Intensity,Custom 1,Custom 2;;!;1";

void LedBasicUsermod::mode_ledbasic() {
  if (!gLedBasicUm || !gLedBasicUm->isEnabled()) FX_FALLBACK_STATIC;
  if (SEGLEN < 1) FX_FALLBACK_STATIC;

  const uint8_t seg = strip.getCurrSegmentId();
  if (!ledbasicWledEnsure(seg, (int)SEGLEN, gLedBasicUm->program())) FX_FALLBACK_STATIC;

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
