#include "DeviceClient.h"
#include "Utf8Path.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace ledbasic {
namespace {

std::string trim(std::string text) {
    size_t begin = 0;
    while (begin < text.size() && std::isspace((unsigned char)text[begin])) begin++;
    size_t end = text.size();
    while (end > begin && std::isspace((unsigned char)text[end - 1])) end--;
    return text.substr(begin, end - begin);
}

bool startsWithNoCase(const std::string& text, const char* prefix) {
    size_t i = 0;
    while (prefix[i] != '\0') {
        if (i >= text.size()) return false;
        if (std::tolower((unsigned char)text[i]) != std::tolower((unsigned char)prefix[i])) return false;
        i++;
    }
    return true;
}

std::string urlEncode(const std::string& text) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back((char)c);
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

std::string jsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
            out += buf;
        } else {
            out.push_back((char)c);
        }
    }
    return out;
}

struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::vector<std::pair<std::string, JsonValue>> objVal;

    const JsonValue* field(const char* key) const {
        if (type != Object) return nullptr;
        for (const auto& kv : objVal) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }

    std::string asString(const std::string& fallback = "") const {
        return type == String ? strVal : fallback;
    }

    int asInt(int fallback) const {
        if (type != Number || !std::isfinite(numVal)) return fallback;
        return (int)numVal;
    }

    bool asBool(bool fallback = false) const {
        return type == Bool ? boolVal : fallback;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& src) : s_(src) {}

    bool parse(JsonValue& out) {
        skipWs();
        if (!parseValue(out, 0)) return false;
        skipWs();
        return i_ == s_.size();
    }

