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
#include <vector>

extern int gFails;
extern int gPass;

#define CHECK(cond, msg) do { \
    if (cond) { gPass++; } \
    else { gFails++; std::printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

// One glyph per terminal cell, so x/y index straight into rows.
using ScreenCells = std::vector<std::vector<std::string>>;

// Renders a component into an in-memory screen. Renders twice so components
// that size themselves from reflect() boxes (SourceEditor) see their real
// height, and so mouse hit boxes match what is drawn.
inline ScreenCells renderCells(ftxui::ComponentBase& c, int width, int height) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                        ftxui::Dimension::Fixed(height));
    for (int pass = 0; pass < 2; pass++) {
        screen.Clear();
        ftxui::Render(screen, c.Render());
    }
    ScreenCells rows((size_t)height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const std::string& ch = screen.PixelAt(x, y).character;
            rows[(size_t)y].push_back(ch.empty() ? " " : ch);
        }
    }
    return rows;
}

// Plain text, one line per row, no ANSI codes.
inline std::string renderText(ftxui::ComponentBase& c, int width, int height) {
    std::string out;
    for (const auto& row : renderCells(c, width, height)) {
        for (const auto& ch : row) out += ch;
        out += '\n';
    }
    return out;
}

// Finds ASCII text on screen; the first match, or the last when last is set.
inline bool findText(const ScreenCells& rows, const std::string& needle, int* outX, int* outY,
                     bool last = false) {
    bool found = false;
    for (size_t y = 0; y < rows.size(); y++) {
        const auto& row = rows[y];
        for (size_t x = 0; x + needle.size() <= row.size(); x++) {
            size_t i = 0;
            while (i < needle.size() && row[x + i] == std::string(1, needle[i])) i++;
            if (i < needle.size()) continue;
            *outX = (int)x;
            *outY = (int)y;
            found = true;
            if (!last) return true;
        }
    }
    return found;
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
