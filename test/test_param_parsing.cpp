// Simple test to verify parameter parsing
#include "include/BasicInterpreter.h"

const char* testProgram = R"(
param speed number(25.0, 5.0, 100.0, 1.0)
param brightness_level number(200.0, 10.0, 255.0, 5.0)
param color_scheme enum(["Red-Blue", "Green-Blue", "Rainbow"])

setup
  brightness(128)
  clear()
end

loop(time)
  // Simple test loop
  show()
end
)";

void testParameterParsing() {
    // Create mock LED array for testing
    CRGB testLeds[10];
    BasicLEDController controller(testLeds, 10);
    
    // Load the test program
    if (controller.loadProgram(String(testProgram))) {
        Serial.println("Program loaded successfully!");
        
        // Get all parameters
        std::vector<Parameter> params = controller.getAllParameters();
        Serial.printf("Found %d parameters:\n", params.size());
        
        for (const Parameter& param : params) {
            Serial.printf("- %s: type=%d, value=%s\n", 
                param.name.c_str(), 
                param.type, 
                param.getStringValue().c_str()
            );
        }
    } else {
        Serial.println("Failed to load program!");
    }
}
