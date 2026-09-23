#pragma once

#include <Arduino.h>
#include <vector>

static const int kMaxUserPrograms = 24;
static const size_t kMaxProgramBytes = 8192;

struct ProgramInfo {
    String name;
    bool builtin;
    size_t bytes;
};

bool validProgramName(const String& name);
bool catalogBegin();
bool isBuiltin(const String& name);
bool lookupSource(const String& name, String& out, bool& builtin);
bool saveUserProgram(const String& name, const String& source, String& error);
bool deleteUserProgram(const String& name, String& error);
void listPrograms(std::vector<ProgramInfo>& out);
