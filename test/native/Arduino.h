#ifndef LEDBASIC_NATIVE_ARDUINO_H
#define LEDBASIC_NATIVE_ARDUINO_H

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

typedef uint8_t byte;

template <typename T>
T constrain(T x, T a, T b) {
    if (x < a) return a;
    if (x > b) return b;
    return x;
}

template <typename T>
T min(T a, T b) { return a < b ? a : b; }

template <typename T>
T max(T a, T b) { return a > b ? a : b; }

inline unsigned long millis() {
    extern unsigned long ledbasic_mock_millis;
    return ledbasic_mock_millis;
}

inline void delay(int) {}

inline long random(long min, long max) {
    if (max <= min) return min;
    return min + (std::rand() % (max - min));
}

class String {
    std::string s;
public:
    String() = default;
    String(const char* v) : s(v ? v : "") {}
    String(const std::string& v) : s(v) {}
    String(char c) : s(1, c) {}
    // One integral overload so size_t/long/unsigned long are not ambiguous
    // on both LP64 (Linux) and LLP64 (Windows).
    template <typename T, typename std::enable_if<
        std::is_integral<T>::value &&
        !std::is_same<T, bool>::value &&
        !std::is_same<T, char>::value, int>::type = 0>
    String(T v) : s(std::to_string(v)) {}
    String(float v) : s(std::to_string(v)) {}
    String(double v) : s(std::to_string(v)) {}

    size_t length() const { return s.size(); }
    const char* c_str() const { return s.c_str(); }
    bool empty() const { return s.empty(); }

    char operator[](size_t i) const { return i < s.size() ? s[i] : 0; }
    char& operator[](size_t i) {
        if (i >= s.size()) s.resize(i + 1);
        return s[i];
    }

    String substring(size_t start, size_t end) const {
        if (start >= s.size()) return String("");
        if (end > s.size()) end = s.size();
        if (end < start) end = start;
        return String(s.substr(start, end - start));
    }
    String substring(size_t start) const { return substring(start, s.size()); }

    float toFloat() const { return s.empty() ? 0.0f : std::strtof(s.c_str(), nullptr); }
    double toDouble() const { return s.empty() ? 0.0 : std::strtod(s.c_str(), nullptr); }
    int toInt() const { return s.empty() ? 0 : std::atoi(s.c_str()); }

    void trim() {
        size_t a = 0;
        while (a < s.size() && std::isspace((unsigned char)s[a])) a++;
        size_t b = s.size();
        while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
        s = s.substr(a, b - a);
    }

    bool equalsIgnoreCase(const String& other) const {
        if (s.size() != other.s.size()) return false;
        for (size_t i = 0; i < s.size(); i++) {
            if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)other.s[i])) return false;
        }
        return true;
    }

    int indexOf(char c) const {
        auto p = s.find(c);
        return p == std::string::npos ? -1 : (int)p;
    }
    int indexOf(const String& sub) const {
        auto p = s.find(sub.s);
        return p == std::string::npos ? -1 : (int)p;
    }

    String& operator=(const char* v) { s = v ? v : ""; return *this; }
    String& operator+=(char c) { s.push_back(c); return *this; }
    String& operator+=(const char* v) { if (v) s += v; return *this; }
    String& operator+=(const String& v) { s += v.s; return *this; }

    friend String operator+(const String& a, const String& b) { return String(a.s + b.s); }
    friend String operator+(const String& a, const char* b) { return String(a.s + (b ? b : "")); }
    friend bool operator==(const String& a, const String& b) { return a.s == b.s; }
    friend bool operator==(const String& a, const char* b) { return a.s == (b ? b : ""); }
    friend bool operator!=(const String& a, const String& b) { return !(a == b); }
    friend bool operator<(const String& a, const String& b) { return a.s < b.s; }
};

class NativeSerial {
public:
    void print(const String&) {}
    void print(const char*) {}
    void print(int) {}
    void print(float) {}
    void println(const String&) {}
    void println(const char*) {}
    void println(int) {}
    void println() {}
    void printf(const char*, ...) {}
    int available() { return 0; }
    int read() { return -1; }
};

static NativeSerial Serial;

#endif
