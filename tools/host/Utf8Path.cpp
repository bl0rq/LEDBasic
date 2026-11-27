#include "Utf8Path.h"

#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace ledbasic {

namespace {

template <typename S>
std::string toStdString(const S& s) {
    // u8string() is std::string in C++17 and std::u8string in C++20.
    return std::string(s.begin(), s.end());
}

} // namespace

fs::path pathFromUtf8(const std::string& utf8) {
    try {
        return fs::u8path(utf8);
    } catch (...) {
        return fs::path();
    }
}

std::string utf8FromPath(const fs::path& p) {
    try {
        return toStdString(p.u8string());
    } catch (...) {
        return std::string();
    }
}

std::string genericUtf8FromPath(const fs::path& p) {
    try {
        return toStdString(p.generic_u8string());
    } catch (...) {
        return std::string();
    }
}

std::string envUtf8(const char* name) {
#ifdef _WIN32
    std::wstring wname = fs::path(name).wstring();
    DWORD n = GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
    if (n == 0) return std::string();
    std::wstring value(n, L'\0');
    n = GetEnvironmentVariableW(wname.c_str(), &value[0], n);
    value.resize(n);
    return utf8FromPath(fs::path(value));
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}

std::vector<std::string> utf8Args(int argc, char** argv) {
    std::vector<std::string> out;
#ifdef _WIN32
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv && wargc == argc) {
        for (int i = 0; i < wargc; i++) out.push_back(utf8FromPath(fs::path(wargv[i])));
        LocalFree(wargv);
        return out;
    }
    if (wargv) LocalFree(wargv);
#endif
    for (int i = 0; i < argc; i++) out.push_back(argv[i]);
    return out;
}

} // namespace ledbasic