private:
    const std::string& s_;
    size_t i_ = 0;

    void skipWs() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\r' || s_[i_] == '\n')) {
            i_++;
        }
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp <= 0x7F) {
            out.push_back((char)cp);
        } else if (cp <= 0x7FF) {
            out.push_back((char)(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp <= 0xFFFF) {
            out.push_back((char)(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            out.push_back((char)(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }

    bool parseHex4(unsigned& out) {
        if (i_ + 4 > s_.size()) return false;
        unsigned v = 0;
        for (int k = 0; k < 4; k++) {
            char c = s_[i_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return false;
        }
        out = v;
        return true;
    }

    bool parseString(std::string& out) {
        if (i_ >= s_.size() || s_[i_] != '"') return false;
        i_++;
        out.clear();
        while (i_ < s_.size()) {
            unsigned char c = (unsigned char)s_[i_++];
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c == '\\') {
                if (i_ >= s_.size()) return false;
                char e = s_[i_++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        unsigned cp = 0;
                        if (!parseHex4(cp)) return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 2 <= s_.size() &&
                            s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                            size_t saved = i_;
                            i_ += 2;
                            unsigned low = 0;
                            if (parseHex4(low) && low >= 0xDC00 && low <= 0xDFFF) {
                                cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                            } else {
                                i_ = saved;
                            }
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: return false;
                }
            } else {
                out.push_back((char)c);
            }
        }
        return false;
    }

    bool parseNumber(double& out) {
        size_t start = i_;
        if (i_ < s_.size() && s_[i_] == '-') i_++;
        if (i_ >= s_.size() || !std::isdigit((unsigned char)s_[i_])) return false;
        if (s_[i_] == '0') {
            i_++;
        } else {
            while (i_ < s_.size() && std::isdigit((unsigned char)s_[i_])) i_++;
        }
        if (i_ < s_.size() && s_[i_] == '.') {
            i_++;
            if (i_ >= s_.size() || !std::isdigit((unsigned char)s_[i_])) return false;
            while (i_ < s_.size() && std::isdigit((unsigned char)s_[i_])) i_++;
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            i_++;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) i_++;
            if (i_ >= s_.size() || !std::isdigit((unsigned char)s_[i_])) return false;
            while (i_ < s_.size() && std::isdigit((unsigned char)s_[i_])) i_++;
        }
        std::string token = s_.substr(start, i_ - start);
        char* end = nullptr;
        out = std::strtod(token.c_str(), &end);
        return end != token.c_str() && std::isfinite(out);
    }

    bool parseValue(JsonValue& out, int depth) {
        if (depth > 32) return false;
        skipWs();
        if (i_ >= s_.size()) return false;
        char c = s_[i_];
        if (c == '"') {
            out = JsonValue{};
            out.type = JsonValue::String;
            return parseString(out.strVal);
        }
        if (c == '{') {
            i_++;
            out = JsonValue{};
            out.type = JsonValue::Object;
            skipWs();
            if (i_ < s_.size() && s_[i_] == '}') {
                i_++;
                return true;
            }
            for (;;) {
                skipWs();
                std::string key;
                if (!parseString(key)) return false;
                skipWs();
                if (i_ >= s_.size() || s_[i_] != ':') return false;
                i_++;
                JsonValue val;
                if (!parseValue(val, depth + 1)) return false;
                out.objVal.emplace_back(std::move(key), std::move(val));
                skipWs();
                if (i_ >= s_.size()) return false;
                if (s_[i_] == '}') {
                    i_++;
                    return true;
                }
                if (s_[i_] != ',') return false;
                i_++;
            }
        }
        if (c == '[') {
            i_++;
            out = JsonValue{};
            out.type = JsonValue::Array;
            skipWs();
            if (i_ < s_.size() && s_[i_] == ']') {
                i_++;
                return true;
            }
            for (;;) {
                JsonValue elem;
                if (!parseValue(elem, depth + 1)) return false;
                out.arrVal.push_back(std::move(elem));
                skipWs();
                if (i_ >= s_.size()) return false;
                if (s_[i_] == ']') {
                    i_++;
                    return true;
                }
                if (s_[i_] != ',') return false;
                i_++;
            }
        }
        if (s_.compare(i_, 4, "true") == 0) {
            i_ += 4;
            out = JsonValue{};
            out.type = JsonValue::Bool;
            out.boolVal = true;
            return true;
        }
        if (s_.compare(i_, 5, "false") == 0) {
            i_ += 5;
            out = JsonValue{};
            out.type = JsonValue::Bool;
            out.boolVal = false;
            return true;
        }
        if (s_.compare(i_, 4, "null") == 0) {
            i_ += 4;
            out = JsonValue{};
            out.type = JsonValue::Null;
            return true;
        }
        if (c == '-' || std::isdigit((unsigned char)c)) {
            out = JsonValue{};
            out.type = JsonValue::Number;
            return parseNumber(out.numVal);
        }
        return false;
    }
};

bool parseJson(const std::string& text, JsonValue& out) {
    JsonParser parser(text);
    return parser.parse(out);
}

std::string formatDiagObject(const JsonValue& diag) {
    if (diag.type != JsonValue::Object) return "";
    const JsonValue* msg = diag.field("message");
    if (!msg || msg->type != JsonValue::String || msg->strVal.empty()) return "";
    const JsonValue* line = diag.field("line");
    const JsonValue* col = diag.field("column");
    int l = line ? line->asInt(0) : 0;
    int c = col ? col->asInt(0) : 0;
    if (l > 0) return std::to_string(l) + ":" + std::to_string(c) + " " + msg->strVal;
    return msg->strVal;
}

// Keep this list in sync with the firmware's builtin catalog
// (firmware/src/catalog.cpp kBuiltins).
const char* kBuiltins[] = {
    "Aurora", "BPM", "BackgroundStars", "BikeParked", "BikeRolling", "Blink",
    "BouncingBalls", "Breathing", "Candle", "Chase", "Colorloop", "Colorwaves",
    "Dissolve", "DoubleRainbow", "Drip", "Fade", "Fairy", "Fire", "FireFlicker",
    "Fireworks", "Flow", "Glitter", "Gradient", "HalloweenEyes", "Heartbeat",
    "ICU", "Juggle", "Lightning", "Loading", "Matrix", "MovingComets",
    "Oscillate", "Pacifica", "Percent", "Phased", "Plasma", "Popcorn", "Pride",
    "PulsingCenter", "Railway", "Rainbow", "RandomColors", "Ripple", "Running",
    "Saw", "Scan", "Shimmer", "SineWave", "Sinelon", "Sparkle", "SplitRainbow",
    "Spots", "Strobe", "Sunrise", "Sweep", "TVSimulator", "Theater",
    "TrafficLight", "TriChase", "Twinkle", "TwoDots", "WashingMachine", "Wipe",
};

#ifdef _WIN32
std::wstring widen(const std::string& text) {
    if (text.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), out.data(), n);
    return out;
}

struct HttpHandle {
    HINTERNET handle = nullptr;
    ~HttpHandle() {
        if (handle) WinHttpCloseHandle(handle);
    }
    HttpHandle() = default;
    HttpHandle(const HttpHandle&) = delete;
    HttpHandle& operator=(const HttpHandle&) = delete;
};
#endif

}  // namespace

void DeviceClient::setBaseUrl(std::string url) {
    url = trim(std::move(url));
    if (url.empty()) {
        baseUrl.clear();
        return;
    }
    if (startsWithNoCase(url, "http://")) {
        url.replace(0, 7, "http://");
    } else if (startsWithNoCase(url, "https://")) {
        url.replace(0, 8, "https://");
    } else {
        url = "http://" + url;
    }
    size_t cut = url.find_first_of("?#");
    if (cut != std::string::npos) url.erase(cut);
    while (!url.empty() && url.back() == '/') url.pop_back();
    if (url == "http:" || url == "https:") {
        baseUrl.clear();
        return;
    }
    baseUrl = std::move(url);
}

HttpResult DeviceClient::request(const std::string& method, const std::string& path,
                                 const std::string& body, const std::string& contentType) const {
    HttpResult result;
    if (baseUrl.empty()) {
        result.error = "no device url";
        return result;
    }
#ifndef _WIN32
    (void)method;
    (void)path;
    (void)body;
    (void)contentType;
    result.error = "device control requires Windows";
    return result;
#else
    std::wstring wideUrl = widen(baseUrl + path);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256];
    wchar_t urlPath[2048];
    wchar_t extra[1024];
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = urlPath;
    parts.dwUrlPathLength = 2048;
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = 1024;
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts) || parts.dwHostNameLength == 0) {
        result.error = "bad device url";
        return result;
    }

    HttpHandle session;
    session.handle = WinHttpOpen(L"LEDBasic", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.handle) {
        result.error = "connection failed";
        return result;
    }
    WinHttpSetTimeouts(session.handle, 5000, 5000, 5000, 5000);

    HttpHandle connection;
    connection.handle = WinHttpConnect(session.handle, std::wstring(host, parts.dwHostNameLength).c_str(),
                                       parts.nPort, 0);
    if (!connection.handle) {
        result.error = "connection failed";
        return result;
    }

    std::wstring fullPath(urlPath, parts.dwUrlPathLength);
    fullPath.append(extra, parts.dwExtraInfoLength);
    DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HttpHandle request;
    request.handle = WinHttpOpenRequest(connection.handle, widen(method).c_str(), fullPath.c_str(),
                                        nullptr, WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request.handle) {
        result.error = "connection failed";
        return result;
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.handle, WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirectPolicy, sizeof(redirectPolicy));

    std::wstring headers = L"Accept: application/json\r\n";
    if (!contentType.empty()) headers += L"Content-Type: " + widen(contentType) + L"\r\n";
    BOOL sent = WinHttpSendRequest(
        request.handle,
        headers.c_str(),
        (DWORD)-1,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
        (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent || !WinHttpReceiveResponse(request.handle, nullptr)) {
        result.error = "connection failed";
        return result;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                             WINHTTP_NO_HEADER_INDEX)) {
        result.error = "connection failed";
        return result;
    }
    result.status = (int)status;

    static constexpr size_t kMaxResponseBytes = 65536;
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(request.handle, &avail)) {
            result.error = "connection interrupted";
            return result;
        }
        if (avail == 0) break;
        if (result.body.size() >= kMaxResponseBytes) {
            result.error = "response too large";
            return result;
        }
        size_t toRead = std::min<size_t>(avail, kMaxResponseBytes - result.body.size() + 1);
        std::string chunk(toRead, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request.handle, chunk.data(), (DWORD)toRead, &read)) {
            result.error = "connection interrupted";
            return result;
        }
        if (read == 0) break;
        if (result.body.size() + read > kMaxResponseBytes) {
            result.error = "response too large";
            return result;
        }
        result.body.append(chunk.data(), read);
    }
    result.transportOk = true;
    return result;
