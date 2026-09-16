// VirtualStripDemo - Example sketch for the LEDBasic library
//
// Demonstrates:
//   - Setting up physical LED strips with FastLED
//   - Creating virtual strips with layered BASIC programs
//   - Using the serial CommandHandler with callbacks
//   - Loading and running BASIC LED animation programs
//
// Hardware: ESP32-S3 with WS2812B strips on pins 5 and 48

#include <Arduino.h>
#include <FastLED.h>
#include "LEDBasic.h"

// Include the example programs you want to use
#include "BasicExamples/Rainbow.h"
#include "BasicExamples/Breathing.h"
#include "BasicExamples/SineWave.h"
#include "BasicExamples/DoubleRainbow.h"
#include "BasicExamples/Matrix.h"
#include "BasicExamples/BackgroundStars.h"
#include "BasicExamples/MovingComets.h"
#include "BasicExamples/PulsingCenter.h"

// ----- Hardware Configuration -----
#define NUM_STRIPS 2
#define SEGMENT_LENGTH 43
#define STRIP_PIN_A 5
#define STRIP_PIN_B 48
#define BRIGHTNESS 64
#define FRAMES_PER_SECOND 120
#define DELAY_TIME (1000 / FRAMES_PER_SECOND)

CRGB leds[NUM_STRIPS][SEGMENT_LENGTH];
const int NUM_LEDS[NUM_STRIPS] = {SEGMENT_LENGTH, SEGMENT_LENGTH};

// BASIC LED Controller instances, one per strip
BasicLEDController* basicControllers[NUM_STRIPS];
bool basicActive[NUM_STRIPS];

// Virtual Strip Manager for layered animations
VirtualStripManager* stripManager;

unsigned long lastMillis = 0;

// ----- Application Functions -----

void displayParameters(int stripIndex) {
  std::vector<Parameter> params = basicControllers[stripIndex]->getAllParameters();
  if (params.empty()) {
    Serial.println("No parameters available for current program.");
    return;
  }
  Serial.printf("\n=== Discovered %d Parameters from Basic Program ===\n", params.size());
  for (const Parameter& param : params) {
    Serial.print("  ");
    Serial.print(param.name);
    Serial.print(": ");
    Serial.print(param.getStringValue());
    if (param.type == PARAM_NUMBER) {
      Serial.printf(" (range: %.1f - %.1f, step: %.1f)", param.minValue, param.maxValue, param.stepValue);
    } else if (param.type == PARAM_BOOLEAN) {
      Serial.print(" (boolean)");
    } else if (param.type == PARAM_ENUM) {
      Serial.print(" (options: ");
      for (int i = 0; i < param.enumValues.size(); i++) {
        if (i > 0) Serial.print(", ");
        Serial.print(param.enumValues[i]);
      }
      Serial.print(")");
    }
    Serial.println();
  }
  Serial.println("===================================================\n");
}

static void removeStripsForIndex(int stripIndex) {
  for (int i = stripManager->getStripCount() - 1; i >= 0; --i) {
    VirtualStrip* vs = stripManager->getStrip(i);
    if (vs && vs->getStripIndex() == stripIndex) {
      stripManager->removeStrip(vs);
    }
  }
}

