#pragma once

#include <Arduino.h>
#include <vector>

class BasicLEDController;

struct OutputDiag {
    int line;
    int column;
    String message;
};

bool outputBegin(const String& ledType, const String& colorOrder, int length, int pin, int brightness);
BasicLEDController* outputController();
void outputSetBrightness(int brightness);
int outputBrightness();
int outputFps();
void outputNoteFrame();

// loadProgram drops the running program before the new source is parsed.
// On failure the previous source is loaded again and diags describes the rejection.
bool outputLoad(const String& source, std::vector<OutputDiag>& diags);