#endif
}

DeviceStatus parseDeviceStatus(const HttpResult& http) {
    DeviceStatus out;
    if (!http.transportOk) {
        out.message = http.error.empty() ? "connection failed" : http.error;
        return out;
    }
    if (http.status != 200) {
        out.message = deviceResultMessage(http);
        return out;
    }
    JsonValue doc;
    if (!parseJson(http.body, doc) || doc.type != JsonValue::Object) {
        out.message = "invalid device response";
        return out;
    }
    out.ok = true;
    if (const JsonValue* v = doc.field("mode")) out.mode = v->asString();
    if (const JsonValue* v = doc.field("program")) out.program = v->asString();
    if (const JsonValue* v = doc.field("unsaved")) out.unsaved = v->asBool();
    if (const JsonValue* v = doc.field("brightness")) out.master = v->asInt(255);
    if (const JsonValue* v = doc.field("palette")) out.palette = v->asString();
    if (out.palette.empty()) out.palette = "Rainbow";
    if (const JsonValue* diags = doc.field("diagnostics");
        diags && diags->type == JsonValue::Array && !diags->arrVal.empty()) {
        out.fault = formatDiagObject(diags->arrVal[0]);
    }
    if (out.fault.empty()) {
        if (const JsonValue* err = doc.field("error")) {
            if (err->type == JsonValue::Object) {
                out.fault = formatDiagObject(*err);
            } else if (err->type == JsonValue::String) {
                out.fault = err->strVal;
            }
        }
    }
    return out;
}