void loadExampleProgram(int stripIndex, int programIndex) {
  const char* program = nullptr;
  bool useVirtualStrips = false;

  basicControllers[stripIndex]->clearParameters();

  switch (programIndex) {
    case 0: program = Rainbow::program; Serial.println("Loading rainbow..."); break;
    case 1: program = Breathing::program; Serial.println("Loading breathing..."); break;
    case 2: program = SineWave::program; Serial.println("Loading sine wave..."); break;
    case 3: program = DoubleRainbow::program; Serial.println("Loading double rainbow..."); break;
    case 4: program = Matrix::program; Serial.println("Loading matrix..."); break;
    case 5:
      useVirtualStrips = true;
      Serial.println("Loading virtual strip layered demo...");
      setupVirtualStripDemo(stripIndex, stripManager, NUM_LEDS, basicActive);
      break;
    case 6:
      useVirtualStrips = true;
      Serial.println("Loading overlapping regions demo...");
      setupOverlappingDemo(stripIndex, stripManager, NUM_LEDS, basicActive);
      break;
    case 7:
      useVirtualStrips = true;
      Serial.println("Loading four-strip segmented demo...");
      setupFourStripDemo(stripManager, NUM_LEDS, basicActive, NUM_STRIPS);
      break;
    default:
      program = Rainbow::program; Serial.println("Loading default rainbow..."); break;
  }

  if (!useVirtualStrips && program) {
    removeStripsForIndex(stripIndex);
    if (basicControllers[stripIndex]->loadProgram(String(program))) {
      Serial.println("Program loaded successfully!");
      basicControllers[stripIndex]->runSetup();
      displayParameters(stripIndex);
      basicActive[stripIndex] = true;
    } else {
      Serial.println("Failed to load program!");
      basicActive[stripIndex] = false;
    }
  } else if (useVirtualStrips) {
    basicActive[stripIndex] = false;
  }
}

void runFrame() {
  unsigned long now = millis();
  bool anyBasic = false;

  for (int i = 0; i < NUM_STRIPS; ++i) {
    if (basicActive[i] && basicControllers[i]) {
      basicControllers[i]->runLoop(now);
      anyBasic = true;
    }
  }

  bool anyVirtual = stripManager->getStripCount() > 0;
  if (anyVirtual) {
    stripManager->runAllLoops(now);
    stripManager->renderToPhysical();
  }

  if (anyVirtual || anyBasic) {
    FastLED.show();
  }
}

// ----- Arduino Setup & Loop -----

void setup() {
  Serial.begin(115200);
  Serial.println("LEDBasic Virtual Strip Demo Starting...");

  // Initialize FastLED strips
  FastLED.addLeds<WS2812B, STRIP_PIN_A, GRB>(leds[0], NUM_LEDS[0]);
  FastLED.addLeds<WS2812B, STRIP_PIN_B, GRB>(leds[1], NUM_LEDS[1]);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show();

  // Create BasicLEDControllers
  for (int i = 0; i < NUM_STRIPS; ++i) {
    basicControllers[i] = new BasicLEDController(leds[i], NUM_LEDS[i]);
    basicActive[i] = false;
  }

  // Initialize Virtual Strip Manager
  stripManager = new VirtualStripManager();
  for (int i = 0; i < NUM_STRIPS; ++i) {
    stripManager->addPhysicalStrip(leds[i], NUM_LEDS[i]);
  }

  // Register command handler callbacks
  initCommandHandler({
    .onLoadProgram = loadExampleProgram,
    .onDisplayParameters = displayParameters
  });

  // Load default programs
  for (int i = 0; i < NUM_STRIPS; ++i) {
    loadExampleProgram(i, i);
  }

  Serial.println("Setup complete! Commands:");
  Serial.println("  s:p  - Load program p on strip s (e.g., 0:1)");
  Serial.println("  Programs: 0=Rainbow, 1=Breathing, 2=Sine, 3=DoubleRainbow, 4=Matrix");
  Serial.println("            5=Layered, 6=Overlap, 7=Four-Strip");
  Serial.println("  s:p  - Show parameters (e.g., 0:p)");
  Serial.println("  s:param=value - Set parameter (e.g., 0:speed=30)");

  lastMillis = millis();
}

void loop() {
  processSerialInput(basicControllers, NUM_STRIPS);
  runFrame();

  unsigned long currentMillis = millis();
  unsigned long elapsed = currentMillis - lastMillis;
  if (elapsed <= DELAY_TIME) {
    FastLED.delay(DELAY_TIME - elapsed);
  }
  lastMillis = currentMillis;
}
