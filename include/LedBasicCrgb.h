#ifndef LEDBASIC_CRGB_H
#define LEDBASIC_CRGB_H

#include <stdint.h>

// Minimal CRGB/CHSV used when FastLED is not linked (WLED, host compile checks).
// Layout matches FastLED/WLED: three uint8_t channels in RGB order.

struct CHSV {
    uint8_t h, s, v;
    CHSV() : h(0), s(0), v(0) {}
    CHSV(uint8_t h_, uint8_t s_, uint8_t v_) : h(h_), s(s_), v(v_) {}
};

struct CRGB {
    uint8_t r, g, b;

    enum {
        Black = 0x000000
    };

    CRGB() : r(0), g(0), b(0) {}
    CRGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
    CRGB(int r_, int g_, int b_) : r((uint8_t)r_), g((uint8_t)g_), b((uint8_t)b_) {}
    CRGB(uint32_t colorcode)
        : r((uint8_t)(colorcode >> 16)),
          g((uint8_t)(colorcode >> 8)),
          b((uint8_t)colorcode) {}
    CRGB(const CHSV& hsv) { hsv2rgb(hsv); }

    bool operator==(const CRGB& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const CRGB& o) const { return !(*this == o); }

    CRGB& operator=(uint32_t colorcode) {
        r = (uint8_t)(colorcode >> 16);
        g = (uint8_t)(colorcode >> 8);
        b = (uint8_t)colorcode;
        return *this;
    }

    void nscale8(uint8_t scale) {
        r = (uint8_t)((r * scale) / 255);
        g = (uint8_t)((g * scale) / 255);
        b = (uint8_t)((b * scale) / 255);
    }

private:
    void hsv2rgb(const CHSV& hsv) {
        uint8_t region = hsv.h / 43;
        uint8_t remainder = (uint8_t)((hsv.h - (region * 43)) * 6);
        uint8_t p = (uint8_t)((hsv.v * (255 - hsv.s)) >> 8);
        uint8_t q = (uint8_t)((hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8);
        uint8_t t = (uint8_t)((hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8);
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

inline void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb) {
    rgb = CRGB(hsv);
}

inline CHSV rgb2hsv_approximate(const CRGB& rgb) {
    uint8_t mx = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);
    uint8_t mn = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
    CHSV out(0, 0, mx);
    if (mx == 0) return out;
    out.s = (uint8_t)((uint16_t)(mx - mn) * 255 / mx);
    if (mx == rgb.r) {
        out.h = (uint8_t)((rgb.g - rgb.b) * 43 / (mx - mn) + (rgb.g < rgb.b ? 255 : 0));
    } else if (mx == rgb.g) {
        out.h = (uint8_t)((rgb.b - rgb.r) * 43 / (mx - mn) + 85);
    } else {
        out.h = (uint8_t)((rgb.r - rgb.g) * 43 / (mx - mn) + 171);
    }
    return out;
}

#endif