bool parseDevicePrograms(const HttpResult& http, std::vector<DeviceProgram>& out, std::string& error) {
    out.clear();
    if (!http.transportOk || http.status != 200) {
        error = deviceResultMessage(http);
        return false;
    }
    JsonValue doc;
    if (!parseJson(http.body, doc) || doc.type != JsonValue::Array) {
        error = "invalid device response";
        return false;
    }
    for (const JsonValue& item : doc.arrVal) {
        if (item.type != JsonValue::Object) continue;
        DeviceProgram program;
        if (const JsonValue* v = item.field("name")) program.name = v->asString();
        if (const JsonValue* v = item.field("builtin")) program.builtin = v->asBool();
        if (const JsonValue* v = item.field("bytes")) program.bytes = v->asInt(0);
        if (const JsonValue* v = item.field("active")) program.active = v->asBool();
        if (!program.name.empty()) out.push_back(std::move(program));
    }
    return true;
}

bool parseDeviceLed(const HttpResult& http, DeviceLed& out, std::string& error) {
    if (!http.transportOk || http.status != 200) {
        error = deviceResultMessage(http);
        return false;
    }
    JsonValue doc;
    if (!parseJson(http.body, doc) || doc.type != JsonValue::Object) {
        error = "invalid device response";
        return false;
    }
    if (const JsonValue* v = doc.field("type")) out.type = v->asString(out.type);
    if (const JsonValue* v = doc.field("order")) out.order = v->asString(out.order);
    if (const JsonValue* v = doc.field("length")) out.length = v->asInt(out.length);
    if (const JsonValue* v = doc.field("pin")) out.pin = v->asInt(out.pin);
    if (const JsonValue* v = doc.field("brightness")) out.brightness = v->asInt(out.brightness);
    return true;
}

DeviceStatus DeviceClient::status() const {
    return parseDeviceStatus(request("GET", "/api/status", "", ""));
}

HttpResult DeviceClient::runSource(const std::string& source) const {
    return request("POST", "/api/run", source, "text/plain");
}

HttpResult DeviceClient::upload(const std::string& name, const std::string& source, bool activate) const {
    std::string path = "/api/program?name=" + urlEncode(name);
    if (activate) path += "&activate=1";
    return request("PUT", path, source, "text/plain");
}

HttpResult DeviceClient::activate(const std::string& name) const {
    return request("POST", "/api/program/activate?name=" + urlEncode(name), "", "");
}

