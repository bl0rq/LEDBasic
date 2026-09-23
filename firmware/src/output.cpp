#include "output.h"

#include <FastLED.h>

#include "BasicInterpreter.h"
#include "net.h"

static CRGB* gLeds = nullptr;
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
    gLeds = new CRGB[length];
    fill_solid(gLeds, length, CRGB::Black);
    addStrip(ledType, colorOrder, pin, gLeds, length);
    FastLED.setBrightness((uint8_t)brightness);
    FastLED.clear(true);
    gController = new BasicLEDController(gLeds, length);
    gController->setOwnsPhysicalOutput(true);
    gController->setAutoShow(true);
    gController->setClock(netMillis, netDelay, nullptr);
    gFpsMark = millis();
    return gController != nullptr;
}

BasicLEDController* outputController() {
    return gController;
}

void outputSetBrightness(int brightness) {
    FastLED.setBrightness((uint8_t)brightness);
}

int outputBrightness() {
    return FastLED.getBrightness();
}

int outputFps() {
    return gFps;
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
