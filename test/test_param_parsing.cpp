// Host tests live in test/native/. From the repo root:
//   pwsh test/native/run.ps1
//
// This file is kept so older sketches that called testParameterParsing()
// still compile if they include it.

#include "BasicInterpreter.h"

void testParameterParsing() {
    CRGB testLeds[10];
    BasicLEDController controller(testLeds, 10);
    const char* testProgram = R"(
param speed number(25.0, 5.0, 100.0, 1.0)
param brightness_level number(200.0, 10.0, 255.0, 5.0)
param color_scheme enum(["Red-Blue", "Green-Blue", "Rainbow"])

setup
  brightness(128)
  clear()
end

loop(time)
  show()
end
)";
    if (controller.loadProgram(String(testProgram))) {
        Serial.println("Program loaded successfully!");
        std::vector<Parameter> params = controller.getAllParameters();
        Serial.printf("Found %d parameters:\n", (int)params.size());
        for (const Parameter& param : params) {
            Serial.printf("- %s: type=%d, value=%s\n",
                param.name.c_str(),
                (int)param.type,
                param.getStringValue().c_str());
        }
    } else {
        Serial.println("Failed to load program!");
    }
}
