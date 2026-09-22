#include "WledJson.h"

#include <cctype>
#include <cstdlib>

namespace ledbasic {

std::string joinDeviceUrl(const std::string& base, const std::string& path) {
    std::string b = base;
    while (!b.empty() && std::isspace(static_cast<unsigned char>(b.front()))) b.erase(b.begin());
    while (!b.empty() && (std::isspace(static_cast<unsigned char>(b.back())) || b.back() == '/')) b.pop_back();
    if (b.empty()) return {};
    if (b.find("://") == std::string::npos) b = "http://" + b;
    std::string p = path;
    if (p.empty() || p[0] != '/') p = "/" + p;
    return b + p;
}

std::string urlEncodeQuery(const std::string& value) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '_' || c == '-' || c == '.') out.push_back(static_cast<char>(c));
        else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 15]);
        }
    }
    return out;
}

static bool readJsonString(const std::string& s, size_t& i, std::string& out) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    if (i >= s.size() || s[i] != '"') return false;
    ++i;
    out.clear();
    while (i < s.size()) {
        char c = s[i++];
        if (c == '\\') {
            if (i >= s.size()) return false;
            char e = s[i++];
            out.push_back(e == 'n' ? '\n' : e);
        } else if (c == '"') {
            return true;
        } else {
            out.push_back(c);
        }
    }
    return false;
}

static bool findKey(const std::string& s, size_t from, const char* key, size_t& valueAt) {
    std::string pat = std::string("\"") + key + "\"";
    size_t k = s.find(pat, from);
    if (k == std::string::npos) return false;
    size_t colon = s.find(':', k + pat.size());
    if (colon == std::string::npos) return false;
    valueAt = colon + 1;
    return true;
}

static size_t matchingObjectEnd(const std::string& s, size_t open) {
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (size_t i = open; i < s.size(); i++) {
        char c = s[i];
        if (inString) {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '{') depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0) return i;
        }
    }
    return std::string::npos;
}

bool parseProgramList(const std::string& json, DeviceCatalog& out) {
    out = DeviceCatalog{};
    size_t at = 0;
    if (!findKey(json, 0, "active", at)) return false;
    if (!readJsonString(json, at, out.active)) return false;

    size_t programsAt = 0;
    if (!findKey(json, 0, "programs", programsAt)) return false;
    size_t bracket = json.find('[', programsAt);
    if (bracket == std::string::npos) return false;

    size_t cursor = bracket + 1;
    while (cursor < json.size()) {
        size_t open = json.find('{', cursor);
        size_t closeBracket = json.find(']', cursor);
        if (open == std::string::npos || (closeBracket != std::string::npos && closeBracket < open)) break;
        size_t end = matchingObjectEnd(json, open);
        if (end == std::string::npos) return false;
        std::string obj = json.substr(open, end - open + 1);
        DeviceProgram program;
        size_t valueAt = 0;
        if (!findKey(obj, 0, "name", valueAt) || !readJsonString(obj, valueAt, program.name)) return false;
        if (findKey(obj, 0, "origin", valueAt)) readJsonString(obj, valueAt, program.origin);
        if (findKey(obj, 0, "bytes", valueAt)) {
            while (valueAt < obj.size() && std::isspace(static_cast<unsigned char>(obj[valueAt]))) valueAt++;
            program.bytes = std::atoi(obj.c_str() + valueAt);
        }
        out.programs.push_back(std::move(program));
        cursor = end + 1;
    }
    return true;
}

}
