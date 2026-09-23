#include "DeviceClient.h"
#include "HostSession.h"
#include "editor.h"
#include "theme.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <mutex>
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
    bool hasDeviceArg = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--leds" && i + 1 < argc) leds = std::atoi(argv[++i]);
        else if (a == "--device" && i + 1 < argc) {
            deviceArg = argv[++i];
            hasDeviceArg = true;
        } else if (a == "--help" || a == "-h") {
            std::printf("ledbasic-tui [file.bas] [--leds N] [--device URL]\n");
            return 0;
        } else if (a[0] != '-') {
            startFile = a;
        }
    }

    ledbasic::DeviceClient device;
    if (hasDeviceArg) device.setBaseUrl(deviceArg);
    else {
        std::string saved;
        if (ledbasic::loadDeviceUrl(saved)) device.setBaseUrl(saved);
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
    bool showDeviceConnect = false;
    bool showDeviceName = false;
    bool showDevicePrograms = false;
    bool showDeviceLeds = false;
    std::string pathInput;
    std::string deviceUrlInput = device.baseUrl;
    std::string deviceNameInput;
    std::string ledTypeInput = "ws2812";
    std::string ledOrderInput = "GRB";
    std::string ledLengthInput = "60";
    std::string ledPinInput = "16";
    std::string ledBrightInput = "128";
    std::string deviceLine = device.baseUrl.empty() ? "device off" : device.baseUrl;
    std::vector<ledbasic::DeviceProgram> devicePrograms;
    int deviceIndex = 0;
    ledbasic::DeviceLed loadedLed;
    bool hasLoadedLed = false;
    std::mutex deviceMu;
    std::vector<std::string> deviceLogs;
    bool deviceBusy = false;
    bool deviceProgramsReady = false;
    std::vector<ledbasic::DeviceProgram> deviceProgramsIncoming;
    bool deviceLedReady = false;
    ledbasic::DeviceLed deviceLedIncoming;
    bool deviceOpenReady = false;
    std::string deviceOpenName;
    std::string deviceOpenSource;
    std::string deviceStatusLive;
    std::thread deviceThread;
    float speed = 1.0f;

    auto pushLog = [&](const std::string& line) {
        immediateLog.push_back(line);
        if (immediateLog.size() > 8) immediateLog.erase(immediateLog.begin());
    };

    auto deviceNote = [&](const std::string& line) {
        std::lock_guard<std::mutex> lock(deviceMu);
        deviceLogs.push_back(line);
    };

    auto applyDeviceStatus = [&](const ledbasic::DeviceStatus& status) {
        std::lock_guard<std::mutex> lock(deviceMu);
        if (!status.ok) {
            deviceStatusLive = device.baseUrl.empty() ? "device off" : device.baseUrl + " offline";
            if (!status.message.empty()) deviceLogs.push_back(status.message);
            return;
        }
        std::string program = status.unsaved ? "unsaved" : status.program;
        deviceStatusLive = device.baseUrl + " " + status.mode + " " + program;
        if (!status.fault.empty()) deviceLogs.push_back(status.fault);
    };

    auto refreshDeviceStatus = [&] {
        applyDeviceStatus(device.status());
    };

    auto startDeviceJob = [&](std::function<void()> fn) {
        {
            std::lock_guard<std::mutex> lock(deviceMu);
            if (deviceBusy) {
                deviceLogs.push_back("device request already running");
                return;
            }
            deviceBusy = true;
        }
        if (deviceThread.joinable()) deviceThread.join();
        deviceThread = std::thread([&, fn = std::move(fn)] {
            fn();
            std::lock_guard<std::mutex> lock(deviceMu);
            deviceBusy = false;
        });
    };

    auto runOnDevice = [&] {
        std::string source = editor->getText();
        startDeviceJob([&, source] {
            ledbasic::HttpResult result = device.runSource(source);
            if (result.transportOk && result.status >= 200 && result.status < 300) {
                deviceNote("device running editor buffer");
            } else {
                deviceNote(ledbasic::deviceResultMessage(result));
            }
            refreshDeviceStatus();
        });
    };

    auto uploadNamed = [&](const std::string& name) {
        std::string source = editor->getText();
        if (!ledbasic::validUserProgramName(name)) {
            startDeviceJob([&, name, source] {
                ledbasic::HttpResult result = device.runSource(source);
                if (result.transportOk && result.status >= 200 && result.status < 300) {
                    deviceNote("ran on device, " + name + " was not stored");
                } else {
                    deviceNote(ledbasic::deviceResultMessage(result));
                }
                refreshDeviceStatus();
            });
            return;
        }
        startDeviceJob([&, name, source] {
            ledbasic::HttpResult result = device.upload(name, source, true);
            if (result.transportOk && result.status >= 200 && result.status < 300) {
                deviceNote("device stored " + name);
            } else {
                deviceNote(ledbasic::deviceResultMessage(result));
            }
            refreshDeviceStatus();
        });
    };

    auto uploadCurrent = [&] {
        std::string name = ledbasic::programNameFromPath(filePath);
        if (filePath.empty() || !ledbasic::validUserProgramName(name)) {
            deviceNameInput = name;
            showDeviceName = true;
            return;
        }
        uploadNamed(name);
    };

    auto fetchPrograms = [&] {
        std::vector<ledbasic::DeviceProgram> programs;
        std::string error;
        if (!device.programs(programs, error)) {
            deviceNote(error.empty() ? "could not list programs" : error);
            return;
        }
        std::lock_guard<std::mutex> lock(deviceMu);
        deviceProgramsIncoming = std::move(programs);
        deviceProgramsReady = true;
    };

    auto fetchLeds = [&] {
        ledbasic::DeviceLed led;
        std::string error;
        if (!device.led(led, error)) {
            deviceNote(error.empty() ? "could not read LEDs" : error);
            return;
        }
        std::lock_guard<std::mutex> lock(deviceMu);
        deviceLedIncoming = led;
        deviceLedReady = true;
    };

    auto saveLeds = [&] {
        ledbasic::DeviceLed led = loadedLed;
        led.type = ledTypeInput;
        led.order = ledOrderInput;
        led.length = std::atoi(ledLengthInput.c_str());
        led.pin = std::atoi(ledPinInput.c_str());
        led.brightness = std::atoi(ledBrightInput.c_str());
        bool typeOk = led.type == "ws2812" || led.type == "ws2811";
        bool orderOk = led.order == "RGB" || led.order == "RBG" || led.order == "GRB" ||
                       led.order == "GBR" || led.order == "BRG" || led.order == "BGR";
        bool pinOk = led.pin == 16 || led.pin == 14 || led.pin == 13 || led.pin == 12 ||
                     led.pin == 4 || led.pin == 2;
        if (!typeOk || !orderOk || led.length < 1 || led.length > 1024 || !pinOk ||
            led.brightness < 0 || led.brightness > 255) {
            pushLog("invalid LED settings");
            return;
        }
        bool hardware = !hasLoadedLed || led.type != loadedLed.type || led.order != loadedLed.order ||
                        led.length != loadedLed.length || led.pin != loadedLed.pin;
        startDeviceJob([&, led, hardware] {
            ledbasic::HttpResult result = device.putLed(led);
            if (!(result.transportOk && result.status >= 200 && result.status < 300)) {
                deviceNote(ledbasic::deviceResultMessage(result));
                return;
            }
            if (hardware || ledbasic::deviceWillRestart(result)) {
                deviceNote("board restarting, reconnect in a few seconds");
            } else {
                deviceNote("brightness " + std::to_string(led.brightness));
            }
            {
                std::lock_guard<std::mutex> lock(deviceMu);
                deviceLedIncoming = led;
                deviceLedReady = true;
            }
            if (!hardware) refreshDeviceStatus();
        });
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
        menuBtn(" Connect… ", [&] {
            closeMenus();
            deviceUrlInput = device.baseUrl;
            showDeviceConnect = true;
        }),
        menuBtn(" Run on device ", [&] {
            closeMenus();
            runOnDevice();
        }),
        menuBtn(" Upload and run ", [&] {
            closeMenus();
            uploadCurrent();
        }),
        menuBtn(" Programs… ", [&] {
            closeMenus();
            showDevicePrograms = true;
            startDeviceJob(fetchPrograms);
        }),
        menuBtn(" LEDs… ", [&] {
            closeMenus();
            showDeviceLeds = true;
            startDeviceJob(fetchLeds);
        }),
    }));

    auto deviceUrlField = Input(&deviceUrlInput, "http://192.168.4.1");
    auto connectDialog = Container::Vertical({
        Renderer([] { return text(" Connect") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        deviceUrlField,
        Container::Horizontal({
            menuBtn(" Save ", [&] {
                device.setBaseUrl(deviceUrlInput);
                deviceUrlInput = device.baseUrl;
                if (ledbasic::saveDeviceUrl(device.baseUrl)) pushLog("saved " + device.baseUrl);
                else pushLog("could not save device url");
                showDeviceConnect = false;
                startDeviceJob(refreshDeviceStatus);
            }),
            menuBtn(" Status ", [&] { startDeviceJob(refreshDeviceStatus); }),
            menuBtn(" Close ", [&] { showDeviceConnect = false; }),
        }),
    });
    connectDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 52);
    });

    auto deviceNameField = Input(&deviceNameInput, "program name");
    auto nameDialog = Container::Vertical({
        Renderer([] { return text(" Program name") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        deviceNameField,
        Container::Horizontal({
            menuBtn(" Upload ", [&] {
                showDeviceName = false;
                uploadNamed(deviceNameInput);
            }),
            menuBtn(" Cancel ", [&] { showDeviceName = false; }),
        }),
    });
    nameDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 52);
    });

    auto programList = Renderer([&] {
        Elements lines;
        lines.push_back(text(" Programs") | bold | color(theme::accent()));
        lines.push_back(separator());
        if (devicePrograms.empty()) {
            lines.push_back(text("  (none)") | color(theme::dim()));
            return vbox(std::move(lines));
        }
        if (deviceIndex >= (int)devicePrograms.size()) deviceIndex = (int)devicePrograms.size() - 1;
        if (deviceIndex < 0) deviceIndex = 0;
        int begin = std::max(0, deviceIndex - 5);
        int end = std::min((int)devicePrograms.size(), begin + 12);
        for (int i = begin; i < end; i++) {
            const auto& program = devicePrograms[(size_t)i];
            std::string label = std::string(i == deviceIndex ? "> " : "  ") + program.name;
            if (program.builtin) label += "  built-in";
            if (program.active) label += "  running";
            auto line = text(label);
            if (i == deviceIndex) line = line | inverted;
            lines.push_back(line);
        }
        return vbox(std::move(lines));
    });
    auto selectedProgram = [&]() -> const ledbasic::DeviceProgram* {
        if (deviceIndex < 0 || deviceIndex >= (int)devicePrograms.size()) return nullptr;
        return &devicePrograms[(size_t)deviceIndex];
    };
    auto programsDialog = Container::Vertical({
        programList,
        Container::Horizontal({
            menuBtn(" Prev ", [&] {
                if (deviceIndex > 0) deviceIndex--;
            }),
            menuBtn(" Next ", [&] {
                if (deviceIndex + 1 < (int)devicePrograms.size()) deviceIndex++;
            }),
        }),
        Container::Horizontal({
            menuBtn(" Run ", [&] {
                const ledbasic::DeviceProgram* program = selectedProgram();
                if (!program) return;
                std::string name = program->name;
                startDeviceJob([&, name] {
                    ledbasic::HttpResult result = device.activate(name);
                    if (result.transportOk && result.status >= 200 && result.status < 300) {
                        deviceNote("device running " + name);
                    } else {
                        deviceNote(ledbasic::deviceResultMessage(result));
                    }
                    refreshDeviceStatus();
                    fetchPrograms();
                });
            }),
            menuBtn(" Open ", [&] {
                const ledbasic::DeviceProgram* program = selectedProgram();
                if (!program) return;
                std::string name = program->name;
                startDeviceJob([&, name] {
                    std::string source;
                    std::string error;
                    if (!device.programSource(name, source, error)) {
                        deviceNote(error.empty() ? "could not open program" : error);
                        return;
                    }
                    std::lock_guard<std::mutex> lock(deviceMu);
                    deviceOpenName = name;
                    deviceOpenSource = std::move(source);
                    deviceOpenReady = true;
                });
            }),
            menuBtn(" Delete ", [&] {
                const ledbasic::DeviceProgram* program = selectedProgram();
                if (!program) return;
                if (program->builtin) {
                    pushLog("built-in program cannot be deleted");
                    return;
                }
                std::string name = program->name;
                startDeviceJob([&, name] {
                    ledbasic::HttpResult result = device.removeProgram(name);
                    if (result.transportOk && result.status >= 200 && result.status < 300) {
                        deviceNote("deleted " + name);
                    } else {
                        deviceNote(ledbasic::deviceResultMessage(result));
                    }
                    refreshDeviceStatus();
                    fetchPrograms();
                });
            }),
            menuBtn(" Refresh ", [&] { startDeviceJob(fetchPrograms); }),
            menuBtn(" Close ", [&] { showDevicePrograms = false; }),
        }),
    });
    programsDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 56);
    });

    auto ledTypeField = Input(&ledTypeInput, "ws2812");
    auto ledOrderField = Input(&ledOrderInput, "GRB");
    auto ledLengthField = Input(&ledLengthInput, "60");
    auto ledPinField = Input(&ledPinInput, "16");
    auto ledBrightField = Input(&ledBrightInput, "128");
    auto ledsDialog = Container::Vertical({
        Renderer([] { return text(" LEDs") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        Renderer([] { return text(" type ws2812 or ws2811") | color(theme::dim()); }),
        ledTypeField,
        Renderer([] { return text(" order RGB RBG GRB GBR BRG BGR") | color(theme::dim()); }),
        ledOrderField,
        Renderer([] { return text(" length 1-1024") | color(theme::dim()); }),
        ledLengthField,
        Renderer([] { return text(" pin 16 14 13 12 4 2") | color(theme::dim()); }),
        ledPinField,
        Renderer([] { return text(" brightness 0-255") | color(theme::dim()); }),
        ledBrightField,
        Container::Horizontal({
            menuBtn(" Load ", [&] { startDeviceJob(fetchLeds); }),
            menuBtn(" Save ", [&] { saveLeds(); }),
            menuBtn(" Close ", [&] { showDeviceLeds = false; }),
        }),
    });
    ledsDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 52);
    });

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
                text(" F5 Run/Continue   Esc Stop   F8 Step   F10 Step Over"),
                text(" Shift+F5 runs the editor buffer on the connected board"),
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

    auto workspace = Container::Vertical({
        menuBar,
        editor,
        paramsFocus,
        immediateInput,
    });

    auto renderer = Renderer(workspace, [&] {
        {
            std::vector<std::string> logs;
            bool takePrograms = false;
            bool takeLed = false;
            bool takeOpen = false;
            std::vector<ledbasic::DeviceProgram> programs;
            ledbasic::DeviceLed led;
            std::string openName;
            std::string openSource;
            {
                std::lock_guard<std::mutex> lock(deviceMu);
                logs.swap(deviceLogs);
                if (!deviceStatusLive.empty()) deviceLine = deviceStatusLive;
                if (deviceProgramsReady) {
                    programs.swap(deviceProgramsIncoming);
                    deviceProgramsReady = false;
                    takePrograms = true;
                }
                if (deviceLedReady) {
                    led = deviceLedIncoming;
                    deviceLedReady = false;
                    takeLed = true;
                }
                if (deviceOpenReady) {
                    openName = deviceOpenName;
                    openSource = std::move(deviceOpenSource);
                    deviceOpenReady = false;
                    takeOpen = true;
                }
            }
            for (const auto& line : logs) pushLog(line);
            if (takePrograms) {
                devicePrograms = std::move(programs);
                if (deviceIndex >= (int)devicePrograms.size()) {
                    deviceIndex = (int)devicePrograms.size() - 1;
                }
                if (deviceIndex < 0) deviceIndex = 0;
            }
            if (takeLed) {
                loadedLed = led;
                hasLoadedLed = true;
                ledTypeInput = led.type;
                ledOrderInput = led.order;
                ledLengthInput = std::to_string(led.length);
                ledPinInput = std::to_string(led.pin);
                ledBrightInput = std::to_string(led.brightness);
            }
            if (takeOpen) {
                session.stop();
                editor->setText(openSource);
                filePath.clear();
                session.clearBreakpoints();
                editor->setBreakpoints({});
                editor->setErrorLine(0);
                if (!session.loadSource(openSource, openName)) {
                    pushLog("opened " + openName + " with errors");
                } else {
                    editor->clearDirty();
                    pushLog("opened " + openName + " from device");
                }
                showDevicePrograms = false;
            }
        }
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
                text(" Shift+F5 Device ") | color(theme::dim()),
                text(" F8 Step ") | color(theme::dim()),
                text(" F9 Break ") | color(theme::dim()),
                text(" F10 Over ") | color(theme::dim()),
                text(" Esc Stop ") | color(theme::dim()),
                filler(),
                text(deviceLine.size() > 42 ? deviceLine.substr(0, 42) : deviceLine) | color(theme::dim()),
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
    renderer |= Modal(connectDialog, &showDeviceConnect);
    renderer |= Modal(nameDialog, &showDeviceName);
    renderer |= Modal(programsDialog, &showDevicePrograms);
    renderer |= Modal(ledsDialog, &showDeviceLeds);
    renderer |= Modal(openDialog, &showOpen);
    renderer |= Modal(saveDialog, &showSaveAs);
    renderer |= Modal(helpDialog, &showHelp);

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
        if (e.input() == "\x1B[15;2~") {
            runOnDevice();
            return true;
        }
        if (e == Event::Escape) {
            if (showHelp || showOpen || showSaveAs || showFileMenu || showViewMenu ||
                showRunMenu || showDebugMenu || showDeviceMenu || showDeviceConnect ||
                showDeviceName || showDevicePrograms || showDeviceLeds) {
                showHelp = showOpen = showSaveAs = false;
                showDeviceConnect = showDeviceName = showDevicePrograms = showDeviceLeds = false;
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

    if (!device.baseUrl.empty()) startDeviceJob(refreshDeviceStatus);
    screen.Loop(renderer);
    alive = false;
    refresh.join();
    if (deviceThread.joinable()) deviceThread.join();
    session.stop();
    return 0;
}
