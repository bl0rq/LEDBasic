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
    if (!validBrightness(cfg.master)) cfg.master = kDefaultMaster;
    if (findPalette(cfg.palette.c_str()) < 0) cfg.palette = kDefaultPalette;
    if (cfg.activeProgram.length() == 0) cfg.activeProgram = kDefaultProgram;
    if (!validLength(cfg.measureWas)) cfg.measureWas = cfg.length;
    if (cfg.measureEnd < 4) cfg.measureEnd = 4;
    if (cfg.measureEnd > kMaxLeds) cfg.measureEnd = kMaxLeds;
}

bool loadConfig(DeviceConfig& cfg) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) {
        cfg.ledType = kDefaultType;
        cfg.colorOrder = kDefaultOrder;
        cfg.length = kDefaultLength;
        cfg.pin = kDefaultPin;
        cfg.brightness = kDefaultBrightness;
        cfg.master = kDefaultMaster;
        cfg.palette = kDefaultPalette;
        cfg.activeProgram = kDefaultProgram;
        cfg.measuring = false;
        cfg.measureWas = cfg.length;
        cfg.measureEnd = cfg.length;
        return false;
    }
    cfg.wifiSsid = prefs.getString("ssid", "");
    cfg.wifiPassword = prefs.getString("pass", "");
    cfg.ledType = prefs.getString("type", kDefaultType);
    cfg.colorOrder = prefs.getString("order", kDefaultOrder);
    cfg.length = prefs.getInt("len", kDefaultLength);
    cfg.pin = prefs.getInt("pin", kDefaultPin);
    cfg.brightness = prefs.getInt("bri", kDefaultBrightness);
    cfg.master = prefs.getInt("master", kDefaultMaster);
    cfg.palette = prefs.getString("pal", kDefaultPalette);
    cfg.activeProgram = prefs.getString("prog", kDefaultProgram);
    cfg.measuring = prefs.getInt("meas", 0) != 0;
    cfg.measureWas = prefs.getInt("mwas", cfg.length);
    cfg.measureEnd = prefs.getInt("mend", cfg.length);
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
    prefs.putInt("master", cfg.master);
    prefs.putString("pal", cfg.palette);
    prefs.putString("prog", cfg.activeProgram);
    prefs.putInt("meas", cfg.measuring ? 1 : 0);
    prefs.putInt("mwas", cfg.measureWas);
    prefs.putInt("mend", cfg.measureEnd);
    prefs.end();
}

void clearWifi(DeviceConfig& cfg) {
    cfg.wifiSsid = "";
    cfg.wifiPassword = "";
}
