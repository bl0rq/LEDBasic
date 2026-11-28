#include "net.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <cmath>
#include <cstdlib>

#include "BasicInterpreter.h"
#include "catalog.h"
#include "config.h"
#include "output.h"
#include "page.h"

static WebServer gServer(80);
static DNSServer gDns;
static DeviceConfig* gCfg = nullptr;

static bool gApOn = false;
static bool gMdns = false;
static String gApSsid;
static uint32_t gStaStarted = 0;
static uint32_t gRestartAt = 0;
static uint32_t gMasterSaveAt = 0;
static uint32_t gMeasureSaveAt = 0;

enum WifiPhase { PhaseAp, PhaseConnecting, PhaseSta, PhaseFallback };
static WifiPhase gPhase = PhaseAp;

static String gRunningName;
static bool gRunningBuiltin = false;
static bool gRunningUnsaved = false;
static uint32_t gLoadId = 0;
static std::vector<OutputDiag> gLoadFailure;

static bool gPending = false;
static bool gPendingRemember = false;
static String gPendingName;
static String gPendingSource;

static String gUpload;
static bool gUploadOverflow = false;

static bool gButtonDown = false;
static bool gButtonFired = false;
static uint32_t gButtonDownAt = 0;

unsigned long netMillis(void*) { return millis(); }

const String& netRunningName() { return gRunningName; }
bool netRunningBuiltin() { return gRunningBuiltin; }
bool netRunningUnsaved() { return gRunningUnsaved; }
const std::vector<OutputDiag>& netLoadFailure() { return gLoadFailure; }

void netEnqueue(const String& name, const String& source, bool remember) {
    gPending = true;
    gPendingRemember = remember;
    gPendingName = name;
    gPendingSource = source;
}

bool netTakePending(String& name, String& source, bool& remember) {
    if (!gPending) return false;
    gPending = false;
    name = gPendingName;
    source = gPendingSource;
    remember = gPendingRemember;
    gPendingName = "";
    gPendingSource = "";
    return true;
}

static uint32_t scheduleAfter(uint32_t ms) {
    uint32_t t = millis() + ms;
    return t == 0 ? 1 : t;
}

static bool parseStrictInt(const String& s, int& out) {
    const char* start = s.c_str();
    while (*start == ' ' || *start == '\t') start++;
    if (*start == '\0') return false;
    char* end = nullptr;
    long v = strtol(start, &end, 10);
    if (end == start) return false;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return false;
    if (v < -2147483647L - 1L || v > 2147483647L) return false;
    out = (int)v;
    return true;
}

static bool parseStrictFloat(const String& s, float& out) {
    const char* start = s.c_str();
    while (*start == ' ' || *start == '\t') start++;
    if (*start == '\0') return false;
    char* end = nullptr;
    float v = strtof(start, &end);
    if (end == start) return false;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0' || !std::isfinite(v)) return false;
    out = v;
    return true;
}

static bool jsonStrictInt(JsonVariant v, int& out) {
    if (v.isNull() || v.is<bool>()) return false;
    if (v.is<long>() || v.is<int>()) {
        long val = v.as<long>();
        if (val < -2147483647L - 1L || val > 2147483647L) return false;
        out = (int)val;
        return true;
    }
    return false;
}

static bool jsonStrictFloat(JsonVariant v, float& out) {
    if (v.isNull() || v.is<bool>()) return false;
    if (v.is<double>()) {
        float val = v.as<float>();
        if (!std::isfinite(val)) return false;
        out = val;
        return true;
    }
    return false;
}

static int storageErrorCode(const String& err) {
    if (err == "storage full") return 507;
    if (err == "storage failed") return 500;
    return 400;
}

static void requestRestart() {
    if (gRestartAt == 0) gRestartAt = scheduleAfter(400);
}

static void pollButton() {
    bool down = digitalRead(kButtonPin) == LOW;
    if (!down) {
        gButtonDown = false;
        gButtonFired = false;
        return;
    }
    if (!gButtonDown) {
        gButtonDown = true;
        gButtonDownAt = millis();
        return;
    }
    if (!gButtonFired && millis() - gButtonDownAt >= 5000) {
        gButtonFired = true;
        Serial.println("Button held, clearing Wi-Fi");
        clearWifi(*gCfg);
        if (!saveConfig(*gCfg)) {
            Serial.println("Failed to persist cleared Wi-Fi credentials");
        }
        ESP.restart();
    }
}

void netDelay(int ms, void*) {
    uint32_t start = millis();
    while ((int32_t)(millis() - start) < ms) {
        if (gPending) {
            if (outputController()) outputController()->interrupt();
            return;
        }
        netLoop();
        if (gPending) {
            if (outputController()) outputController()->interrupt();
            return;
        }
        delay(1);
    }
}

static void startMdns() {
    if (gMdns) return;
    if (!MDNS.begin(kHostname)) {
        Serial.println("mDNS start failed");
        return;
    }
    MDNS.addService("http", "tcp", 80);
    gMdns = true;
    Serial.printf("http://%s.local\n", kHostname);
}

