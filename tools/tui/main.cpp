#include "app.h"

#include <ftxui/component/screen_interactive.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef RGB
#undef RGB
#endif
#endif

using namespace ftxui;

static void enableUtf8Console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(h, &mode)) {
            mode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
            SetConsoleMode(h, mode);
        }
    }
#endif
}

int main(int argc, char** argv) {
    enableUtf8Console();

    AppOptions options;
    options.startFile = defaultProgramPath();
    bool hasDeviceArg = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--leds" && i + 1 < argc) options.leds = std::atoi(argv[++i]);
        else if (a == "--device" && i + 1 < argc) {
            options.deviceUrl = argv[++i];
            hasDeviceArg = true;
        } else if (a == "--help" || a == "-h") {
            std::printf("ledbasic-tui [file.bas] [--leds N] [--device URL]\n");
            return 0;
        } else if (a[0] != '-') {
            options.startFile = a;
        }
    }
    if (!hasDeviceArg) {
        std::string saved;
        if (ledbasic::loadDeviceUrl(saved)) options.deviceUrl = saved;
    }

    App app(options);
    auto screen = ScreenInteractive::Fullscreen();
    screen.TrackMouse(true);
    app.onExit = screen.ExitLoopClosure();

    std::atomic<bool> alive{true};
    std::thread refresh([&] {
        while (alive) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            screen.Post(Event::Custom);
        }
    });

    app.start();
    screen.Loop(app.root());
    alive = false;
    refresh.join();
    return 0;
}
