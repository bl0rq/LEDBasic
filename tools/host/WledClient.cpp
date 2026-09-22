#include "WledClient.h"

#include <cstring>
#include <utility>

namespace ledbasic {

void WledClient::setBaseUrl(std::string base) {
    while (!base.empty() && (base.back() == ' ' || base.back() == '\r' || base.back() == '\n')) base.pop_back();
    base_ = std::move(base);
}

static DeviceResponse failed(const std::string& error) {
    DeviceResponse r;
    r.error = error;
    return r;
}

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

static std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

struct UrlParts {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = 0;
    bool https = false;
};

static bool crackUrl(const std::string& url, UrlParts& out, std::string& err) {
    std::wstring wide = widen(url);
    URL_COMPONENTS uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256];
    wchar_t path[2048];
    wchar_t extra[2048];
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    uc.lpszExtraInfo = extra;
    uc.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &uc)) {
        err = "bad device url";
        return false;
    }
    out.host.assign(host, uc.dwHostNameLength);
    out.path.assign(path, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) out.path.append(extra, uc.dwExtraInfoLength);
    if (out.path.empty()) out.path = L"/";
    out.port = uc.nPort;
    out.https = uc.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

DeviceResponse WledClient::request(const std::string& method, const std::string& path,
                                   const std::string& body, const std::string& contentType) const {
    if (base_.empty()) return failed("set a device host");
    std::string url = joinDeviceUrl(base_, path);
    if (url.empty()) return failed("set a device host");

    UrlParts parts;
    std::string err;
    if (!crackUrl(url, parts, err)) return failed(err);

    HINTERNET ses = WinHttpOpen(L"LEDBasic/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return failed("could not open HTTP");
    WinHttpSetTimeouts(ses, 2000, 2000, 2000, 3000);
    HINTERNET con = WinHttpConnect(ses, parts.host.c_str(), parts.port, 0);
    if (!con) {
        WinHttpCloseHandle(ses);
        return failed("could not connect");
    }
    DWORD flags = parts.https ? WINHTTP_FLAG_SECURE : 0;
    std::wstring wmethod = widen(method);
    HINTERNET req = WinHttpOpenRequest(con, wmethod.c_str(), parts.path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return failed("could not open request");
    }

    std::wstring headers;
    if (!contentType.empty()) headers = L"Content-Type: " + widen(contentType) + L"\r\n";
    BOOL sent = WinHttpSendRequest(
        req,
        headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        headers.empty() ? 0 : (DWORD)-1,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
        (DWORD)body.size(),
        (DWORD)body.size(),
        0);
    if (!sent || !WinHttpReceiveResponse(req, nullptr)) {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return failed("device did not respond");
    }

    DeviceResponse result;
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    result.status = (int)status;

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail)) break;
        if (avail == 0) break;
        std::string chunk(avail, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(req, &chunk[0], avail, &read)) break;
        chunk.resize(read);
        result.body += chunk;
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    if (result.status == 0) result.error = "device did not respond";
    return result;
}

#else

DeviceResponse WledClient::request(const std::string&, const std::string&, const std::string&,
                                   const std::string&) const {
    return failed("the device link is built for Windows");
}

#endif

static std::string httpError(const DeviceResponse& r, const char* fallback) {
    if (!r.error.empty()) return r.error;
    if (!r.body.empty()) return r.body;
    return fallback;
}

bool WledClient::listPrograms(DeviceCatalog& out, std::string& err) const {
    DeviceResponse r = request("GET", "/ledbasic/programs", "", "");
    if (!r.error.empty() || r.status != 200) {
        err = httpError(r, "could not list scripts");
        return false;
    }
    if (!parseProgramList(r.body, out)) {
        err = "could not read the script list";
        return false;
    }
    return true;
}

bool WledClient::getSource(const std::string& name, std::string& source, std::string& err) const {
    DeviceResponse r = request("GET", "/ledbasic/program?name=" + urlEncodeQuery(name), "", "");
    if (!r.error.empty() || r.status != 200) {
        err = httpError(r, "could not pull the script");
        return false;
    }
    source = std::move(r.body);
    return true;
}

bool WledClient::putSource(const std::string& name, const std::string& source, std::string& err) const {
    DeviceResponse r = request("PUT", "/ledbasic/program?name=" + urlEncodeQuery(name), source, "text/plain");
    if (!r.error.empty() || (r.status != 200 && r.status != 202)) {
        err = httpError(r, "could not push the script");
        return false;
    }
    return true;
}

bool WledClient::removeProgram(const std::string& name, std::string& err) const {
    DeviceResponse r = request("DELETE", "/ledbasic/program?name=" + urlEncodeQuery(name), "", "");
    if (!r.error.empty() || r.status != 200) {
        err = httpError(r, "could not delete the script");
        return false;
    }
    return true;
}

bool WledClient::activate(const std::string& name, std::string& err) const {
    std::string body = std::string("{\"LEDBasic\":{\"programName\":\"") + name + "\"}}";
    DeviceResponse r = request("POST", "/json/state", body, "application/json");
    if (!r.error.empty() || r.status != 200) {
        err = httpError(r, "could not run the script");
        return false;
    }
    return true;
}

}
