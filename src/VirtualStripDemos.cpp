#include "VirtualStripDemos.h"
#include "BasicInterpreter.h"
#include "BasicExamples/BackgroundStars.h"
#include "BasicExamples/MovingComets.h"
#include "BasicExamples/PulsingCenter.h"
#include "BasicExamples/Rainbow.h"
#include "BasicExamples/Matrix.h"
#include "BasicExamples/SineWave.h"
#include "BasicExamples/Breathing.h"
#include "BasicExamples/DoubleRainbow.h"

void removeStripsForIndex(int stripIndex, VirtualStripManager* stripManager) {
  for (int i = stripManager->getStripCount() - 1; i >= 0; --i) {
    VirtualStrip* vs = stripManager->getStrip(i);
    if (vs && vs->getStripIndex() == stripIndex) {
      stripManager->removeStrip(vs);
    }
  }
}

void setupVirtualStripDemo(int stripIndex, VirtualStripManager* stripManager, const int* numLeds, bool* basicActive) {
  Serial.print("Setting up virtual strip demo on strip ");
  Serial.println(stripIndex);

  removeStripsForIndex(stripIndex, stripManager);

  VirtualStrip* stars = stripManager->createStrip(stripIndex, 0, numLeds[stripIndex], 0, BLEND_ADD);
  if (stars) stars->loadProgram(String(BackgroundStars::program));

  VirtualStrip* comets = stripManager->createStrip(stripIndex, 0, numLeds[stripIndex]/2, 10, BLEND_ADD);
  if (comets) comets->loadProgram(String(MovingComets::program));

  VirtualStrip* pulse = stripManager->createStrip(stripIndex, numLeds[stripIndex]/4, numLeds[stripIndex]/2, 20, BLEND_ADD);
  if (pulse) pulse->loadProgram(String(PulsingCenter::program));

  stripManager->runAllSetups();
  if (basicActive) {
    basicActive[stripIndex] = false;
  }

  Serial.println("Virtual strip demo ready!");
  Serial.println("Background stars: Full strip, Z=0, Additive");
  Serial.println("Moving comets: First half, Z=10, Additive");
  Serial.println("Pulsing center: Center region, Z=20, Additive");
}

void setupOverlappingDemo(int stripIndex, VirtualStripManager* stripManager, const int* numLeds, bool* basicActive) {
  Serial.print("Setting up overlapping regions demo on strip ");
  Serial.println(stripIndex);

  removeStripsForIndex(stripIndex, stripManager);

  VirtualStrip* leftRainbow = stripManager->createStrip(stripIndex, 0, numLeds[stripIndex]*2/3, 0, BLEND_REPLACE);
  if (leftRainbow) leftRainbow->loadProgram(String(Rainbow::program));

  VirtualStrip* rightMatrix = stripManager->createStrip(stripIndex, numLeds[stripIndex]/3, numLeds[stripIndex]*2/3, 10, BLEND_ADD);
  if (rightMatrix) rightMatrix->loadProgram(String(Matrix::program));

  stripManager->runAllSetups();
  if (basicActive) {
    basicActive[stripIndex] = false;
  }

  Serial.println("Overlapping demo ready!");
  Serial.println("Left rainbow: 0 to 2/3, Z=0, Replace");
  Serial.println("Right matrix: 1/3 to end, Z=10, Additive");
}

void setupFourStripDemo(VirtualStripManager* stripManager, const int* numLeds, bool* basicActive, int numStrips) {
  Serial.println("Setting up four-strip segmented demo...");

  stripManager->removeAllStrips();
  for (int i = 0; i < numStrips; ++i) {
    basicActive[i] = false;
  }

  for (int s = 0; s < numStrips; ++s) {
    int segmentLen = numLeds[s] / 4;
    int start = 0;

    VirtualStrip* seg1 = stripManager->createStrip(s, start, segmentLen, 0, BLEND_REPLACE);
    if (seg1) seg1->loadProgram(String(Rainbow::program));
    start += segmentLen;

    VirtualStrip* seg2 = stripManager->createStrip(s, start, segmentLen, 0, BLEND_REPLACE);
    if (seg2) seg2->loadProgram(String(SineWave::program));
    start += segmentLen;

    VirtualStrip* seg3 = stripManager->createStrip(s, start, segmentLen, 0, BLEND_REPLACE);
    if (seg3) seg3->loadProgram(String(Breathing::program));
    start += segmentLen;

    VirtualStrip* seg4 = stripManager->createStrip(s, start, numLeds[s] - start, 0, BLEND_REPLACE);
    if (seg4) seg4->loadProgram(String(DoubleRainbow::program));
  }

  stripManager->runAllSetups();

  Serial.println("Four-strip segmented demo ready!");
  Serial.println("Segments: Rainbow | Sine Wave | Breathing | Double Rainbow");
}
