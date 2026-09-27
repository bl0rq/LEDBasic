#pragma once

#include <Arduino.h>
#include <vector>

#include "output.h"

struct DeviceConfig;

struct ApplyResult {
    bool ok;
    bool pending;
    String error;
    std::vector<OutputDiag> diagnostics;
};

void netBegin(DeviceConfig& cfg);
void netLoop();
void netSetBusy(bool busy);
bool netBusy();
unsigned long netMillis(void* user);
void netDelay(int ms, void* user);

ApplyResult netApply(const String& name, const String& source, bool remember);
void netEnqueue(const String& name, const String& source, bool remember);
bool netTakePending(String& name, String& source, bool& remember);

bool netMeasuring();
int netMeasureEnd();

const String& netRunningName();
bool netRunningBuiltin();
bool netRunningUnsaved();
const std::vector<OutputDiag>& netLoadFailure();
