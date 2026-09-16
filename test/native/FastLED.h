#ifndef LEDBASIC_NATIVE_FASTLED_H
#define LEDBASIC_NATIVE_FASTLED_H

#include <cstdint>

struct CHSV {
    uint8_t h, s, v;
    CHSV(uint8_t h_ = 0, uint8_t s_ = 0, uint8_t v_ = 0) : h(h_), s(s_), v(v_) {}
};

struct CRGB {
    uint8_t r, g, b;
    static CRGB Black;

    CRGB() : r(0), g(0), b(0) {}
    CRGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
    CRGB(int r_, int g_, int b_) : r((uint8_t)r_), g((uint8_t)g_), b((uint8_t)b_) {}
    CRGB(const CHSV& hsv) { hsv2rgb(hsv); }

    bool operator==(const CRGB& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const CRGB& o) const { return !(*this == o); }

    void nscale8(uint8_t scale) {
        r = (uint8_t)((r * scale) / 255);
        g = (uint8_t)((g * scale) / 255);
        b = (uint8_t)((b * scale) / 255);
    }

private:
    void hsv2rgb(const CHSV& hsv) {
        uint8_t region = hsv.h / 43;
        uint8_t remainder = (hsv.h - (region * 43)) * 6;
        uint8_t p = (hsv.v * (255 - hsv.s)) >> 8;
        uint8_t q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
        uint8_t t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;
        switch (region) {
            case 0: r = hsv.v; g = t; b = p; break;
            case 1: r = q; g = hsv.v; b = p; break;
            case 2: r = p; g = hsv.v; b = t; break;
            case 3: r = p; g = q; b = hsv.v; break;
            case 4: r = t; g = p; b = hsv.v; break;
            default: r = hsv.v; g = p; b = q; break;
        }
    }
};

inline CRGB CRGB::Black{0, 0, 0};

inline void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb) {
    rgb = CRGB(hsv);
}

inline CHSV rgb2hsv_approximate(const CRGB& rgb) {
    uint8_t mx = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);
    uint8_t mn = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
    CHSV out(0, 0, mx);
    if (mx == 0) return out;
    out.s = (uint8_t)((uint16_t)(mx - mn) * 255 / mx);
    return out;
}

struct CFastLED {
    int showCount = 0;
    uint8_t brightness = 255;
    void show() { showCount++; }
    void setBrightness(uint8_t b) { brightness = b; }
};

inline CFastLED FastLED;

#endif
