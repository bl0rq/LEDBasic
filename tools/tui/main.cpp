#include "HostSession.h"
#include "WledClient.h"
#include "editor.h"
#include "theme.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

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
namespace fs = std::filesystem;

#ifndef LEDBASIC_EXAMPLES_DIR
#define LEDBASIC_EXAMPLES_DIR "examples/bas"
#endif

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

static std::string examplesDir() {
    fs::path p(LEDBASIC_EXAMPLES_DIR);
    if (fs::is_directory(p)) return p.string();
    if (fs::is_directory("examples/bas")) return "examples/bas";
    if (fs::is_directory("../examples/bas")) return "../examples/bas";
    return p.string();
}

static std::string defaultProgramPath() {
    fs::path p = fs::path(examplesDir()) / "Rainbow.bas";
    return p.string();
}

static std::string deviceUrlPath() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata || !appdata[0]) return {};
    fs::path dir = fs::path(appdata) / "LEDBasic";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return (dir / "device.url").string();
#else
    return {};
#endif
}

static std::string loadDeviceUrl() {
    std::string path = deviceUrlPath();
    if (path.empty()) return {};
    std::ifstream in(path);
    if (!in) return {};
    std::string line;
    std::getline(in, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\n')) line.pop_back();
    return line;
}

static void saveDeviceUrl(const std::string& url) {
    std::string path = deviceUrlPath();
    if (path.empty()) return;
    std::ofstream out(path);
    if (out) out << url;
}

static std::string trimCopy(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

static const char* stateLabel(ledbasic::Snapshot::State s) {
    switch (s) {
        case ledbasic::Snapshot::Playing: return "PLAY";
        case ledbasic::Snapshot::Paused: return "PAUSE";
        default: return "STOP";
    }
}

static Element renderStripCells(const ledbasic::Snapshot& snap, int width) {
    if (width < 8) width = 8;
    int n = (int)snap.leds.size();
    Elements cells;
    cells.reserve((size_t)n);
    float scale = snap.brightness / 255.0f;
    for (int i = 0; i < n; i++) {
        int r = (int)(snap.leds[(size_t)i].r * scale);
        int g = (int)(snap.leds[(size_t)i].g * scale);
        int b = (int)(snap.leds[(size_t)i].b * scale);
        cells.push_back(text("█") | color(Color::RGB(r, g, b)));
    }
    Elements rows;
    for (int i = 0; i < (int)cells.size(); i += width) {
        Elements row;
        int end = std::min((int)cells.size(), i + width);
        for (int j = i; j < end; j++) row.push_back(cells[(size_t)j]);
        rows.push_back(hbox(std::move(row)));
    }
    if (rows.empty()) rows.push_back(text("(no LEDs)") | color(theme::dim()));
    return vbox(std::move(rows)) | bgcolor(theme::bg());
}

int main(int argc, char** argv) {
    enableUtf8Console();

    int leds = ledbasic::HostSession::kDefaultLeds;
    std::string startFile = defaultProgramPath();
    std::string deviceArg;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--leds" && i + 1 < argc) leds = std::atoi(argv[++i]);
        else if (a == "--device" && i + 1 < argc) deviceArg = argv[++i];
        else if (a == "--help" || a == "-h") {
            std::printf("ledbasic-tui [file.bas] [--leds N] [--device http://wled-ip]\n");
            return 0;
        } else if (a[0] != '-') {
            startFile = a;
        }
    }

    ledbasic::HostSession session(leds);
    auto editor = std::make_shared<SourceEditor>();

    std::string filePath = startFile;
    {
        std::ifstream in(filePath);
        if (in) {
            std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            editor->setText(body);
            session.loadFile(filePath);
            editor->clearDirty();
        } else {
            editor->setText(
                "setup\n"
                "  brightness(128)\n"
                "  clear()\n"
                "end\n"
                "\n"
                "loop(time)\n"
                "  fill(hsv(time / 20 % 360, 255, 255))\n"
                "  show()\n"
                "end\n");
            session.loadSource(editor->getText(), "");
            editor->clearDirty();
        }
    }

    editor->onBreakpointChanged = [&] {
        session.clearBreakpoints();
        for (int line : editor->breakpoints()) session.setBreakpoint(line, true);
    };

    auto screen = ScreenInteractive::Fullscreen();
    screen.TrackMouse(true);

    std::string immediate;
    std::vector<std::string> immediateLog;
    int paramIndex = 0;
    bool showHelp = false;
    bool showOpen = false;
    bool showSaveAs = false;
    bool showFileMenu = false;
    bool showViewMenu = false;
    bool showRunMenu = false;
    bool showDebugMenu = false;
    bool showDeviceMenu = false;
    bool showDeviceHost = false;
    bool showDevicePull = false;
    bool showDevicePush = false;
    bool showDiscard = false;
    std::string pathInput;
    std::string deviceHostInput;
    std::string devicePushInput;
    std::string deviceStatus = " device: off ";
    std::vector<std::string> deviceLabels{"  (refresh the device) "};
    std::vector<std::string> deviceNames;
    std::vector<std::string> deviceOrigins;
    int deviceIndex = 0;
    ledbasic::WledClient device;
    std::function<void()> afterDiscard;
    float speed = 1.0f;
    {
        std::string saved = deviceArg.empty() ? loadDeviceUrl() : trimCopy(deviceArg);
        if (!saved.empty()) {
            device.setBaseUrl(saved);
            deviceHostInput = saved;
            if (!deviceArg.empty()) saveDeviceUrl(saved);
            deviceStatus = " device: " + saved;
        }
    }

    auto pushLog = [&](const std::string& line) {
        immediateLog.push_back(line);
        if (immediateLog.size() > 8) immediateLog.erase(immediateLog.begin());
    };

    auto reloadFromEditor = [&]() -> bool {
        bool ok = session.loadSource(editor->getText(), filePath);
        session.clearBreakpoints();
        for (int line : editor->breakpoints()) session.setBreakpoint(line, true);
        if (!ok) {
            auto diags = session.diagnostics();
            if (!diags.empty()) {
                editor->setErrorLine(diags[0].line);
                pushLog("error " + std::to_string(diags[0].line) + ": " +
                        std::string(diags[0].message.c_str()));
            } else {
                pushLog("load failed");
            }
            return false;
        }
        editor->setErrorLine(0);
        return true;
    };

    auto saveTo = [&](const std::string& path) {
        std::ofstream out(path);
        if (!out) {
            pushLog("could not write " + path);
            return;
        }
        out << editor->getText();
        filePath = path;
        editor->clearDirty();
        pushLog("saved " + path);
    };

    auto doOpen = [&](const std::string& path) {
        session.stop();
        std::ifstream in(path);
        if (!in) {
            pushLog("could not open " + path);
            return;
        }
        std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        editor->setText(body);
        filePath = path;
        session.clearBreakpoints();
        editor->setBreakpoints({});
        if (!session.loadFile(path)) {
            auto diags = session.diagnostics();
            if (!diags.empty()) {
                editor->setErrorLine(diags[0].line);
                pushLog("error " + std::to_string(diags[0].line) + ": " +
                        std::string(diags[0].message.c_str()));
            }
        } else {
            editor->setErrorLine(0);
            pushLog("opened " + path);
        }
    };

    auto doNew = [&] {
        session.stop();
        editor->setText("setup\nend\n\nloop(time)\n  show()\nend\n");
        filePath.clear();
        session.loadSource(editor->getText(), "");
        editor->clearDirty();
        pushLog("new program");
    };

    auto doSave = [&] {
        if (filePath.empty()) {
            showSaveAs = true;
            pathInput = "untitled.bas";
        } else {
            saveTo(filePath);
        }
    };

    auto closeMenus = [&] {
        showFileMenu = showViewMenu = showRunMenu = showDebugMenu = showDeviceMenu = false;
    };

    auto selectedDeviceName = [&]() -> std::string {
        if (deviceIndex < 0 || deviceIndex >= (int)deviceNames.size()) return {};
        return deviceNames[(size_t)deviceIndex];
    };

    auto refreshDevice = [&]() -> std::string {
        if (!device.ready()) {
            deviceStatus = " device: set a host ";
            return {};
        }
        std::string keep = selectedDeviceName();
        ledbasic::DeviceCatalog catalog;
        std::string err;
        if (!device.listPrograms(catalog, err)) {
            deviceStatus = " device: " + err;
            return {};
        }
        deviceLabels.clear();
        deviceNames.clear();
        deviceOrigins.clear();
        for (const auto& program : catalog.programs) {
            deviceNames.push_back(program.name);
            deviceOrigins.push_back(program.origin);
            deviceLabels.push_back(std::string("  ") + program.name +
                                    (program.origin == "user" ? "  " : "  (built-in) "));
        }
        if (deviceLabels.empty()) deviceLabels.push_back("  (none) ");
        deviceIndex = 0;
        for (int i = 0; i < (int)deviceNames.size(); i++) {
            if (deviceNames[(size_t)i] == keep) deviceIndex = i;
        }
        deviceStatus = " device: " + catalog.active + " @ " + device.baseUrl();
        return catalog.active;
    };

    auto pullDevice = [&]() {
        std::string name = selectedDeviceName();
        if (name.empty()) {
            deviceStatus = " device: refresh, then pick a script ";
            return;
        }
        auto go = [&, name]() {
            std::string src;
            std::string err;
            if (!device.getSource(name, src, err)) {
                deviceStatus = " device: " + err;
                return;
            }
            session.stop();
            editor->setText(src);
            filePath.clear();
            session.clearBreakpoints();
            editor->setBreakpoints({});
            if (!session.loadSource(src, name)) {
                auto diags = session.diagnostics();
                if (!diags.empty()) editor->setErrorLine(diags[0].line);
                deviceStatus = " device: pulled " + name + " (load error)";
            } else {
                editor->setErrorLine(0);
                deviceStatus = " device: pulled " + name;
            }
            editor->clearDirty();
        };
        if (editor->dirty()) {
            afterDiscard = go;
            showDiscard = true;
            return;
        }
        go();
    };

    auto pushDevice = [&]() {
        std::string name = trimCopy(devicePushInput);
        if (name.empty()) {
            deviceStatus = " device: enter a script name ";
            return;
        }
        bool builtin = false;
        for (size_t i = 0; i < deviceNames.size(); i++) {
            if (deviceNames[i] == name && (i >= deviceOrigins.size() || deviceOrigins[i] != "user")) builtin = true;
        }
        if (builtin) {
            deviceStatus = " device: built-ins stay read-only; choose a new name ";
            return;
        }
        std::string err;
        if (!device.putSource(name, editor->getText(), err)) {
            deviceStatus = " device: " + err;
            return;
        }
        showDevicePush = false;
        refreshDevice();
        deviceStatus = " device: saved " + name;
    };

    auto activateDevice = [&]() {
        std::string name = selectedDeviceName();
        if (name.empty()) {
            deviceStatus = " device: refresh, then pick a script ";
            return;
        }
        std::string err;
        if (!device.activate(name, err)) {
            deviceStatus = " device: " + err;
            return;
        }
        std::string active = refreshDevice();
        if (active == name) deviceStatus = " device: running " + name;
        else if (!active.empty()) deviceStatus = " device: could not run " + name;
    };

    auto toggleMenu = [&](bool* which) {
        bool next = !*which;
        closeMenus();
        *which = next;
    };

    InputOption immOpt;
    immOpt.on_enter = [&] {
        std::string cmd = immediate;
        immediate.clear();
        if (cmd.empty()) return;
        std::string upper = cmd;
        for (char& c : upper) if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (upper == "RUN") {
            if (reloadFromEditor()) session.run();
            return;
        }
        if (upper == "STOP") {
            session.stop();
            return;
        }
        auto r = session.evalImmediate(cmd);
        pushLog("? " + cmd);
        pushLog(r.ok ? r.text : r.text);
    };
    auto immediateInput = Input(&immediate, "Immediate  (? expr  |  name = expr  |  RUN)", immOpt);

    auto applySpeed = [&](float s) {
        speed = s;
        session.setSpeedMultiplier(s);
    };

    class FocusPad : public ComponentBase {
    public:
        bool Focusable() const override { return true; }
        Element OnRender() override { return text(" "); }
    };
    auto paramsFocus = Make<FocusPad>();

    auto btnOpt = ButtonOption::Simple();
    auto menuBtn = [&](const char* label, std::function<void()> fn) {
        return Button(label, std::move(fn), btnOpt);
    };

    auto fileBtn = menuBtn(" File ", [&] { toggleMenu(&showFileMenu); });
    auto viewBtn = menuBtn(" View ", [&] { toggleMenu(&showViewMenu); });
    auto runBtn = menuBtn(" Run ", [&] { toggleMenu(&showRunMenu); });
    auto debugBtn = menuBtn(" Debug ", [&] { toggleMenu(&showDebugMenu); });
    auto deviceBtn = menuBtn(" Device ", [&] { toggleMenu(&showDeviceMenu); });
    auto helpBtn = menuBtn(" Help ", [&] {
        closeMenus();
        showHelp = true;
    });
    auto menuBar = Container::Horizontal({fileBtn, viewBtn, runBtn, debugBtn, deviceBtn, helpBtn});

    auto wrapMenu = [](const char* title, Component inner) {
        inner |= Renderer([title](Element e) {
            return vbox(Elements{
                       text(std::string(" ") + title) | bold | color(theme::accent()),
                       separator(),
                       e,
                   }) |
                   border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 34);
        });
        return inner;
    };

    auto fileMenu = wrapMenu("File", Container::Vertical({
        menuBtn(" New              Ctrl+N ", [&] { closeMenus(); doNew(); }),
        menuBtn(" Open…            Ctrl+O ", [&] {
            closeMenus();
            showOpen = true;
            pathInput = examplesDir() + "/";
        }),
        menuBtn(" Save             Ctrl+S ", [&] { closeMenus(); doSave(); }),
        menuBtn(" Save As…                ", [&] {
            closeMenus();
            showSaveAs = true;
            pathInput = filePath.empty() ? "untitled.bas" : filePath;
        }),
        menuBtn(" Exit             Ctrl+Q ", [&] {
            closeMenus();
            screen.Exit();
        }),
    }));

    auto viewMenu = wrapMenu("View", Container::Vertical({
        menuBtn(" Speed 0.25x   (1) ", [&] { closeMenus(); applySpeed(0.25f); }),
        menuBtn(" Speed 1x      (2) ", [&] { closeMenus(); applySpeed(1.0f); }),
        menuBtn(" Speed 2x      (3) ", [&] { closeMenus(); applySpeed(2.0f); }),
        menuBtn(" Speed 4x      (4) ", [&] { closeMenus(); applySpeed(4.0f); }),
        menuBtn(" 30 LEDs           ", [&] {
            closeMenus();
            session.setLedCount(30);
            reloadFromEditor();
        }),
        menuBtn(" 60 LEDs           ", [&] {
            closeMenus();
            session.setLedCount(60);
            reloadFromEditor();
        }),
        menuBtn(" 120 LEDs          ", [&] {
            closeMenus();
            session.setLedCount(120);
            reloadFromEditor();
        }),
    }));

    auto runMenu = wrapMenu("Run", Container::Vertical({
        menuBtn(" Start / Continue    F5  ", [&] {
            closeMenus();
            if (reloadFromEditor()) session.run();
        }),
        menuBtn(" Stop                Esc ", [&] {
            closeMenus();
            session.stop();
        }),
    }));

    auto debugMenu = wrapMenu("Debug", Container::Vertical({
        menuBtn(" Step Into           F8  ", [&] {
            closeMenus();
            if (session.snapshot().state == ledbasic::Snapshot::Stopped) {
                if (!reloadFromEditor()) return;
            }
            session.stepInto();
        }),
        menuBtn(" Step Over           F10 ", [&] {
            closeMenus();
            if (session.snapshot().state == ledbasic::Snapshot::Stopped) {
                if (!reloadFromEditor()) return;
            }
            session.stepOver();
        }),
        menuBtn(" Toggle Breakpoint   F9  ", [&] {
            closeMenus();
            editor->toggleBreakpointAtCursor();
        }),
    }));

    auto deviceMenu = wrapMenu("Device", Container::Vertical({
        menuBtn(" Set host…              ", [&] {
            closeMenus();
            deviceHostInput = device.baseUrl();
            showDeviceHost = true;
        }),
        menuBtn(" Refresh                ", [&] {
            closeMenus();
            refreshDevice();
        }),
        menuBtn(" Pull…                  ", [&] {
            closeMenus();
            showDevicePull = true;
        }),
        menuBtn(" Push…                  ", [&] {
            closeMenus();
            std::string seed = selectedDeviceName();
            if (seed.empty()) seed = "MyEffect";
            else if (deviceIndex >= 0 && deviceIndex < (int)deviceOrigins.size() &&
                     deviceOrigins[(size_t)deviceIndex] != "user") {
                seed += "Copy";
            }
            devicePushInput = seed;
            showDevicePush = true;
        }),
        menuBtn(" Activate               ", [&] {
            closeMenus();
            activateDevice();
        }),
        Renderer([&] { return text(deviceStatus) | color(theme::dim()); }),
    }));

    auto openPathField = Input(&pathInput, "path to .bas file");
    auto savePathField = Input(&pathInput, "path to .bas file");
    std::vector<std::string> exampleLabels;
    std::vector<std::string> examplePaths;
    {
        std::error_code ec;
        std::vector<fs::path> examples;
        for (auto& p : fs::directory_iterator(examplesDir(), ec)) {
            if (p.path().extension() == ".bas") examples.push_back(p.path());
        }
        std::sort(examples.begin(), examples.end());
        for (const auto& p : examples) {
            examplePaths.push_back(p.string());
            exampleLabels.push_back("  " + p.filename().string() + "  ");
        }
    }
    Components openKids;
    openKids.push_back(Renderer([] { return text(" Open") | bold | color(theme::accent()); }));
    openKids.push_back(Renderer([] { return separator(); }));
    for (size_t i = 0; i < exampleLabels.size(); i++) {
        const std::string& path = examplePaths[i];
        openKids.push_back(Button(&exampleLabels[i], [&, path] {
            doOpen(path);
            showOpen = false;
        }, btnOpt));
    }
    openKids.push_back(Renderer([] { return separator(); }));
    openKids.push_back(openPathField);
    openKids.push_back(Container::Horizontal({
        menuBtn(" Open ", [&] {
            doOpen(pathInput);
            showOpen = false;
        }),
        menuBtn(" Cancel ", [&] { showOpen = false; }),
    }));
    auto openDialog = Container::Vertical(std::move(openKids));
    openDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 52);
    });

    auto saveDialog = Container::Vertical({
        Renderer([] { return text(" Save as") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        savePathField,
        Container::Horizontal({
            menuBtn(" Save ", [&] {
                saveTo(pathInput);
                showSaveAs = false;
            }),
            menuBtn(" Cancel ", [&] { showSaveAs = false; }),
        }),
    });
    saveDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 52);
    });

    auto helpDialog = Container::Vertical({
        Renderer([] {
            return vbox(Elements{
                text("LEDBasic TUI") | bold | color(theme::accent()) | center,
                separator(),
                text(" Click File / View / Run / Debug / Device / Help, or use keys:"),
                text(" Device pushes and pulls scripts on a WLED board"),
                text(" F5 Run/Continue   Esc Stop   F8 Step   F10 Step Over"),
                text(" F9 Toggle breakpoint on the current line"),
                text(" F4 Immediate    Ctrl+O Open   Ctrl+S Save   Ctrl+N New"),
                text(" Click a line to move the caret; click the gutter for a breakpoint"),
                text(" Immediate: ? x   x = 3   RUN   STOP"),
            });
        }),
        menuBtn(" Close ", [&] { showHelp = false; }),
    });
    helpDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 70) | center;
    });

    auto deviceHostField = Input(&deviceHostInput, "http://192.168.1.20");
    auto deviceHostDialog = Container::Vertical({
        Renderer([] { return text(" WLED host") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        deviceHostField,
        Container::Horizontal({
            menuBtn(" Save ", [&] {
                std::string host = trimCopy(deviceHostInput);
                if (host.empty()) {
                    deviceStatus = " device: set a host ";
                    return;
                }
                device.setBaseUrl(host);
                saveDeviceUrl(host);
                showDeviceHost = false;
                refreshDevice();
            }),
            menuBtn(" Cancel ", [&] { showDeviceHost = false; }),
        }),
    });
    deviceHostDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 52);
    });

    auto deviceList = Menu(&deviceLabels, &deviceIndex) | vscroll_indicator | size(HEIGHT, LESS_THAN, 12);
    auto devicePullDialog = Container::Vertical({
        Renderer([] { return text(" Pull from device") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        deviceList,
        Container::Horizontal({
            menuBtn(" Pull ", [&] {
                showDevicePull = false;
                pullDevice();
            }),
            menuBtn(" Cancel ", [&] { showDevicePull = false; }),
        }),
    });
    devicePullDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 42);
    });

    auto devicePushField = Input(&devicePushInput, "script name");
    auto devicePushDialog = Container::Vertical({
        Renderer([] { return text(" Push to device") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        Renderer([] { return text(" Built-in names need a new name") | color(theme::dim()); }),
        devicePushField,
        Container::Horizontal({
            menuBtn(" Push ", [&] { pushDevice(); }),
            menuBtn(" Cancel ", [&] { showDevicePush = false; }),
        }),
    });
    devicePushDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 52);
    });

    auto discardDialog = Container::Vertical({
        Renderer([] { return text(" Discard unsaved edits?") | bold | color(theme::accent()); }),
        Container::Horizontal({
            menuBtn(" Discard ", [&] {
                showDiscard = false;
                auto next = std::move(afterDiscard);
                afterDiscard = nullptr;
                if (next) next();
            }),
            menuBtn(" Cancel ", [&] {
                showDiscard = false;
                afterDiscard = nullptr;
            }),
        }),
    });
    discardDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 40);
    });

    auto workspace = Container::Vertical({
        menuBar,
        editor,
        paramsFocus,
        immediateInput,
    });

    auto renderer = Renderer(workspace, [&] {
        auto snap = session.snapshot();
        editor->setReadOnly(snap.state == ledbasic::Snapshot::Playing);
        editor->setCurrentLine(snap.state == ledbasic::Snapshot::Paused ? snap.currentLine : 0);
        if (snap.runtimeError) editor->setErrorLine(snap.currentLine);

        char title[256];
        std::snprintf(title, sizeof(title), " %s%s ",
                      filePath.empty() ? "untitled.bas" : filePath.c_str(),
                      editor->dirty() ? " *" : "");

        Element menuRow = hbox(Elements{
            menuBar->Render(),
            filler(),
            text(" LEDBasic ") | color(theme::accent()) | bold,
        });

        char stripHead[160];
        std::snprintf(stripHead, sizeof(stripHead),
                      " %d LEDs   brightness %u   t=%lums   %.2fx   %s ",
                      (int)snap.leds.size(), (unsigned)snap.brightness, snap.timeMs,
                      speed, stateLabel(snap.state));
        Element strip = vbox(Elements{
            text(stripHead) | color(theme::accent()),
            renderStripCells(snap, 80),
        });

        Elements watchLines;
        watchLines.push_back(text(" Watch") | bold | color(theme::accent()));
        if (snap.variables.empty()) {
            watchLines.push_back(text("  (run to populate)") | color(theme::dim()));
        } else {
            int shown = 0;
            for (const auto& v : snap.variables) {
                if (v.name == "PI" || v.name == "E") continue;
                watchLines.push_back(text("  " + v.name + " = " + v.display) | color(theme::fg()));
                if (++shown >= 12) break;
            }
        }
        watchLines.push_back(separator());
        watchLines.push_back(
            text(paramsFocus->Focused() ? " Params  (Tab, Left/Right)" : " Params") |
            bold | color(theme::accent()));
        if (snap.parameters.empty()) {
            watchLines.push_back(text("  (none)") | color(theme::dim()));
        } else {
            if (paramIndex >= (int)snap.parameters.size()) paramIndex = (int)snap.parameters.size() - 1;
            if (paramIndex < 0) paramIndex = 0;
            for (int i = 0; i < (int)snap.parameters.size(); i++) {
                const auto& p = snap.parameters[(size_t)i];
                char buf[128];
                std::snprintf(buf, sizeof(buf), " %s %s = %.4g",
                              (i == paramIndex ? ">" : " "), p.name.c_str(), p.value);
                auto line = text(buf);
                if (i == paramIndex) line = line | inverted;
                watchLines.push_back(line);
            }
            watchLines.push_back(text("  Left/Right adjust") | color(theme::dim()));
        }

        Elements immLines;
        for (const auto& l : immediateLog) {
            immLines.push_back(text(l) | color(theme::dim()));
        }
        if (immLines.empty()) {
            immLines.push_back(text("Type an expression, assignment, RUN, or STOP") | color(theme::dim()));
        }

        Element editorPane = vbox(Elements{
            text(title) | color(theme::dim()),
            separator(),
            editor->Render() | flex,
        }) | border;

        Element side = vbox(std::move(watchLines)) | border | size(WIDTH, EQUAL, 28);
        Element imm = vbox(Elements{
            text(" Immediate") | color(theme::accent()),
            vbox(std::move(immLines)),
            immediateInput->Render(),
        }) | border | size(HEIGHT, EQUAL, 8);

        Element body = vbox(Elements{
            menuRow | bgcolor(theme::chrome()),
            (strip | border),
            hbox(Elements{editorPane | flex, side}) | flex,
            imm,
            hbox(Elements{
                text(" F1 Help ") | color(theme::dim()),
                text(" F4 Immediate ") | color(theme::dim()),
                text(" F5 Run ") | color(theme::accent()),
                text(" F8 Step ") | color(theme::dim()),
                text(" F9 Break ") | color(theme::dim()),
                text(" F10 Over ") | color(theme::dim()),
                text(" Esc Stop ") | color(theme::dim()),
                filler(),
                text(deviceStatus) | color(theme::dim()),
                text(editor->readOnly() ? " running " : " editing ") | color(theme::dim()),
            }) | bgcolor(theme::chrome()),
        }) | bgcolor(theme::bg());

        return body;
    });

    renderer |= Modal(fileMenu, &showFileMenu);
    renderer |= Modal(viewMenu, &showViewMenu);
    renderer |= Modal(runMenu, &showRunMenu);
    renderer |= Modal(debugMenu, &showDebugMenu);
    renderer |= Modal(deviceMenu, &showDeviceMenu);
    renderer |= Modal(openDialog, &showOpen);
    renderer |= Modal(saveDialog, &showSaveAs);
    renderer |= Modal(helpDialog, &showHelp);
    renderer |= Modal(deviceHostDialog, &showDeviceHost);
    renderer |= Modal(devicePullDialog, &showDevicePull);
    renderer |= Modal(devicePushDialog, &showDevicePush);
    renderer |= Modal(discardDialog, &showDiscard);

    renderer |= CatchEvent([&](Event e) {
        if (e == Event::F1) {
            closeMenus();
            showHelp = true;
            return true;
        }
        if (e == Event::F4) {
            immediateInput->TakeFocus();
            return true;
        }
        if (e == Event::F5) {
            if (reloadFromEditor()) session.run();
            return true;
        }
        if (e == Event::Escape) {
            if (showHelp || showOpen || showSaveAs || showDeviceHost || showDevicePull ||
                showDevicePush || showDiscard || showFileMenu || showViewMenu || showRunMenu ||
                showDebugMenu || showDeviceMenu) {
                showHelp = showOpen = showSaveAs = false;
                showDeviceHost = showDevicePull = showDevicePush = showDiscard = false;
                afterDiscard = nullptr;
                closeMenus();
                return true;
            }
            session.stop();
            return true;
        }
        if (e == Event::F8) {
            if (session.snapshot().state == ledbasic::Snapshot::Stopped) {
                if (!reloadFromEditor()) return true;
            }
            session.stepInto();
            return true;
        }
        if (e == Event::F10) {
            if (session.snapshot().state == ledbasic::Snapshot::Stopped) {
                if (!reloadFromEditor()) return true;
            }
            session.stepOver();
            return true;
        }
        if (e == Event::F9) {
            editor->toggleBreakpointAtCursor();
            return true;
        }
        if (e == Event::CtrlQ) {
            screen.Exit();
            return true;
        }
        if (e == Event::CtrlS) {
            doSave();
            return true;
        }
        if (e == Event::CtrlO) {
            closeMenus();
            showOpen = true;
            pathInput = examplesDir() + "/";
            return true;
        }
        if (e == Event::CtrlN) {
            doNew();
            return true;
        }

        if (e.is_character() && !editor->Focused() && !immediateInput->Focused()) {
            std::string ch = e.character();
            if (ch == "1") { applySpeed(0.25f); return true; }
            if (ch == "2") { applySpeed(1.0f); return true; }
            if (ch == "3") { applySpeed(2.0f); return true; }
            if (ch == "4") { applySpeed(4.0f); return true; }
        }

        auto snap = session.snapshot();
        if (paramsFocus->Focused() && !snap.parameters.empty()) {
            if (e == Event::ArrowUp) {
                if (paramIndex > 0) paramIndex--;
                return true;
            }
            if (e == Event::ArrowDown) {
                if (paramIndex + 1 < (int)snap.parameters.size()) paramIndex++;
                return true;
            }
            if (e == Event::ArrowLeft || e == Event::ArrowRight) {
                const auto& p = snap.parameters[(size_t)paramIndex];
                float step = p.stepValue != 0 ? p.stepValue : 1.f;
                float v = p.value + (e == Event::ArrowRight ? step : -step);
                if (v < p.minValue) v = p.minValue;
                if (v > p.maxValue) v = p.maxValue;
                session.setParameter(p.name, v);
                return true;
            }
        }

        if (!editor->Focused() && !immediateInput->Focused() && e.is_character() &&
            e.character() == "[") {
            int n = session.ledCount();
            if (n > 10) session.setLedCount(n / 2);
            if (editor->dirty() || !filePath.empty()) reloadFromEditor();
            return true;
        }
        if (!editor->Focused() && !immediateInput->Focused() && e.is_character() &&
            e.character() == "]") {
            int n = session.ledCount();
            session.setLedCount(std::min(n * 2, ledbasic::HostSession::kMaxLeds));
            if (editor->dirty() || !filePath.empty()) reloadFromEditor();
            return true;
        }

        return false;
    });

    std::atomic<bool> alive{true};
    std::thread refresh([&] {
        while (alive) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            screen.Post(Event::Custom);
        }
    });

    screen.Loop(renderer);
    alive = false;
    refresh.join();
    session.stop();
    return 0;
}
