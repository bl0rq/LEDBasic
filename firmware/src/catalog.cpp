#include "catalog.h"

#include <LittleFS.h>

#include "BasicExamples/Aurora.h"
#include "BasicExamples/BPM.h"
#include "BasicExamples/BackgroundStars.h"
#include "BasicExamples/BikeParked.h"
#include "BasicExamples/BikeRolling.h"
#include "BasicExamples/Blink.h"
#include "BasicExamples/BouncingBalls.h"
#include "BasicExamples/Breathing.h"
#include "BasicExamples/Candle.h"
#include "BasicExamples/Chase.h"
#include "BasicExamples/Colorloop.h"
#include "BasicExamples/Colorwaves.h"
#include "BasicExamples/Dissolve.h"
#include "BasicExamples/DoubleRainbow.h"
#include "BasicExamples/Drip.h"
#include "BasicExamples/Fade.h"
#include "BasicExamples/Fairy.h"
#include "BasicExamples/Fire.h"
#include "BasicExamples/FireFlicker.h"
#include "BasicExamples/Fireworks.h"
#include "BasicExamples/Flow.h"
#include "BasicExamples/Glitter.h"
#include "BasicExamples/Gradient.h"
#include "BasicExamples/HalloweenEyes.h"
#include "BasicExamples/Heartbeat.h"
#include "BasicExamples/ICU.h"
#include "BasicExamples/Juggle.h"
#include "BasicExamples/Lightning.h"
#include "BasicExamples/Loading.h"
#include "BasicExamples/Matrix.h"
#include "BasicExamples/MovingComets.h"
#include "BasicExamples/Oscillate.h"
#include "BasicExamples/Pacifica.h"
#include "BasicExamples/Percent.h"
#include "BasicExamples/Phased.h"
#include "BasicExamples/Plasma.h"
#include "BasicExamples/Popcorn.h"
#include "BasicExamples/Pride.h"
#include "BasicExamples/PulsingCenter.h"
#include "BasicExamples/Railway.h"
#include "BasicExamples/Rainbow.h"
#include "BasicExamples/RandomColors.h"
#include "BasicExamples/Ripple.h"
#include "BasicExamples/Running.h"
#include "BasicExamples/Saw.h"
#include "BasicExamples/Scan.h"
#include "BasicExamples/Shimmer.h"
#include "BasicExamples/SineWave.h"
#include "BasicExamples/Sinelon.h"
#include "BasicExamples/Sparkle.h"
#include "BasicExamples/SplitRainbow.h"
#include "BasicExamples/Spots.h"
#include "BasicExamples/Strobe.h"
#include "BasicExamples/Sunrise.h"
#include "BasicExamples/Sweep.h"
#include "BasicExamples/TVSimulator.h"
#include "BasicExamples/Theater.h"
#include "BasicExamples/TrafficLight.h"
#include "BasicExamples/TriChase.h"
#include "BasicExamples/Twinkle.h"
#include "BasicExamples/TwoDots.h"
#include "BasicExamples/WashingMachine.h"
#include "BasicExamples/Wipe.h"

struct BuiltinProgram {
    const char* name;
    const char* source;
};

static const BuiltinProgram kBuiltins[] = {
    {"Aurora", Aurora::program},
    {"BPM", BPM::program},
    {"BackgroundStars", BackgroundStars::program},
    {"BikeParked", BikeParked::program},
    {"BikeRolling", BikeRolling::program},
    {"Blink", Blink::program},
    {"BouncingBalls", BouncingBalls::program},
    {"Breathing", Breathing::program},
    {"Candle", Candle::program},
    {"Chase", Chase::program},
    {"Colorloop", Colorloop::program},
    {"Colorwaves", Colorwaves::program},
    {"Dissolve", Dissolve::program},
    {"DoubleRainbow", DoubleRainbow::program},
    {"Drip", Drip::program},
    {"Fade", Fade::program},
    {"Fairy", Fairy::program},
    {"Fire", Fire::program},
    {"FireFlicker", FireFlicker::program},
    {"Fireworks", Fireworks::program},
    {"Flow", Flow::program},
    {"Glitter", Glitter::program},
    {"Gradient", Gradient::program},
    {"HalloweenEyes", HalloweenEyes::program},
    {"Heartbeat", Heartbeat::program},
    {"ICU", ICU::program},
    {"Juggle", Juggle::program},
    {"Lightning", Lightning::program},
    {"Loading", Loading::program},
    {"Matrix", Matrix::program},
    {"MovingComets", MovingComets::program},
    {"Oscillate", Oscillate::program},
    {"Pacifica", Pacifica::program},
    {"Percent", Percent::program},
    {"Phased", Phased::program},
    {"Plasma", Plasma::program},
    {"Popcorn", Popcorn::program},
    {"Pride", Pride::program},
    {"PulsingCenter", PulsingCenter::program},
    {"Railway", Railway::program},
    {"Rainbow", Rainbow::program},
    {"RandomColors", RandomColors::program},
    {"Ripple", Ripple::program},
    {"Running", Running::program},
    {"Saw", Saw::program},
    {"Scan", Scan::program},
    {"Shimmer", Shimmer::program},
    {"SineWave", SineWave::program},
    {"Sinelon", Sinelon::program},
    {"Sparkle", Sparkle::program},
    {"SplitRainbow", SplitRainbow::program},
    {"Spots", Spots::program},
    {"Strobe", Strobe::program},
    {"Sunrise", Sunrise::program},
    {"Sweep", Sweep::program},
    {"TVSimulator", TVSimulator::program},
    {"Theater", Theater::program},
    {"TrafficLight", TrafficLight::program},
    {"TriChase", TriChase::program},
    {"Twinkle", Twinkle::program},
    {"TwoDots", TwoDots::program},
    {"WashingMachine", WashingMachine::program},
    {"Wipe", Wipe::program},
};

