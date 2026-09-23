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
        saveConfig(gConfig);
    }

    Serial.printf("LED %s %s pin %d x %d brightness %d program %s\n",
                  gConfig.ledType.c_str(), gConfig.colorOrder.c_str(), gConfig.pin,
                  gConfig.length, gConfig.brightness, gConfig.activeProgram.c_str());

    outputBegin(gConfig.ledType, gConfig.colorOrder, gConfig.length, gConfig.pin, gConfig.brightness);
    netBegin(gConfig);

    ApplyResult boot = netApply(gConfig.activeProgram, source, true);
    if (!boot.ok) {
        Serial.println("Boot program failed");
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
    if (outputController()) {
        outputController()->runLoop(millis());
    }
    outputNoteFrame();
    netSetBusy(false);
}