static void startAp(bool keepSta) {
    uint8_t mac[6];
    char ssid[32];
    WiFi.mode(keepSta ? WIFI_AP_STA : WIFI_AP);
    WiFi.macAddress(mac);
    snprintf(ssid, sizeof(ssid), "LEDBasic-%02X%02X", mac[4], mac[5]);
    gApSsid = ssid;
    IPAddress ip(192, 168, 4, 1);
    WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(gApSsid.c_str(), kApPassword)) {
        Serial.println("Access point start failed");
        return;
    }
    gDns.start(53, "*", ip);
    gApOn = true;
    Serial.printf("AP %s password %s http://192.168.4.1\n", gApSsid.c_str(), kApPassword);
}

static String currentIp() {
    if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
    if (gApOn) return WiFi.softAPIP().toString();
    return "";
}

static const char* modeName() {
    return WiFi.status() == WL_CONNECTED ? "sta" : "ap";
}

static String currentSsid() {
    if (WiFi.status() == WL_CONNECTED) return WiFi.SSID();
    return gApSsid;
}

static String htmlEscape(const String& text) {
    String out;
    out.reserve(text.length());
    for (unsigned i = 0; i < text.length(); i++) {
        char c = text[i];
        if (c == '&') out += F("&amp;");
        else if (c == '<') out += F("&lt;");
        else if (c == '>') out += F("&gt;");
        else if (c == '"') out += F("&quot;");
        else out += c;
    }
    return out;
}

static bool asJson() {
    if (gServer.method() == HTTP_PUT || gServer.method() == HTTP_DELETE) return true;
    return gServer.arg("plain").length() > 0;
}

static bool clientWantsJson() {
    if (asJson()) return true;
    return gServer.header("Accept").indexOf("application/json") >= 0;
}

static void sendHome() {
    gServer.sendHeader("Cache-Control", "no-store");
    gServer.send_P(200, PSTR("text/html"), kPage);
}

static void sendJson(int code, const String& body) {
    gServer.send(code, "application/json", body);
}

static void sendPage(int code, const String& error) {
    if (error.length() == 0) {
        sendHome();
        return;
    }
    String html;
    html.reserve(500 + error.length());
    html += FPSTR(kNoticePrefix);
    html += htmlEscape(error);
    html += FPSTR(kNoticeSuffix);
    gServer.sendHeader("Cache-Control", "no-store");
    gServer.send(code, "text/html", html);
}

static void sendOk() {
    if (clientWantsJson()) {
        sendJson(200, "{\"ok\":true}");
        return;
    }
    gServer.sendHeader("Location", "/", true);
    gServer.send(303, "text/plain", "");
}

static void sendPending() {
    if (clientWantsJson()) {
        sendJson(202, "{\"ok\":true,\"pending\":true}");
        return;
    }
    gServer.sendHeader("Location", "/", true);
    gServer.send(303, "text/plain", "");
}

static void sendError(int code, const char* message, const std::vector<OutputDiag>* diags) {
    if (!clientWantsJson() && gServer.method() == HTTP_POST) {
        sendPage(code, message);
        return;
    }
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = message;
    if (diags) {
        JsonArray arr = doc["diagnostics"].to<JsonArray>();
        for (size_t i = 0; i < diags->size(); i++) {
            JsonObject item = arr.add<JsonObject>();
            item["line"] = (*diags)[i].line;
            item["column"] = (*diags)[i].column;
            item["message"] = (*diags)[i].message;
        }
    }
    String body;
    serializeJson(doc, body);
    sendJson(code, body);
}

static void sendRestarting() {
    if (clientWantsJson()) {
        sendJson(200, "{\"ok\":true,\"restart\":true}");
    } else {
        gServer.sendHeader("Cache-Control", "no-store");
        gServer.send_P(200, PSTR("text/html"), kRestartPage);
    }
    requestRestart();
}

static void sendApply(const ApplyResult& result) {
    if (!result.ok) {
        const char* message = result.error.length() ? result.error.c_str() : "program rejected";
        int code = storageErrorCode(result.error);
        sendError(code, message, result.diagnostics.empty() ? nullptr : &result.diagnostics);
        return;
    }
    if (result.pending) {
        sendPending();
        return;
    }
    sendOk();
}

ApplyResult netApply(const String& name, const String& source, bool remember) {
    ApplyResult result;
    result.ok = false;
    result.pending = false;
    if (!outputLoad(source, result.diagnostics)) {
        result.error = "program rejected";
        gLoadFailure = result.diagnostics;
        Serial.printf("Rejected program (%u diagnostics)\n", (unsigned)result.diagnostics.size());
        return result;
    }
    gLoadFailure.clear();
    gLoadId++;
    if (remember) {
        gRunningName = name;
        gRunningBuiltin = isBuiltin(name);
        gRunningUnsaved = false;
        if (gCfg->activeProgram != name) {
            gCfg->activeProgram = name;
            if (!saveConfig(*gCfg)) {
                Serial.println("Failed to persist active program selection");
            }
        }
        Serial.printf("Running %s\n", name.c_str());
    } else {
        gRunningName = "";
        gRunningBuiltin = false;
        gRunningUnsaved = true;
        Serial.println("Running unsaved program");
    }
    result.ok = true;
    return result;
}