static const int kBuiltinCount = (int)(sizeof(kBuiltins) / sizeof(kBuiltins[0]));

static bool nameStart(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool nameCont(char c) {
    return nameStart(c) || (c >= '0' && c <= '9') || c == '_';
}

bool validProgramName(const String& name) {
    if (name.length() < 1 || name.length() > 23) return false;
    if (!nameStart(name[0])) return false;
    for (unsigned i = 1; i < name.length(); i++) {
        if (!nameCont(name[i])) return false;
    }
    return true;
}

static const BuiltinProgram* findBuiltin(const String& name) {
    for (int i = 0; i < kBuiltinCount; i++) {
        if (name == kBuiltins[i].name) return &kBuiltins[i];
    }
    return nullptr;
}

bool isBuiltin(const String& name) {
    return findBuiltin(name) != nullptr;
}

static String programPath(const String& name) {
    return "/programs/" + name + ".bas";
}

static String fileBaseName(const String& path) {
    int slash = path.lastIndexOf('/');
    String name = slash >= 0 ? path.substring(slash + 1) : path;
    if (name.endsWith(".bas")) name = name.substring(0, name.length() - 4);
    return name;
}

bool catalogBegin() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
        return false;
    }
    if (!LittleFS.exists("/programs")) {
        LittleFS.mkdir("/programs");
    }
    Serial.println("LittleFS ready");
    return true;
}

static int countUserPrograms() {
    int count = 0;
    File dir = LittleFS.open("/programs");
    if (!dir) return 0;
    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = fileBaseName(file.name());
            if (validProgramName(name) && !isBuiltin(name)) count++;
        }
        file = dir.openNextFile();
    }
    return count;
}

bool lookupSource(const String& name, String& out, bool& builtin) {
    const BuiltinProgram* found = findBuiltin(name);
    if (found) {
        out = found->source;
        builtin = true;
        return true;
    }
    if (!validProgramName(name)) return false;
    File file = LittleFS.open(programPath(name), "r");
    if (!file) return false;
    out = file.readString();
    file.close();
    builtin = false;
    return true;
}

bool saveUserProgram(const String& name, const String& source, String& error) {
    if (!validProgramName(name)) {
        error = "invalid name";
        return false;
    }
    if (isBuiltin(name)) {
        error = "built-in program cannot be replaced";
        return false;
    }
    if (source.length() == 0) {
        error = "empty program";
        return false;
    }
    if (source.length() > kMaxProgramBytes) {
        error = "program too large";
        return false;
    }
    String path = programPath(name);
    bool replacing = LittleFS.exists(path);
    if (!replacing && countUserPrograms() >= kMaxUserPrograms) {
        error = "program limit reached";
        return false;
    }
    File file = LittleFS.open(path, "w");
    if (!file) {
        error = "storage failed";
        return false;
    }
    size_t wrote = file.print(source);
    file.close();
    if (wrote != source.length()) {
        LittleFS.remove(path);
        error = "storage failed";
        return false;
    }
    return true;
}

bool deleteUserProgram(const String& name, String& error) {
    if (!validProgramName(name)) {
        error = "invalid name";
        return false;
    }
    if (isBuiltin(name)) {
        error = "built-in program cannot be deleted";
        return false;
    }
    String path = programPath(name);
    if (!LittleFS.exists(path)) {
        error = "program not found";
        return false;
    }
    if (!LittleFS.remove(path)) {
        error = "storage failed";
        return false;
    }
    return true;
}

void listPrograms(std::vector<ProgramInfo>& out) {
    out.clear();
    for (int i = 0; i < kBuiltinCount; i++) {
        ProgramInfo info;
        info.name = kBuiltins[i].name;
        info.builtin = true;
        info.bytes = strlen(kBuiltins[i].source);
        out.push_back(info);
    }
    File dir = LittleFS.open("/programs");
    if (!dir) return;
    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = fileBaseName(file.name());
            if (validProgramName(name) && !isBuiltin(name)) {
                ProgramInfo info;
                info.name = name;
                info.builtin = false;
                info.bytes = file.size();
                out.push_back(info);
            }
        }
        file = dir.openNextFile();
    }
}
