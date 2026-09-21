#ifndef LEDBASIC_TUI_THEME_H
#define LEDBASIC_TUI_THEME_H

#include <ftxui/screen/color.hpp>

namespace theme {
using ftxui::Color;

inline Color bg() { return Color::RGB(18, 18, 22); }
inline Color fg() { return Color::RGB(220, 222, 228); }
inline Color dim() { return Color::RGB(110, 114, 128); }
inline Color chrome() { return Color::RGB(32, 34, 42); }
inline Color keyword() { return Color::RGB(122, 178, 255); }
inline Color func() { return Color::RGB(232, 208, 118); }
inline Color number() { return Color::RGB(138, 214, 158); }
inline Color string() { return Color::RGB(236, 178, 118); }
inline Color comment() { return Color::RGB(98, 108, 120); }
inline Color current() { return Color::RGB(36, 58, 88); }
inline Color error() { return Color::RGB(236, 92, 92); }
inline Color accent() { return Color::RGB(90, 200, 180); }
inline Color menuSel() { return Color::RGB(48, 72, 112); }
}

#endif
