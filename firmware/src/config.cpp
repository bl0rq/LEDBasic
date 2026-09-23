#include "config.h"

#include <Preferences.h>

static const char* kNamespace = "ledbasic";

bool validLedType(const String& type) {
    for (int i = 0; i < kLedTypeCount; i++) {
        if (type == kLedTypes[i]) return true;
    }
    return false;
}

bool validColorOrder(const String& order) {
    for (int i = 0; i < kColorOrderCount; i++) {
        if (order == kColorOrders[i]) return true;
    }
    return false;
}

bool validDataPin(int pin) {
    for (int i = 0; i < kDataPinCount; i++) {
        if (pin == kDataPins[i]) return true;
    }
    return false;
}

bool validLength(int length) {
    return length >= 1 && length <= kMaxLeds;
}

bool validBrightness(int brightness) {
    return brightness >= 0 && brightness <= 255;
}

void sanitizeConfig(DeviceConfig& cfg) {
    if (!validLedType(cfg.ledType)) cfg.ledType = kDefaultType;
    if (!validColorOrder(cfg.colorOrder)) cfg.colorOrder = kDefaultOrder;
    if (!validLength(cfg.length)) cfg.length = kDefaultLength;
    if (!validDataPin(cfg.pin)) cfg.pin = kDefaultPin;
    if (!validBrightness(cfg.brightness)) cfg.brightness = kDefaultBrightness;
    if (cfg.activeProgram.length() == 0) cfg.activeProgram = kDefaultProgram;
}

bool loadConfig(DeviceConfig& cfg) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) {
        cfg.ledType = kDefaultType;
        cfg.colorOrder = kDefaultOrder;
        cfg.length = kDefaultLength;
        cfg.pin = kDefaultPin;
        cfg.brightness = kDefaultBrightness;
        cfg.activeProgram = kDefaultProgram;
        return false;
    }
    cfg.wifiSsid = prefs.getString("ssid", "");
    cfg.wifiPassword = prefs.getString("pass", "");
    cfg.ledType = prefs.getString("type", kDefaultType);
    cfg.colorOrder = prefs.getString("order", kDefaultOrder);
    cfg.length = prefs.getInt("len", kDefaultLength);
    cfg.pin = prefs.getInt("pin", kDefaultPin);
    cfg.brightness = prefs.getInt("bri", kDefaultBrightness);
    cfg.activeProgram = prefs.getString("prog", kDefaultProgram);
    prefs.end();
    sanitizeConfig(cfg);
    return true;
}

void saveConfig(const DeviceConfig& cfg) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        Serial.println("NVS open failed");
        return;
    }
    prefs.putString("ssid", cfg.wifiSsid);
    prefs.putString("pass", cfg.wifiPassword);
    prefs.putString("type", cfg.ledType);
    prefs.putString("order", cfg.colorOrder);
    prefs.putInt("len", cfg.length);
    prefs.putInt("pin", cfg.pin);
    prefs.putInt("bri", cfg.brightness);
    prefs.putString("prog", cfg.activeProgram);
    prefs.end();
}

void clearWifi(DeviceConfig& cfg) {
    cfg.wifiSsid = "";
    cfg.wifiPassword = "";
}