// HTTP handlers never load a program themselves. Loading runs the program's
// setup, and a delay() there calls netLoop(), which would re-enter
// WebServer::handleClient() from inside the running handler: after
// HTTP_MAX_DATA_WAIT the nested call drops this handler's client and the
// response goes to the wrong socket. Instead, syntax-check here so a rejected
// program still gets an immediate 400 with diagnostics, and let loop() load
// it. A program that parses always loads, so 202 "pending" means it will run.
static ApplyResult checkProgram(const String& source) {
    ApplyResult result;
    result.ok = outputCheck(source, result.diagnostics);
    result.pending = result.ok;
    if (!result.ok) result.error = "program rejected";
    return result;
}

static ApplyResult queueApply(const String& name, const String& source, bool remember) {
    ApplyResult result = checkProgram(source);
    if (result.ok) netEnqueue(name, source, remember);
    return result;
}

static void addRuntimeError(JsonObject doc) {
    BasicLEDController* controller = outputController();
    if (controller && controller->hasRuntimeError()) {
        std::vector<Diagnostic> diags = controller->getDiagnostics();
        for (int i = (int)diags.size() - 1; i >= 0; i--) {
            if (diags[i].kind == Diagnostic::Runtime) {
                JsonObject err = doc["error"].to<JsonObject>();
                err["line"] = diags[i].line;
                err["column"] = diags[i].column;
                err["message"] = diags[i].message;
                return;
            }
        }
    }
    if (!gLoadFailure.empty()) {
        JsonObject err = doc["error"].to<JsonObject>();
        err["line"] = gLoadFailure[0].line;
        err["column"] = gLoadFailure[0].column;
        err["message"] = gLoadFailure[0].message;
        return;
    }
    doc["error"] = nullptr;
}

