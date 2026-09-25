#include "net.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>

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
static bool gBusy = false;
static String gApSsid;
static uint32_t gStaStarted = 0;
static uint32_t gRestartAt = 0;

enum WifiPhase { PhaseAp, PhaseConnecting, PhaseSta, PhaseFallback };
static WifiPhase gPhase = PhaseAp;

static String gRunningName;
static bool gRunningBuiltin = false;
static bool gRunningUnsaved = false;
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

void netSetBusy(bool busy) { gBusy = busy; }
bool netBusy() { return gBusy; }
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

static void requestRestart() {
    if (gRestartAt == 0) gRestartAt = millis() + 400;
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
        saveConfig(*gCfg);
        ESP.restart();
    }
}

void netDelay(int ms, void*) {
    uint32_t start = millis();
    while ((int32_t)(millis() - start) < ms) {
        netLoop();
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
        sendError(400, message, &result.diagnostics);
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
    if (remember) {
        gRunningName = name;
        gRunningBuiltin = isBuiltin(name);
        gRunningUnsaved = false;
        if (gCfg->activeProgram != name) {
            gCfg->activeProgram = name;
            saveConfig(*gCfg);
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

static ApplyResult applyOrQueue(const String& name, const String& source, bool remember) {
    if (netBusy()) {
        netEnqueue(name, source, remember);
        ApplyResult result;
        result.ok = true;
        result.pending = true;
        return result;
    }
    return netApply(name, source, remember);
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
    doc["brightness"] = outputBrightness();
    doc["fps"] = outputFps();
    addRuntimeError(doc.as<JsonObject>());
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static void handleLedGet() {
    JsonDocument doc;
    doc["type"] = gCfg->ledType;
    doc["order"] = gCfg->colorOrder;
    doc["length"] = gCfg->length;
    doc["pin"] = gCfg->pin;
    doc["brightness"] = outputBrightness();
    String body;
    serializeJson(doc, body);
    sendJson(200, body);
}

static bool readLed(DeviceConfig& next, String& error) {
    next = *gCfg;
    if (asJson()) {
        JsonDocument doc;
        if (deserializeJson(doc, gServer.arg("plain"))) {
            error = "invalid json";
            return false;
        }
        if (!doc["type"].isNull()) next.ledType = doc["type"].as<String>();
        if (!doc["order"].isNull()) next.colorOrder = doc["order"].as<String>();
        if (!doc["length"].isNull()) next.length = doc["length"].as<int>();
        if (!doc["pin"].isNull()) next.pin = doc["pin"].as<int>();
        if (!doc["brightness"].isNull()) next.brightness = doc["brightness"].as<int>();
    } else {
        if (gServer.hasArg("type")) next.ledType = gServer.arg("type");
        if (gServer.hasArg("order")) next.colorOrder = gServer.arg("order");
        if (gServer.hasArg("length")) next.length = gServer.arg("length").toInt();
        if (gServer.hasArg("pin")) next.pin = gServer.arg("pin").toInt();
        if (gServer.hasArg("brightness")) next.brightness = gServer.arg("brightness").toInt();
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

static void handleLedPut() {
    DeviceConfig next;
    String error;
    if (!readLed(next, error)) {
        sendError(400, error.c_str(), nullptr);
        return;
    }
    bool hardware = next.ledType != gCfg->ledType || next.colorOrder != gCfg->colorOrder ||
                    next.length != gCfg->length || next.pin != gCfg->pin;
    int brightness = next.brightness;
    *gCfg = next;
    saveConfig(*gCfg);
    outputSetBrightness(brightness);
    if (hardware) {
        Serial.printf("LED %s %s pin %d x %d, restarting\n",
                      gCfg->ledType.c_str(), gCfg->colorOrder.c_str(), gCfg->pin, gCfg->length);
        sendRestarting();
        return;
    }
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

static void handleProgramPut() {
    String name = gServer.arg("name");
    String source = gServer.arg("plain");
    String error;
    if (!saveUserProgram(name, source, error)) {
        sendError(400, error.c_str(), nullptr);
        return;
    }
    if (!wantsActivate()) {
        sendOk();
        return;
    }
    sendApply(applyOrQueue(name, source, true));
}

static void handleUploadData() {
    HTTPUpload& upload = gServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        gUpload = "";
        gUploadOverflow = false;
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (gUpload.length() + upload.currentSize > kMaxProgramBytes) {
            gUploadOverflow = true;
            return;
        }
        gUpload.concat((const char*)upload.buf, upload.currentSize);
    }
}

static void handleUpload() {
    if (gUploadOverflow) {
        gUpload = "";
        sendError(400, "program too large", nullptr);
        return;
    }
    String name = gServer.arg("name");
    String error;
    if (!saveUserProgram(name, gUpload, error)) {
        gUpload = "";
        sendError(400, error.c_str(), nullptr);
        return;
    }
    String source = gUpload;
    gUpload = "";
    if (!wantsActivate()) {
        sendOk();
        return;
    }
    sendApply(applyOrQueue(name, source, true));
}

static void handleActivate() {
    String name = gServer.arg("name");
    String source;
    bool builtin = false;
    if (!lookupSource(name, source, builtin)) {
        sendError(404, "program not found", nullptr);
        return;
    }
    sendApply(applyOrQueue(name, source, true));
}

static void handleDelete() {
    String name = gServer.arg("name");
    String error;
    if (!deleteUserProgram(name, error)) {
        int code = error == "program not found" ? 404 : 400;
        sendError(code, error.c_str(), nullptr);
        return;
    }
    if (gCfg->activeProgram == name) {
        gCfg->activeProgram = kDefaultProgram;
        saveConfig(*gCfg);
    }
    if (!gRunningUnsaved && gRunningName == name) {
        String source;
        bool builtin = false;
        if (lookupSource(kDefaultProgram, source, builtin)) {
            sendApply(applyOrQueue(kDefaultProgram, source, true));
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
    sendApply(applyOrQueue("", source, false));
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

static int scanNetworks() {
    if (WiFi.getMode() == WIFI_AP) WiFi.mode(WIFI_AP_STA);
    return WiFi.scanNetworks();
}

static void handleWifiScan() {
    int count = scanNetworks();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < count; i++) {
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
        ssid = doc["ssid"].as<String>();
        password = doc["password"].as<String>();
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
    gCfg->wifiSsid = ssid;
    gCfg->wifiPassword = password;
    saveConfig(*gCfg);
    Serial.printf("Saved network %s, restarting\n", ssid.c_str());
    sendRestarting();
}

static void handleWifiReset() {
    clearWifi(*gCfg);
    saveConfig(*gCfg);
    Serial.println("Cleared Wi-Fi, restarting");
    sendRestarting();
}

static void handleRestart() {
    Serial.println("Restart requested");
    sendRestarting();
}

static void redirectHome() {
    gServer.sendHeader("Location", "/", true);
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
    gServer.on("/api/led", HTTP_GET, handleLedGet);
    gServer.on("/api/led", HTTP_PUT, handleLedPut);
    gServer.on("/api/led", HTTP_POST, handleLedPut);
    gServer.on("/api/programs", HTTP_GET, handlePrograms);
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
        Serial.printf("STA %s\n", WiFi.localIP().toString().c_str());
        startMdns();
    }
    if (gApOn) gDns.processNextRequest();
    gServer.handleClient();
    if (gRestartAt != 0 && (int32_t)(millis() - gRestartAt) >= 0) {
        ESP.restart();
    }
}
