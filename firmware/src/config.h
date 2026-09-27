#pragma once

#include <Arduino.h>

#include "Palettes.h"

// Factory defaults match the GLEDOPTO Elite 6D-EXMU data output.
// GPIO12 is a flash-voltage strap; it is safe as a data pin only after boot.
static const int kDataPins[] = {16, 14, 13, 12, 4, 2};
static const int kDataPinCount = 6;
static const char* kLedTypes[] = {"ws2812", "ws2811"};
static const int kLedTypeCount = 2;
static const char* kColorOrders[] = {"RGB", "RBG", "GRB", "GBR", "BRG", "BGR"};
static const int kColorOrderCount = 6;

static const int kDefaultLength = 60;
static const int kDefaultPin = 16;
static const int kDefaultBrightness = 128;
static const int kDefaultMaster = 255;
static const char* kDefaultType = "ws2812";
static const char* kDefaultOrder = "GRB";
static const char* kDefaultProgram = "Rainbow";
static const char* kDefaultPalette = "Rainbow";
static const int kMaxLeds = 1024;
static const int kButtonPin = 17;
// Elite 6D-EXMU energy-saving relay. WLED "Invert" means high connects input V+ to the outputs.
static const int kRelayPin = 18;
static const char* kApPassword = "ledbasic";
static const char* kHostname = "ledbasic";
static const char* kFirmwareVersion = "0.1.0";

struct DeviceConfig {
    String wifiSsid;
    String wifiPassword;
    String ledType;
    String colorOrder;
    int length;
    int pin;
    int brightness;
    int master;
    String palette;
    String activeProgram;
    bool measuring;
    int measureWas;
    int measureEnd;
};

bool validLedType(const String& type);
bool validColorOrder(const String& order);
bool validDataPin(int pin);
bool validLength(int length);
bool validBrightness(int brightness);
void sanitizeConfig(DeviceConfig& cfg);
bool loadConfig(DeviceConfig& cfg);
bool saveConfig(const DeviceConfig& cfg);
void clearWifi(DeviceConfig& cfg);
