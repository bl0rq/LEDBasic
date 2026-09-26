#include "DeviceClient.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

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

size_t findKey(const std::string& json, const std::string& key, size_t from = 0) {
    const std::string pat = "\"" + key + "\"";
    size_t at = json.find(pat, from);
    if (at == std::string::npos) return std::string::npos;
    at = json.find(':', at + pat.size());
    if (at == std::string::npos) return std::string::npos;
    return at + 1;
}

void skipWs(const std::string& json, size_t& i) {
    while (i < json.size() && std::isspace((unsigned char)json[i])) i++;
}

std::string parseJsonString(const std::string& json, size_t& i) {
    skipWs(json, i);
    if (i >= json.size() || json[i] != '"') return "";
    i++;
    std::string out;
    while (i < json.size()) {
        char c = json[i++];
        if (c == '"') break;
        if (c == '\\' && i < json.size()) {
            char e = json[i++];
            if (e == 'n') out.push_back('\n');
            else if (e == 't') out.push_back('\t');
            else out.push_back(e);
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string jsonString(const std::string& json, const std::string& key) {
    size_t at = findKey(json, key);
    if (at == std::string::npos) return "";
    return parseJsonString(json, at);
}

int jsonInt(const std::string& json, const std::string& key, int fallback) {
    size_t at = findKey(json, key);
    if (at == std::string::npos) return fallback;
    skipWs(json, at);
    if (at >= json.size()) return fallback;
    return std::atoi(json.c_str() + at);
}

bool jsonBool(const std::string& json, const std::string& key) {
    size_t at = findKey(json, key);
    if (at == std::string::npos) return false;
    skipWs(json, at);
    return json.compare(at, 4, "true") == 0;
}

std::string firstDiagnostic(const std::string& json) {
    size_t diag = json.find("\"diagnostics\"");
    if (diag == std::string::npos) return "";
    size_t obj = json.find('{', diag);
    if (obj == std::string::npos) return "";
    size_t end = json.find('}', obj);
    if (end == std::string::npos) return "";
    std::string one = json.substr(obj, end - obj + 1);
    int line = jsonInt(one, "line", 0);
    int column = jsonInt(one, "column", 0);
    std::string message = jsonString(one, "message");
    if (message.empty()) return "";
    return std::to_string(line) + ":" + std::to_string(column) + " " + message;
}

const char* kBuiltins[] = {
    "Rainbow", "Breathing", "SineWave", "DoubleRainbow", "Matrix",
    "BackgroundStars", "MovingComets", "PulsingCenter", "BikeParked", "BikeRolling",
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
    url = trim(url);
    if (!url.empty() && url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        url = "http://" + url;
    }
    while (!url.empty() && url.back() == '/') url.pop_back();
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
    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts)) {
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

    std::wstring headers;
    if (!contentType.empty()) headers = L"Content-Type: " + widen(contentType);
    BOOL sent = WinHttpSendRequest(
        request.handle,
        headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        headers.empty() ? 0 : (DWORD)-1,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
        (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent || !WinHttpReceiveResponse(request.handle, nullptr)) {
        result.error = "connection failed";
        return result;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    result.status = (int)status;
    result.transportOk = true;

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(request.handle, &avail) || avail == 0) break;
        std::string chunk(avail, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request.handle, chunk.data(), avail, &read)) break;
        chunk.resize(read);
        result.body += chunk;
        if (result.body.size() > 65536) break;
    }
    return result;
#endif
}

DeviceStatus DeviceClient::status() const {
    DeviceStatus out;
    HttpResult http = request("GET", "/api/status", "", "");
    if (!http.transportOk) {
        out.message = http.error.empty() ? "connection failed" : http.error;
        return out;
    }
    if (http.status != 200) {
        out.message = deviceResultMessage(http);
        return out;
    }
    out.ok = true;
    out.mode = jsonString(http.body, "mode");
    out.program = jsonString(http.body, "program");
    out.unsaved = jsonBool(http.body, "unsaved");
    out.master = jsonInt(http.body, "brightness", 255);
    out.palette = jsonString(http.body, "palette");
    if (out.palette.empty()) out.palette = "Rainbow";
    out.fault = firstDiagnostic(http.body);
    if (out.fault.empty()) {
        size_t err = http.body.find("\"error\"");
        if (err != std::string::npos) {
            size_t obj = http.body.find('{', err);
            size_t nul = http.body.find("null", err);
            if (obj != std::string::npos && (nul == std::string::npos || obj < nul)) {
                size_t end = http.body.find('}', obj);
                if (end != std::string::npos) {
                    out.fault = jsonString(http.body.substr(obj, end - obj + 1), "message");
                }
            }
        }
    }
    return out;
}

HttpResult DeviceClient::runSource(const std::string& source) const {
    return request("POST", "/api/run", source, "text/plain");
}

HttpResult DeviceClient::upload(const std::string& name, const std::string& source, bool activate) const {
    std::string path = "/api/program?name=" + name;
    if (activate) path += "&activate=1";
    return request("PUT", path, source, "text/plain");
}

HttpResult DeviceClient::activate(const std::string& name) const {
    return request("POST", "/api/program/activate?name=" + name, "", "");
}

HttpResult DeviceClient::removeProgram(const std::string& name) const {
    return request("DELETE", "/api/program?name=" + name, "", "");
}

bool DeviceClient::programs(std::vector<DeviceProgram>& out, std::string& error) const {
    out.clear();
    HttpResult http = request("GET", "/api/programs", "", "");
    if (!http.transportOk || http.status != 200) {
        error = deviceResultMessage(http);
        return false;
    }
    size_t i = 0;
    while (i < http.body.size()) {
        size_t begin = http.body.find('{', i);
        if (begin == std::string::npos) break;
        size_t end = http.body.find('}', begin);
        if (end == std::string::npos) break;
        std::string obj = http.body.substr(begin, end - begin + 1);
        DeviceProgram program;
        program.name = jsonString(obj, "name");
        program.builtin = jsonBool(obj, "builtin");
        program.bytes = jsonInt(obj, "bytes", 0);
        program.active = jsonBool(obj, "active");
        if (!program.name.empty()) out.push_back(program);
        i = end + 1;
    }
    return true;
}

bool DeviceClient::programSource(const std::string& name, std::string& out, std::string& error) const {
    HttpResult http = request("GET", "/api/program?name=" + name, "", "");
    if (!http.transportOk || http.status != 200) {
        error = deviceResultMessage(http);
        return false;
    }
    out = http.body;
    return true;
}

bool DeviceClient::led(DeviceLed& out, std::string& error) const {
    HttpResult http = request("GET", "/api/led", "", "");
    if (!http.transportOk || http.status != 200) {
        error = deviceResultMessage(http);
        return false;
    }
    out.type = jsonString(http.body, "type");
    out.order = jsonString(http.body, "order");
    out.length = jsonInt(http.body, "length", out.length);
    out.pin = jsonInt(http.body, "pin", out.pin);
    out.brightness = jsonInt(http.body, "brightness", out.brightness);
    return true;
}

HttpResult DeviceClient::setPalette(const std::string& name) const {
    std::ostringstream body;
    body << "{\"palette\":\"" << name << "\"}";
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
    body << "{\"type\":\"" << led.type << "\",\"order\":\"" << led.order
         << "\",\"length\":" << led.length << ",\"pin\":" << led.pin
         << ",\"brightness\":" << led.brightness << "}";
    return request("PUT", "/api/led", body.str(), "application/json");
}

std::string deviceUrlPath() {
    const char* appdata = std::getenv("APPDATA");
    if (!appdata || !*appdata) return "device.url";
    return std::string(appdata) + "\\LEDBasic\\device.url";
}

bool loadDeviceUrl(std::string& url) {
    std::ifstream in(deviceUrlPath());
    if (!in) return false;
    std::string line;
    std::getline(in, line);
    if (trim(line).empty()) return false;
    url = trim(line);
    return true;
}

bool saveDeviceUrl(const std::string& url) {
    std::string path = deviceUrlPath();
#ifdef _WIN32
    std::string dir = path;
    size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) {
        CreateDirectoryA(dir.substr(0, slash).c_str(), nullptr);
    }
#endif
    std::ofstream out(path);
    if (!out) return false;
    out << url;
    return (bool)out;
}

bool validUserProgramName(const std::string& name) {
    if (name.size() < 1 || name.size() > 23) return false;
    unsigned char first = (unsigned char)name[0];
    if (!std::isalpha(first)) return false;
    for (size_t i = 1; i < name.size(); i++) {
        unsigned char c = (unsigned char)name[i];
        if (!std::isalnum(c) && c != '_') return false;
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
    std::string message = jsonString(result.body, "error");
    std::string diag = firstDiagnostic(result.body);
    std::string out = "device " + std::to_string(result.status);
    if (!message.empty()) out += " " + message;
    if (!diag.empty()) out += " " + diag;
    return out;
}

bool deviceWillRestart(const HttpResult& result) {
    return result.body.find("\"restart\":true") != std::string::npos ||
           result.body.find("\"restart\": true") != std::string::npos;
}

}  // namespace ledbasic