HttpResult DeviceClient::removeProgram(const std::string& name) const {
    return request("DELETE", "/api/program?name=" + urlEncode(name), "", "");
}

bool DeviceClient::programs(std::vector<DeviceProgram>& out, std::string& error) const {
    return parseDevicePrograms(request("GET", "/api/programs", "", ""), out, error);
}

bool DeviceClient::programSource(const std::string& name, std::string& out, std::string& error) const {
    HttpResult http = request("GET", "/api/program?name=" + urlEncode(name), "", "");
    if (!http.transportOk || http.status != 200) {
        error = deviceResultMessage(http);
        return false;
    }
    out = http.body;
    return true;
}

bool DeviceClient::led(DeviceLed& out, std::string& error) const {
    return parseDeviceLed(request("GET", "/api/led", "", ""), out, error);
}

HttpResult DeviceClient::setPalette(const std::string& name) const {
    std::ostringstream body;
    body << "{\"palette\":\"" << jsonEscape(name) << "\"}";
    return request("POST", "/api/palette", body.str(), "application/json");
}

HttpResult DeviceClient::setBrightness(int brightness) const {
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    std::ostringstream body;
    body << "{\"brightness\":" << brightness << "}";
    return request("POST", "/api/brightness", body.str(), "application/json");
}

HttpResult DeviceClient::putLed(const DeviceLed& led) const {
    std::ostringstream body;
    body << "{\"type\":\"" << jsonEscape(led.type) << "\",\"order\":\"" << jsonEscape(led.order)
         << "\",\"length\":" << led.length << ",\"pin\":" << led.pin
         << ",\"brightness\":" << led.brightness << "}";
    return request("PUT", "/api/led", body.str(), "application/json");
}

std::string deviceUrlPath() {
    std::string appdata = envUtf8("APPDATA");
    if (appdata.empty()) return "device.url";
    return appdata + "\\LEDBasic\\device.url";
}

bool loadDeviceUrl(std::string& url) {
    std::ifstream in(pathFromUtf8(deviceUrlPath()));
    if (!in) return false;
    std::string line;
    std::getline(in, line);
    if (trim(line).empty()) return false;
    url = trim(line);
    return true;
}

bool saveDeviceUrl(const std::string& url) {
    std::string path = deviceUrlPath();
    std::filesystem::path file = pathFromUtf8(path);
    std::error_code ec;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file);
    if (!out) return false;
    out << url;
    return (bool)out;
}

bool validUserProgramName(const std::string& name) {
    if (name.size() < 1 || name.size() > 23) return false;
    char first = name[0];
    bool startOk = (first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z');
    if (!startOk) return false;
    for (size_t i = 1; i < name.size(); i++) {
        char c = name[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_';
        if (!ok) return false;
    }
    return !isBuiltinProgramName(name);
}

bool isBuiltinProgramName(const std::string& name) {
    for (const char* builtin : kBuiltins) {
        if (name == builtin) return true;
    }
    return false;
}

std::string programNameFromPath(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    size_t dot = base.rfind('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    return base;
}

std::string deviceResultMessage(const HttpResult& result) {
    if (!result.transportOk) return result.error.empty() ? "connection failed" : result.error;
    std::string out = "device " + std::to_string(result.status);
    JsonValue doc;
    if (parseJson(result.body, doc) && doc.type == JsonValue::Object) {
        if (const JsonValue* err = doc.field("error"); err && err->type == JsonValue::String && !err->strVal.empty()) {
            out += " " + err->strVal;
        }
        if (const JsonValue* diags = doc.field("diagnostics");
            diags && diags->type == JsonValue::Array && !diags->arrVal.empty()) {
            std::string diag = formatDiagObject(diags->arrVal[0]);
            if (!diag.empty()) out += " " + diag;
        }
    }
    return out;
}

bool deviceWillRestart(const HttpResult& result) {
    JsonValue doc;
    if (!parseJson(result.body, doc) || doc.type != JsonValue::Object) return false;
    const JsonValue* v = doc.field("restart");
    return v && v->type == JsonValue::Bool && v->boolVal;
}

}  // namespace ledbasic
