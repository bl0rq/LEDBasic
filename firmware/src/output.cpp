#include "output.h"

#include <FastLED.h>

#include "BasicInterpreter.h"
#include "config.h"
#include "net.h"

static void setRelay(int brightness) {
    pinMode(kRelayPin, OUTPUT);
    digitalWrite(kRelayPin, brightness > 0 ? HIGH : LOW);
}

static CRGB* gLeds = nullptr;
static int gLedCount = 0;
static int gMaster = 255;
static BasicLEDController* gController = nullptr;
static fl::ChannelPtr gChannel;
static String gGoodSource;
static int gFps = 0;
static uint32_t gFrames = 0;
static uint32_t gFpsMark = 0;

static EOrder orderFromName(const String& name) {
    if (name == "RGB") return RGB;
    if (name == "RBG") return RBG;
    if (name == "GBR") return GBR;
    if (name == "BRG") return BRG;
    if (name == "BGR") return BGR;
    return GRB;
}

// The channel pointer has to stay alive. FastLED drops the strip when it is released.
static void addStrip(const String& ledType, const String& colorOrder, int pin, CRGB* leds, int count) {
    EOrder order = orderFromName(colorOrder);
    fl::span<CRGB> pixels(leds, count);
    fl::ChannelConfig config = ledType == "ws2811"
        ? fl::ChannelConfig(fl::makeClockless<fl::TIMING_WS2811_400KHZ>(pin), pixels, order)
        : fl::ChannelConfig(fl::makeClockless<fl::TIMING_WS2812_800KHZ>(pin), pixels, order);
    gChannel = FastLED.add(config);
    if (!gChannel) {
        Serial.println("LED channel was not created");
    }
}

bool outputBegin(const String& ledType, const String& colorOrder, int length, int pin, int brightness) {
    gLedCount = length;
    gLeds = new CRGB[length];
    fill_solid(gLeds, length, CRGB::Black);
    addStrip(ledType, colorOrder, pin, gLeds, length);
    gController = new BasicLEDController(gLeds, length);
    gController->setOwnsPhysicalOutput(true);
    gController->setAutoShow(true);
    gController->setClock(netMillis, netDelay, nullptr);
    outputSetBrightness(brightness);
    FastLED.clear(true);
    Serial.printf("Relay GPIO%d %s\n", kRelayPin, brightness > 0 ? "on" : "off");
    gFpsMark = millis();
    return gController != nullptr;
}

BasicLEDController* outputController() {
    return gController;
}

void outputSetBrightness(int brightness) {
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    gMaster = brightness;
    setRelay(gMaster);
    if (gController) gController->setMasterBrightness((uint8_t)gMaster);
}

int outputBrightness() {
    return FastLED.getBrightness();
}

int outputMaster() {
    return gMaster;
}

int outputFps() {
    return gFps;
}

int outputPreview(uint8_t* rgb, int maxSamples) {
    if (!gLeds || gLedCount <= 0 || maxSamples <= 0) return 0;
    int n = gLedCount < maxSamples ? gLedCount : maxSamples;
    int bright = FastLED.getBrightness();
    for (int i = 0; i < n; i++) {
        int src = n == 1 ? 0 : (i * (gLedCount - 1)) / (n - 1);
        rgb[i * 3] = (uint8_t)((gLeds[src].r * bright) / 255);
        rgb[i * 3 + 1] = (uint8_t)((gLeds[src].g * bright) / 255);
        rgb[i * 3 + 2] = (uint8_t)((gLeds[src].b * bright) / 255);
    }
    return n;
}

void outputNoteFrame() {
    gFrames++;
    uint32_t now = millis();
    if (now - gFpsMark >= 1000) {
        gFps = (int)gFrames;
        gFrames = 0;
        gFpsMark = now;
    }
}

bool outputLoad(const String& source, std::vector<OutputDiag>& diags) {
    diags.clear();
    if (!gController) return false;
    netSetBusy(true);
    gController->interrupt();
    if (!gController->loadProgram(source)) {
        std::vector<Diagnostic> found = gController->getDiagnostics();
        for (size_t i = 0; i < found.size(); i++) {
            OutputDiag diag;
            diag.line = found[i].line;
            diag.column = found[i].column;
            diag.message = found[i].message;
            diags.push_back(diag);
        }
        if (gGoodSource.length() > 0 && gController->loadProgram(gGoodSource)) {
            gController->runSetup();
        }
        netSetBusy(false);
        return false;
    }
    gGoodSource = source;
    gController->runSetup();
    netSetBusy(false);
    return true;
}
