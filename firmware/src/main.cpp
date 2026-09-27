#include <Arduino.h>

#include "BasicInterpreter.h"
#include "catalog.h"
#include "config.h"
#include "net.h"
#include "output.h"

static DeviceConfig gConfig;

void setup() {
    Serial.begin(115200);
    delay(50);
    Serial.println();
    Serial.println("LEDBasic");

    loadConfig(gConfig);
    if (!catalogBegin()) {
        Serial.println("Program storage is unavailable");
    }

    String source;
    bool builtin = false;
    if (!lookupSource(gConfig.activeProgram, source, builtin)) {
        Serial.printf("Program %s missing, using %s\n", gConfig.activeProgram.c_str(), kDefaultProgram);
        gConfig.activeProgram = kDefaultProgram;
        lookupSource(gConfig.activeProgram, source, builtin);
        if (!saveConfig(gConfig)) {
            Serial.println("Failed to persist fallback program selection");
        }
    }

    Serial.printf("LED %s %s pin %d x %d brightness %d program %s\n",
                  gConfig.ledType.c_str(), gConfig.colorOrder.c_str(), gConfig.pin,
                  gConfig.length, gConfig.brightness, gConfig.activeProgram.c_str());

    int pixels = gConfig.measuring ? kMaxLeds : gConfig.length;
    if (!outputBegin(gConfig.ledType, gConfig.colorOrder, pixels, gConfig.pin, gConfig.master)) {
        Serial.println("LED output unavailable; continuing without a physical strip");
    }
    outputSetPalette(gConfig.palette.c_str());
    netBegin(gConfig);

    ApplyResult boot = netApply(gConfig.activeProgram, source, true);
    if (!boot.ok) {
        Serial.printf("Boot program %s failed to load; falling back to %s\n",
                      gConfig.activeProgram.c_str(), kDefaultProgram);
        if (gConfig.activeProgram != String(kDefaultProgram)) {
            String defaultSource;
            bool defaultBuiltin = false;
            if (lookupSource(kDefaultProgram, defaultSource, defaultBuiltin)) {
                // netApply() updates gConfig.activeProgram and persists it
                // via saveConfig() since it differs from the failed program.
                boot = netApply(kDefaultProgram, defaultSource, true);
            }
            if (!boot.ok) {
                Serial.println("Default program also failed to load");
            }
        }
    }
}

void loop() {
    netLoop();

    String name;
    String source;
    bool remember = false;
    if (netTakePending(name, source, remember)) {
        netApply(name, source, remember);
    }

    netSetBusy(true);
    if (netMeasuring()) {
        outputPaintMeasure(netMeasureEnd());
    } else if (outputController()) {
        outputController()->runLoop(millis());
    }
    outputNoteFrame();
    netSetBusy(false);
}
