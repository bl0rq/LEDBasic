#ifndef LEDBASIC_TUI_TEST_H
#define LEDBASIC_TUI_TEST_H

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>

extern int gFails;
extern int gPass;

#define CHECK(cond, msg) do { \
    if (cond) { gPass++; } \
    else { gFails++; std::printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

// Renders a component into an in-memory screen and returns plain text,
// one line per row, no ANSI codes. Renders twice so components that size
// themselves from reflect() boxes (SourceEditor) see their real height.
inline std::string renderText(ftxui::ComponentBase& c, int width, int height) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                        ftxui::Dimension::Fixed(height));
    for (int pass = 0; pass < 2; pass++) {
        screen.Clear();
        ftxui::Render(screen, c.Render());
    }
    std::string out;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const std::string& ch = screen.PixelAt(x, y).character;
            out += ch.empty() ? " " : ch;
        }
        out += '\n';
    }
    return out;
}

inline bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

inline void typeText(ftxui::ComponentBase& c, const std::string& s) {
    for (char ch : s) c.OnEvent(ftxui::Event::Character(ch));
}

inline ftxui::Event click(int x, int y) {
    ftxui::Mouse m;
    m.button = ftxui::Mouse::Left;
    m.motion = ftxui::Mouse::Pressed;
    m.x = x;
    m.y = y;
    return ftxui::Event::Mouse("", m);
}

// Polls pred until it holds or timeoutMs passes. The simulator runs on its
// own worker thread, so state changes after F5/F8 are not synchronous.
inline bool waitFor(const std::function<bool()>& pred, int timeoutMs = 2000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

#endif