static void handleStatus() {
    JsonDocument doc;
    doc["name"] = kHostname;
    doc["version"] = kFirmwareVersion;
    doc["mode"] = modeName();
    doc["ip"] = currentIp();
    doc["hostname"] = String(kHostname) + ".local";
    doc["ssid"] = currentSsid();
    doc["program"] = gRunningName;
    doc["builtin"] = gRunningBuiltin;
    doc["unsaved"] = gRunningUnsaved;
    doc["loadId"] = gLoadId;
    doc["brightness"] = outputMaster();
    doc["palette"] = outputPaletteName();
    doc["fps"] = outputFps();
    addRuntimeError(doc.as<JsonObject>());
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

bool netMeasuring() {
    return gCfg && gCfg->measuring;
}

int netMeasureEnd() {
    if (!gCfg) return 0;
    return gCfg->measureEnd;
}

static int clampMeasure(int end) {
    if (end < 4) end = 4;
    if (end > kMaxLeds) end = kMaxLeds;
    return end;
}

static void handleMeasure() {
    String action;
    int by = 1;
    if (asJson()) {
        JsonDocument doc;
        if (deserializeJson(doc, gServer.arg("plain"))) {
            sendError(400, "invalid json", nullptr);
            return;
        }
        action = doc["action"] | "";
        if (!doc["by"].isNull() && !jsonStrictInt(doc["by"], by)) {
            sendError(400, "invalid step", nullptr);
            return;
        }
    } else {
        action = gServer.arg("action");
        if (gServer.hasArg("by") && !parseStrictInt(gServer.arg("by"), by)) {
            sendError(400, "invalid step", nullptr);
            return;
        }
    }
    if (action == "start") {
        DeviceConfig next = *gCfg;
        if (!next.measuring) {
            next.measureEnd = clampMeasure(next.length);
            next.measuring = true;
        }
        if (!saveConfig(next)) {
            sendError(500, "storage failed", nullptr);
            return;
        }
        *gCfg = next;
        gMeasureSaveAt = 0;
        sendRestarting();
        return;
    }
    if (!gCfg->measuring) {
        sendError(400, "not measuring", nullptr);
        return;
    }
    if (action == "move") {
        int64_t moved = (int64_t)gCfg->measureEnd + (int64_t)by;
        if (moved < 4) moved = 4;
        if (moved > kMaxLeds) moved = kMaxLeds;
        gCfg->measureEnd = (int)moved;
        gMeasureSaveAt = scheduleAfter(400);
        JsonDocument doc;
        doc["ok"] = true;
        doc["end"] = gCfg->measureEnd;
        String body;
        serializeJson(doc, body);
        sendJson(200, body);
        return;
    }
    if (action == "save") {
        DeviceConfig next = *gCfg;
        next.length = next.measureEnd;
        next.measuring = false;
        if (!saveConfig(next)) {
            sendError(500, "storage failed", nullptr);
            return;
        }
        *gCfg = next;
        gMeasureSaveAt = 0;
        JsonDocument doc;
        doc["ok"] = true;
        doc["restart"] = true;
        doc["length"] = gCfg->length;
        String body;
        serializeJson(doc, body);
        sendJson(200, body);
        requestRestart();
        return;
    }
    if (action == "cancel") {
        DeviceConfig next = *gCfg;
        next.measuring = false;
        if (!saveConfig(next)) {
            sendError(500, "storage failed", nullptr);
            return;
        }
        *gCfg = next;
        gMeasureSaveAt = 0;
        sendRestarting();
        return;
    }
    sendError(400, "invalid action", nullptr);
}

static void handleLedGet() {
    JsonDocument doc;
    doc["type"] = gCfg->ledType;
    doc["order"] = gCfg->colorOrder;
    doc["length"] = gCfg->length;
    doc["pin"] = gCfg->pin;
    doc["brightness"] = outputMaster();
    JsonObject measure = doc["measure"].to<JsonObject>();
    measure["active"] = gCfg->measuring;
    measure["end"] = gCfg->measureEnd;
    measure["max"] = kMaxLeds;
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static bool readLed(DeviceConfig& next, String& error, bool& hasBrightness) {
    next = *gCfg;
    hasBrightness = false;
    if (asJson()) {
        JsonDocument doc;
        if (deserializeJson(doc, gServer.arg("plain"))) {
            error = "invalid json";
            return false;
        }
        if (!doc["type"].isNull()) next.ledType = doc["type"] | "";
        if (!doc["order"].isNull()) next.colorOrder = doc["order"] | "";
        if (!doc["length"].isNull() && !jsonStrictInt(doc["length"], next.length)) {
            error = "invalid length";
            return false;
        }
        if (!doc["pin"].isNull() && !jsonStrictInt(doc["pin"], next.pin)) {
            error = "invalid pin";
            return false;
        }
        if (!doc["brightness"].isNull()) {
            if (!jsonStrictInt(doc["brightness"], next.brightness)) {
                error = "invalid brightness";
                return false;
            }
            hasBrightness = true;
        }
    } else {
        if (gServer.hasArg("type")) next.ledType = gServer.arg("type");
        if (gServer.hasArg("order")) next.colorOrder = gServer.arg("order");
        if (gServer.hasArg("length") && !parseStrictInt(gServer.arg("length"), next.length)) {
            error = "invalid length";
            return false;
        }
        if (gServer.hasArg("pin") && !parseStrictInt(gServer.arg("pin"), next.pin)) {
            error = "invalid pin";
            return false;
        }
        if (gServer.hasArg("brightness")) {
            if (!parseStrictInt(gServer.arg("brightness"), next.brightness)) {
                error = "invalid brightness";
                return false;
            }
            hasBrightness = true;
        }
    }
    if (!validLedType(next.ledType)) {
        error = "invalid led type";
        return false;
    }
    if (!validColorOrder(next.colorOrder)) {
        error = "invalid color order";
        return false;
    }
    if (!validLength(next.length)) {
        error = "invalid length";
        return false;
    }
    if (!validDataPin(next.pin)) {
        error = "invalid pin";
        return false;
    }
    if (!validBrightness(next.brightness)) {
        error = "invalid brightness";
        return false;
    }
    return true;
}

static void rememberMaster(int value) {
    gCfg->master = value;
    gCfg->brightness = value;
    outputSetBrightness(value);
    gMasterSaveAt = scheduleAfter(400);
}

static void handlePaletteGet() {
    JsonDocument doc;
    doc["palette"] = outputPaletteName();
    JsonArray names = doc["palettes"].to<JsonArray>();
    for (int i = 0; i < paletteCount(); i++) names.add(paletteName(i));
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static void handlePalettePost() {
    String name;
    if (asJson()) {
        JsonDocument doc;
        if (deserializeJson(doc, gServer.arg("plain"))) {
            sendError(400, "invalid json", nullptr);
            return;
        }
        name = doc["palette"] | "";
    } else if (gServer.hasArg("palette")) {
        name = gServer.arg("palette");
    }
    if (findPalette(name.c_str()) < 0) {
        sendError(400, "invalid palette", nullptr);
        return;
    }
    DeviceConfig next = *gCfg;
    next.palette = name;
    if (!saveConfig(next)) {
        sendError(500, "storage failed", nullptr);
        return;
    }
    *gCfg = next;
    outputSetPalette(name.c_str());
    sendOk();
}

static void handleBrightnessGet() {
    JsonDocument doc;
    doc["brightness"] = outputMaster();
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static void handleBrightnessPost() {
    int value = -1;
    if (asJson()) {
        JsonDocument doc;
        if (deserializeJson(doc, gServer.arg("plain"))) {
            sendError(400, "invalid json", nullptr);
            return;
        }
        if (!jsonStrictInt(doc["brightness"], value)) {
            sendError(400, "invalid brightness", nullptr);
            return;
        }
    } else if (!gServer.hasArg("brightness") || !parseStrictInt(gServer.arg("brightness"), value)) {
        sendError(400, "invalid brightness", nullptr);
        return;
    }
    if (!validBrightness(value)) {
        sendError(400, "invalid brightness", nullptr);
        return;
    }
    rememberMaster(value);
    sendOk();
}

static void handleLedPut() {
    DeviceConfig next;
    String error;
    bool hasBrightness = false;
    if (!readLed(next, error, hasBrightness)) {
        sendError(400, error.c_str(), nullptr);
        return;
    }
    // Measuring boots with a kMaxLeds buffer, so leaving it always needs a
    // restart to come back up at the configured length.
    bool wasMeasuring = gCfg->measuring;
    bool hardware = next.ledType != gCfg->ledType || next.colorOrder != gCfg->colorOrder ||
                    next.length != gCfg->length || next.pin != gCfg->pin;
    if (hasBrightness) {
        next.master = next.brightness;
    } else {
        next.master = gCfg->master;
    }
    next.measuring = false;
    if (!saveConfig(next)) {
        sendError(500, "storage failed", nullptr);
        return;
    }
    *gCfg = next;
    if (hasBrightness) {
        outputSetBrightness(gCfg->brightness);
    }
    gMasterSaveAt = 0;
    gMeasureSaveAt = 0;
    if (hardware || wasMeasuring) {
        Serial.printf("LED %s %s pin %d x %d, restarting\n",
                      gCfg->ledType.c_str(), gCfg->colorOrder.c_str(), gCfg->pin, gCfg->length);
        sendRestarting();
        return;
    }
    sendOk();
}

static const char* paramTypeName(ParameterType type) {
    if (type == PARAM_BOOLEAN) return "boolean";
    if (type == PARAM_ENUM) return "enum";
    return "number";
}

static bool paramValueFromJson(Parameter* param, JsonVariant in, Value& out, String& error) {
    if (in.isNull()) {
        error = "invalid value";
        return false;
    }
    if (param->type == PARAM_BOOLEAN) {
        bool on = false;
        if (in.is<bool>()) on = in.as<bool>();
        else if (in.is<const char*>()) {
            String text = in.as<String>();
            on = text == "1" || text.equalsIgnoreCase("true") || text.equalsIgnoreCase("on");
        } else {
            on = in.as<float>() != 0;
        }
        out = Value(on ? 1.0f : 0.0f);
        return true;
    }
    if (param->type == PARAM_ENUM) {
        int index = -1;
        if (in.is<const char*>()) {
            String text = in.as<String>();
            for (int i = 0; i < (int)param->enumValues.size(); i++) {
                if (param->enumValues[i].equalsIgnoreCase(text)) {
                    index = i;
                    break;
                }
            }
            if (index < 0 && !parseStrictInt(text, index)) {
                error = "invalid value";
                return false;
            }
        } else if (!jsonStrictInt(in, index)) {
            error = "invalid value";
            return false;
        }
        if (index < 0 || index >= (int)param->enumValues.size()) {
            error = "invalid value";
            return false;
        }
        out = Value((float)index);
        return true;
    }
    float num = 0.0f;
    if (!jsonStrictFloat(in, num)) {
        error = "invalid value";
        return false;
    }
    out = Value(num);
    return true;
}

static void handleParamsGet() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    BasicLEDController* controller = outputController();
    if (controller) {
        std::vector<Parameter> params = controller->getAllParameters();
        for (size_t i = 0; i < params.size(); i++) {
            const Parameter& param = params[i];
            JsonObject item = arr.add<JsonObject>();
            item["name"] = param.name;
            item["type"] = paramTypeName(param.type);
            if (param.type == PARAM_BOOLEAN) {
                item["value"] = param.currentValue.asNumber() != 0;
            } else if (param.type == PARAM_ENUM) {
                item["value"] = (int)param.currentValue.asNumber();
                JsonArray values = item["values"].to<JsonArray>();
                for (size_t v = 0; v < param.enumValues.size(); v++) values.add(param.enumValues[v]);
            } else {
                item["value"] = param.currentValue.asNumber();
                item["min"] = param.minValue;
                item["max"] = param.maxValue;
                item["step"] = param.stepValue == 0 ? 1.0f : param.stepValue;
            }
        }
    }
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static void handleParamsPost() {
    BasicLEDController* controller = outputController();
    if (!controller) {
        sendError(400, "no program", nullptr);
        return;
    }
    String name;
    Value value;
    String error;
    bool ok = false;
    if (asJson()) {
        JsonDocument doc;
        if (deserializeJson(doc, gServer.arg("plain"))) {
            sendError(400, "invalid json", nullptr);
            return;
        }
        name = doc["name"].as<String>();
        Parameter* param = controller->getParameter(name);
        if (!param) {
            sendError(404, "parameter not found", nullptr);
            return;
        }
        ok = paramValueFromJson(param, doc["value"], value, error);
    } else {
        name = gServer.arg("name");
        Parameter* param = controller->getParameter(name);
        if (!param) {
            sendError(404, "parameter not found", nullptr);
            return;
        }
        if (!gServer.hasArg("value")) {
            sendError(400, "invalid value", nullptr);
            return;
        }
        JsonDocument box;
        String raw = gServer.arg("value");
        if (param->type == PARAM_NUMBER) {
            float num = 0.0f;
            if (!parseStrictFloat(raw, num)) {
                sendError(400, "invalid value", nullptr);
                return;
            }
            box.set(num);
        } else {
            box.set(raw);
        }
        ok = paramValueFromJson(param, box.as<JsonVariant>(), value, error);
    }
    if (!ok) {
        sendError(400, error.c_str(), nullptr);
        return;
    }
    controller->setParameterValue(name, value);
    sendOk();
}

static void handlePrograms() {
    std::vector<ProgramInfo> programs;
    listPrograms(programs);
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (size_t i = 0; i < programs.size(); i++) {
        JsonObject item = arr.add<JsonObject>();
        item["name"] = programs[i].name;
        item["builtin"] = programs[i].builtin;
        item["bytes"] = programs[i].bytes;
        item["active"] = !gRunningUnsaved && programs[i].name == gRunningName;
    }
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static void handleProgramGet() {
    String name = gServer.arg("name");
    String source;
    bool builtin = false;
    if (!lookupSource(name, source, builtin)) {
        sendError(404, "program not found", nullptr);
        return;
    }
    gServer.send(200, "text/plain", source);
}

static bool wantsActivate() {
    if (!gServer.hasArg("activate")) return false;
    String value = gServer.arg("activate");
    return value != "0" && value != "false";
}

// Persists `source` under `name` and, if requested, activates it.
// The program must always parse *before* it is written to disk, so an invalid
// replacement never overwrites a stored/active program (which would otherwise
// only surface as a boot failure after a later restart). When activating, it is
// queued only after the write succeeds, so a failed write never leaves the
// board running (and remembering) a program it didn't save.
static void saveAndRespond(const String& name, const String& source) {
    String error;
    if (!validateUserProgram(name, source, error)) {
        sendError(storageErrorCode(error), error.c_str(), nullptr);
        return;
    }
    ApplyResult result = checkProgram(source);
    if (!result.ok) {
        sendApply(result);
        return;
    }
    if (!writeUserProgramFile(name, source, error)) {
        result.ok = false;
        result.pending = false;
        result.error = error;
        result.diagnostics.clear();
        sendApply(result);
        return;
    }
    if (wantsActivate()) {
        netEnqueue(name, source, true);
        sendApply(result);
    } else {
        sendOk();
    }
}

static void handleProgramPut() {
    String name = gServer.arg("name");
    String source = gServer.arg("plain");
    saveAndRespond(name, source);
}

static void handleUploadData() {
    HTTPUpload& upload = gServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        gUpload = "";
        gUploadOverflow = false;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (gUploadOverflow) return;
        if (gUpload.length() + upload.currentSize > kMaxProgramBytes) {
            gUploadOverflow = true;
            gUpload = "";
            return;
        }
        if (!gUpload.concat((const char*)upload.buf, upload.currentSize)) {
            gUploadOverflow = true;
            gUpload = "";
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        gUpload = "";
        gUploadOverflow = false;
    }
}

static void handleUpload() {
    if (gUploadOverflow) {
        gUpload = "";
        gUploadOverflow = false;
        sendError(400, "program too large", nullptr);
        return;
    }
    String name = gServer.arg("name");
    String source = gUpload;
    gUpload = "";
    saveAndRespond(name, source);
}

static void handleActivate() {
    String name = gServer.arg("name");
    String source;
    bool builtin = false;
    if (!lookupSource(name, source, builtin)) {
        sendError(404, "program not found", nullptr);
        return;
    }
    sendApply(queueApply(name, source, true));
}

static void handleDelete() {
    String name = gServer.arg("name");
    String error;
    if (!deleteUserProgram(name, error)) {
        int code = error == "program not found" ? 404 : storageErrorCode(error);
        sendError(code, error.c_str(), nullptr);
        return;
    }
    if (gPending && gPendingRemember && gPendingName == name) {
        gPending = false;
        gPendingRemember = false;
        gPendingName = "";
        gPendingSource = "";
    }
    if (gCfg->activeProgram == name) {
        gCfg->activeProgram = kDefaultProgram;
        if (!saveConfig(*gCfg)) {
            Serial.println("Failed to persist active program change after delete");
        }
    }
    if (!gRunningUnsaved && gRunningName == name) {
        String source;
        bool builtin = false;
        if (lookupSource(kDefaultProgram, source, builtin)) {
            sendApply(queueApply(kDefaultProgram, source, true));
            return;
        }
    }
    sendOk();
}

static void handleRun() {
    String source = gServer.arg("plain");
    if (source.length() == 0) {
        sendError(400, "empty program", nullptr);
        return;
    }
    if (source.length() > kMaxProgramBytes) {
        sendError(400, "program too large", nullptr);
        return;
    }
    sendApply(queueApply("", source, false));
}

static void handleWifiGet() {
    JsonDocument doc;
    doc["mode"] = modeName();
    doc["ssid"] = currentSsid();
    doc["ip"] = currentIp();
    doc["hostname"] = String(kHostname) + ".local";
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static bool gScanRunning = false;
static uint32_t gScanStartedAt = 0;

// Wi-Fi scans block for 1-3s. Run them asynchronously so the main loop (and
// LED rendering) keeps running; the client polls this endpoint until "done".
static void handleWifiScan() {
    if (gScanRunning && millis() - gScanStartedAt > 15000) {
        WiFi.scanDelete();
        gScanRunning = false;
    }
    if (!gScanRunning) {
        if (WiFi.getMode() == WIFI_AP) WiFi.mode(WIFI_AP_STA);
        int started = WiFi.scanNetworks(true);
        if (started == WIFI_SCAN_FAILED) {
            sendError(503, "wifi scan failed", nullptr);
            return;
        }
        gScanRunning = true;
        gScanStartedAt = millis();
    }
    int count = WiFi.scanComplete();
    if (count == WIFI_SCAN_RUNNING) {
        JsonDocument doc;
        doc["done"] = false;
        String body;
        serializeJson(doc, body);
        sendJson(200, body);
        return;
    }
    gScanRunning = false;
    if (count < 0) {
        WiFi.scanDelete();
        sendError(503, "wifi scan failed", nullptr);
        return;
    }
    JsonDocument doc;
    doc["done"] = true;
    JsonArray arr = doc["networks"].to<JsonArray>();
    int limit = count > 20 ? 20 : count;
    for (int i = 0; i < limit; i++) {
        JsonObject item = arr.add<JsonObject>();
        item["ssid"] = WiFi.SSID(i);
        item["rssi"] = WiFi.RSSI(i);
        item["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static bool readWifi(String& ssid, String& password, String& error) {
    if (asJson()) {
        JsonDocument doc;
        if (deserializeJson(doc, gServer.arg("plain"))) {
            error = "invalid json";
            return false;
        }
        // `| ""` yields "" for a missing, null or non-string field; as<String>()
        // would serialize those to the literal "null".
        ssid = doc["ssid"] | "";
        password = doc["password"] | "";
    } else {
        ssid = gServer.arg("ssid");
        password = gServer.arg("password");
    }
    if (ssid.length() == 0 || ssid.length() > 32) {
        error = "invalid ssid";
        return false;
    }
    if (password.length() > 63) {
        error = "invalid password";
        return false;
    }
    return true;
}

static void handleWifiPost() {
    String ssid;
    String password;
    String error;
    if (!readWifi(ssid, password, error)) {
        sendError(400, error.c_str(), nullptr);
        return;
    }
    DeviceConfig next = *gCfg;
    next.wifiSsid = ssid;
    next.wifiPassword = password;
    if (!saveConfig(next)) {
        sendError(500, "storage failed", nullptr);
        return;
    }
    *gCfg = next;
    Serial.printf("Saved network %s, restarting\n", ssid.c_str());
    sendRestarting();
}

static void handleWifiReset() {
    DeviceConfig next = *gCfg;
    clearWifi(next);
    if (!saveConfig(next)) {
        sendError(500, "storage failed", nullptr);
        return;
    }
    *gCfg = next;
    Serial.println("Cleared Wi-Fi, restarting");
    sendRestarting();
}

static void handleRestart() {
    Serial.println("Restart requested");
    sendRestarting();
}

static void redirectHome() {
    gServer.sendHeader("Location", gApOn ? "http://192.168.4.1/" : "/", true);
    gServer.send(302, "text/plain", "");
}

static void handleNotFound() {
    String uri = gServer.uri();
    if (uri.startsWith("/api/")) {
        sendError(404, "not found", nullptr);
        return;
    }
    if (gApOn) {
        redirectHome();
        return;
    }
    gServer.send(404, "text/plain", "not found");
}

static void handlePreview() {
    uint8_t rgb[64 * 3];
    int n = outputPreview(rgb, 64);
    char body[64 * 16];
    size_t used = 0;
    body[used++] = '[';
    for (int i = 0; i < n; i++) {
        int wrote = snprintf(body + used, sizeof(body) - used, "%s[%u,%u,%u]",
                             i ? "," : "", rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
        if (wrote < 0 || (size_t)wrote >= sizeof(body) - used) break;
        used += (size_t)wrote;
    }
    if (used + 1 < sizeof(body)) body[used++] = ']';
    body[used] = '\0';
    sendJson(200, body);
}

static void handleRoot() {
    sendHome();
}

// SECURITY NOTE: none of these routes require authentication or CSRF
// protection. Once the device joins a station Wi-Fi network, any host on
// that LAN can upload/activate programs, change LED/Wi-Fi settings, or
// restart the device. This is accepted as a reasonable tradeoff for a
// hobby/home controller intended to run on a trusted home network only —
// do not expose this device's HTTP port to an untrusted network or the
// public internet without adding an auth layer (e.g. HTTP Basic Auth or a
// shared token) in front of it.
static void registerRoutes() {
    gServer.on("/", HTTP_GET, handleRoot);
    gServer.on("/generate_204", HTTP_GET, redirectHome);
    gServer.on("/gen_204", HTTP_GET, redirectHome);
    gServer.on("/hotspot-detect.html", HTTP_GET, redirectHome);
    gServer.on("/connecttest.txt", HTTP_GET, redirectHome);
    gServer.on("/ncsi.txt", HTTP_GET, redirectHome);
    gServer.on("/fwlink", HTTP_GET, redirectHome);

    gServer.on("/api/status", HTTP_GET, handleStatus);
    gServer.on("/api/preview", HTTP_GET, handlePreview);
    gServer.on("/api/palette", HTTP_GET, handlePaletteGet);
    gServer.on("/api/palette", HTTP_POST, handlePalettePost);
    gServer.on("/api/palette", HTTP_PUT, handlePalettePost);
    gServer.on("/api/brightness", HTTP_GET, handleBrightnessGet);
    gServer.on("/api/brightness", HTTP_POST, handleBrightnessPost);
    gServer.on("/api/brightness", HTTP_PUT, handleBrightnessPost);
    gServer.on("/api/led", HTTP_GET, handleLedGet);
    gServer.on("/api/measure", HTTP_POST, handleMeasure);
    gServer.on("/api/led", HTTP_PUT, handleLedPut);
    gServer.on("/api/led", HTTP_POST, handleLedPut);
    gServer.on("/api/programs", HTTP_GET, handlePrograms);
    gServer.on("/api/params", HTTP_GET, handleParamsGet);
    gServer.on("/api/params", HTTP_POST, handleParamsPost);
    gServer.on("/api/program", HTTP_GET, handleProgramGet);
    gServer.on("/api/program", HTTP_PUT, handleProgramPut);
    gServer.on("/api/program", HTTP_DELETE, handleDelete);
    gServer.on("/api/program/upload", HTTP_POST, handleUpload, handleUploadData);
    gServer.on("/api/program/activate", HTTP_POST, handleActivate);
    gServer.on("/api/program/delete", HTTP_POST, handleDelete);
    gServer.on("/api/run", HTTP_POST, handleRun);
    gServer.on("/api/wifi", HTTP_GET, handleWifiGet);
    gServer.on("/api/wifi", HTTP_POST, handleWifiPost);
    gServer.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
    gServer.on("/api/wifi/reset", HTTP_POST, handleWifiReset);
    gServer.on("/api/restart", HTTP_POST, handleRestart);
    gServer.onNotFound(handleNotFound);
}

void netBegin(DeviceConfig& cfg) {
    gCfg = &cfg;
    pinMode(kButtonPin, INPUT_PULLUP);
    WiFi.persistent(false);
    WiFi.setSleep(false);
    WiFi.setHostname(kHostname);
    if (cfg.wifiSsid.length() == 0) {
        startAp(false);
        gPhase = PhaseAp;
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPassword.c_str());
        gPhase = PhaseConnecting;
        gStaStarted = millis();
        Serial.printf("Joining %s\n", cfg.wifiSsid.c_str());
    }
    registerRoutes();
    const char* headers[] = {"Accept"};
    gServer.collectHeaders(headers, 1);
    gServer.begin();
}

void netLoop() {
    if (!gCfg) return;
    if (gMasterSaveAt != 0 && (int32_t)(millis() - gMasterSaveAt) >= 0) {
        gMasterSaveAt = 0;
        if (!saveConfig(*gCfg)) {
            Serial.println("Failed to persist brightness change");
        }
    }
    if (gMeasureSaveAt != 0 && (int32_t)(millis() - gMeasureSaveAt) >= 0) {
        gMeasureSaveAt = 0;
        if (!saveConfig(*gCfg)) {
            Serial.println("Failed to persist measurement progress");
        }
    }
    pollButton();
    if (gPhase == PhaseConnecting) {
        if (WiFi.status() == WL_CONNECTED) {
            gPhase = PhaseSta;
            Serial.printf("STA %s\n", WiFi.localIP().toString().c_str());
            startMdns();
        } else if (millis() - gStaStarted > 20000) {
            Serial.println("Join timed out, starting access point");
            startAp(true);
            gPhase = PhaseFallback;
        }
    } else if (gPhase == PhaseSta && WiFi.status() != WL_CONNECTED) {
        Serial.println("Station dropped, starting access point");
        if (!gApOn) startAp(true);
        gPhase = PhaseFallback;
    } else if (gPhase == PhaseFallback && WiFi.status() == WL_CONNECTED) {
        gPhase = PhaseSta;
        if (gApOn) {
            gDns.stop();
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_STA);
            gApOn = false;
        }
        Serial.printf("STA %s\n", WiFi.localIP().toString().c_str());
        startMdns();
    }
    if (gApOn) gDns.processNextRequest();
    // Backstop for the rule above queueApply(): if anything a handler calls
    // ever reaches netDelay(), don't serve a second request from inside it.
    static bool inHandler = false;
    if (!inHandler) {
        inHandler = true;
        gServer.handleClient();
        inHandler = false;
    }
    if (gRestartAt != 0 && (int32_t)(millis() - gRestartAt) >= 0) {
        ESP.restart();
    }
}
