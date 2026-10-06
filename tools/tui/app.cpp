#include "app.h"
#include "Palettes.h"
#include "theme.h"
#include <ftxui/component/mouse.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

using namespace ftxui;
namespace fs = std::filesystem;

#ifndef LEDBASIC_EXAMPLES_DIR
#define LEDBASIC_EXAMPLES_DIR "examples/bas"
#endif

std::string examplesDir() {
    fs::path p(LEDBASIC_EXAMPLES_DIR);
    if (fs::is_directory(p)) return p.string();
    if (fs::is_directory("examples/bas")) return "examples/bas";
    if (fs::is_directory("../examples/bas")) return "../examples/bas";
    return p.string();
}

std::string defaultProgramPath() {
    fs::path p = fs::path(examplesDir()) / "Rainbow.bas";
    return p.string();
}

namespace {

const char* stateLabel(ledbasic::Snapshot::State s) {
    switch (s) {
        case ledbasic::Snapshot::Playing: return "PLAY";
        case ledbasic::Snapshot::Paused: return "PAUSE";
        default: return "STOP";
    }
}

Element renderStripCells(const ledbasic::Snapshot& snap, int width) {
    if (width < 8) width = 8;
    int n = (int)snap.leds.size();
    Elements cells;
    cells.reserve((size_t)n);
    float scale = (snap.brightness / 255.0f) * (snap.master / 255.0f);
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

bool httpOk(const ledbasic::HttpResult& result) {
    return result.transportOk && result.status >= 200 && result.status < 300;
}

class FocusPad : public ComponentBase {
public:
    bool Focusable() const override { return true; }
    Element OnRender() override { return text(" "); }
};

Component menuBtn(const char* label, std::function<void()> fn) {
    return Button(label, std::move(fn), ButtonOption::Simple());
}

Component wrapMenu(const char* title, Component inner) {
    inner |= Renderer([title](Element e) {
        return vbox(Elements{
                   text(std::string(" ") + title) | bold | color(theme::accent()),
                   separator(),
                   e,
               }) |
               border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 34);
    });
    return inner;
}

Component dialogFrame(Component inner, int width) {
    inner |= Renderer([width](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, width);
    });
    return inner;
}

} // namespace

App::App(const AppOptions& options)
    : session_(options.leds), editor_(std::make_shared<SourceEditor>()) {
    if (!options.deviceUrl.empty()) device_.setBaseUrl(options.deviceUrl);
    deviceUrlInput_ = device_.baseUrl;
    deviceLine_ = device_.baseUrl.empty() ? "device off" : device_.baseUrl;

    filePath_ = options.startFile;
    std::ifstream in(filePath_);
    if (!filePath_.empty() && in) {
        std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        editor_->setText(body);
        session_.loadFile(filePath_);
        editor_->clearDirty();
    } else {
        editor_->setText(
            "setup\n"
            "  brightness(128)\n"
            "  clear()\n"
            "end\n"
            "\n"
            "loop(time)\n"
            "  fill(hsv(time / 20 % 360, 255, 255))\n"
            "  show()\n"
            "end\n");
        session_.loadSource(editor_->getText(), "");
        editor_->clearDirty();
    }

    editor_->onBreakpointChanged = [this] {
        session_.clearBreakpoints();
        for (int line : editor_->breakpoints()) session_.setBreakpoint(line, true);
    };

    build();
}

App::~App() {
    if (deviceThread_.joinable()) deviceThread_.join();
    session_.stop();
}

void App::start() {
    if (!device_.baseUrl.empty()) startDeviceJob([this] { refreshDeviceStatus(); });
}

// ---------------------------------------------------------------- device

void App::pushLog(const std::string& line) {
    immediateLog_.push_back(line);
    if (immediateLog_.size() > 8) immediateLog_.erase(immediateLog_.begin());
}

void App::deviceNote(const std::string& line) {
    std::lock_guard<std::mutex> lock(deviceMu_);
    deviceLogs_.push_back(line);
}

void App::applyDeviceStatus(const ledbasic::DeviceStatus& status) {
    std::lock_guard<std::mutex> lock(deviceMu_);
    if (!status.ok) {
        deviceStatusLive_ = device_.baseUrl.empty() ? "device off" : device_.baseUrl + " offline";
        if (!status.message.empty()) deviceLogs_.push_back(status.message);
        return;
    }
    std::string program = status.unsaved ? "unsaved" : status.program;
    deviceStatusLive_ = device_.baseUrl + " " + status.mode + " " + program;
    if (!status.fault.empty()) deviceLogs_.push_back(status.fault);
}

void App::refreshDeviceStatus() {
    applyDeviceStatus(device_.status());
}

void App::startDeviceJob(std::function<void()> fn) {
    {
        std::lock_guard<std::mutex> lock(deviceMu_);
        if (deviceBusy_) {
            deviceLogs_.push_back("device request already running");
            return;
        }
        deviceBusy_ = true;
    }
    if (deviceThread_.joinable()) deviceThread_.join();
    deviceThread_ = std::thread([this, fn = std::move(fn)] {
        fn();
        std::lock_guard<std::mutex> lock(deviceMu_);
        deviceBusy_ = false;
    });
}

void App::runOnDevice() {
    std::string source = editor_->getText();
    startDeviceJob([this, source] {
        ledbasic::HttpResult result = device_.runSource(source);
        if (httpOk(result)) {
            deviceNote("device running editor buffer");
        } else {
            deviceNote(ledbasic::deviceResultMessage(result));
        }
        refreshDeviceStatus();
    });
}

void App::uploadNamed(const std::string& name) {
    std::string source = editor_->getText();
    if (!ledbasic::validUserProgramName(name)) {
        startDeviceJob([this, name, source] {
            ledbasic::HttpResult result = device_.runSource(source);
            if (httpOk(result)) {
                deviceNote("ran on device, " + name + " was not stored");
            } else {
                deviceNote(ledbasic::deviceResultMessage(result));
            }
            refreshDeviceStatus();
        });
        return;
    }
    startDeviceJob([this, name, source] {
        ledbasic::HttpResult result = device_.upload(name, source, true);
        if (httpOk(result)) {
            deviceNote("device stored " + name);
        } else {
            deviceNote(ledbasic::deviceResultMessage(result));
        }
        refreshDeviceStatus();
    });
}

void App::uploadCurrent() {
    std::string name = ledbasic::programNameFromPath(filePath_);
    if (filePath_.empty() || !ledbasic::validUserProgramName(name)) {
        deviceNameInput_ = name;
        showDeviceName_ = true;
        return;
    }
    uploadNamed(name);
}

void App::fetchPrograms() {
    std::vector<ledbasic::DeviceProgram> programs;
    std::string error;
    if (!device_.programs(programs, error)) {
        deviceNote(error.empty() ? "could not list programs" : error);
        return;
    }
    std::lock_guard<std::mutex> lock(deviceMu_);
    deviceProgramsIncoming_ = std::move(programs);
    deviceProgramsReady_ = true;
}

void App::fetchLeds() {
    ledbasic::DeviceLed led;
    std::string error;
    if (!device_.led(led, error)) {
        deviceNote(error.empty() ? "could not read LEDs" : error);
        return;
    }
    std::lock_guard<std::mutex> lock(deviceMu_);
    deviceLedIncoming_ = led;
    deviceLedReady_ = true;
}

void App::saveLeds() {
    ledbasic::DeviceLed led = loadedLed_;
    led.type = ledTypeInput_;
    led.order = ledOrderInput_;
    led.length = std::atoi(ledLengthInput_.c_str());
    led.pin = std::atoi(ledPinInput_.c_str());
    led.brightness = std::atoi(ledBrightInput_.c_str());
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
    bool hardware = !hasLoadedLed_ || led.type != loadedLed_.type || led.order != loadedLed_.order ||
                    led.length != loadedLed_.length || led.pin != loadedLed_.pin;
    startDeviceJob([this, led, hardware] {
        ledbasic::HttpResult result = device_.putLed(led);
        if (!httpOk(result)) {
            deviceNote(ledbasic::deviceResultMessage(result));
            return;
        }
        if (hardware || ledbasic::deviceWillRestart(result)) {
            deviceNote("board restarting, reconnect in a few seconds");
        } else {
            deviceNote("brightness " + std::to_string(led.brightness));
        }
        {
            std::lock_guard<std::mutex> lock(deviceMu_);
            deviceLedIncoming_ = led;
            deviceLedReady_ = true;
        }
        if (!hardware) refreshDeviceStatus();
    });
}

// Pulls results the device thread left for the UI thread. Runs every frame.
void App::drainDeviceResults() {
    std::vector<std::string> logs;
    bool takePrograms = false;
    bool takeLed = false;
    bool takeOpen = false;
    std::vector<ledbasic::DeviceProgram> programs;
    ledbasic::DeviceLed led;
    std::string openName;
    std::string openSource;
    {
        std::lock_guard<std::mutex> lock(deviceMu_);
        logs.swap(deviceLogs_);
        if (!deviceStatusLive_.empty()) deviceLine_ = deviceStatusLive_;
        if (deviceProgramsReady_) {
            programs.swap(deviceProgramsIncoming_);
            deviceProgramsReady_ = false;
            takePrograms = true;
        }
        if (deviceLedReady_) {
            led = deviceLedIncoming_;
            deviceLedReady_ = false;
            takeLed = true;
        }
        if (deviceOpenReady_) {
            openName = deviceOpenName_;
            openSource = std::move(deviceOpenSource_);
            deviceOpenReady_ = false;
            takeOpen = true;
        }
    }
    for (const auto& line : logs) pushLog(line);
    if (takePrograms) {
        devicePrograms_ = std::move(programs);
        if (deviceIndex_ >= (int)devicePrograms_.size()) {
            deviceIndex_ = (int)devicePrograms_.size() - 1;
        }
        if (deviceIndex_ < 0) deviceIndex_ = 0;
    }
    if (takeLed) {
        loadedLed_ = led;
        hasLoadedLed_ = true;
        ledTypeInput_ = led.type;
        ledOrderInput_ = led.order;
        ledLengthInput_ = std::to_string(led.length);
        ledPinInput_ = std::to_string(led.pin);
        ledBrightInput_ = std::to_string(led.brightness);
    }
    if (takeOpen) {
        session_.stop();
        editor_->setText(openSource);
        filePath_.clear();
        session_.clearBreakpoints();
        editor_->setBreakpoints({});
        editor_->setErrorLine(0);
        if (!session_.loadSource(openSource, openName)) {
            pushLog("opened " + openName + " with errors");
        } else {
            editor_->clearDirty();
            pushLog("opened " + openName + " from device");
        }
        showDevicePrograms_ = false;
    }
}

// Applies palette changes and forwards brightness/palette to the board,
// coalescing rapid slider moves into one in-flight request.
void App::syncDeviceTargets() {
    if (paletteIndex_ != session_.palette()) {
        session_.setPalette(paletteIndex_);
        if (!device_.baseUrl.empty()) devicePaletteTarget_.store(paletteIndex_);
    }

    auto deviceBusy = [this] {
        std::lock_guard<std::mutex> lock(deviceMu_);
        return deviceBusy_;
    };

    int pendingBright = deviceBrightTarget_.exchange(-1);
    if (pendingBright >= 0) {
        if (deviceBusy()) {
            int empty = -1;
            deviceBrightTarget_.compare_exchange_strong(empty, pendingBright);
        } else {
            startDeviceJob([this, pendingBright] {
                int send = pendingBright;
                for (;;) {
                    ledbasic::HttpResult result = device_.setBrightness(send);
                    if (!httpOk(result) && result.error.find("requires Windows") == std::string::npos) {
                        deviceNote(ledbasic::deviceResultMessage(result));
                    }
                    int next = deviceBrightTarget_.exchange(-1);
                    if (next < 0 || next == send) return;
                    send = next;
                }
            });
        }
    }

    int pendingPal = devicePaletteTarget_.exchange(-1);
    if (pendingPal >= 0) {
        if (deviceBusy()) {
            int empty = -1;
            devicePaletteTarget_.compare_exchange_strong(empty, pendingPal);
        } else {
            startDeviceJob([this, pendingPal] {
                int send = pendingPal;
                for (;;) {
                    ledbasic::HttpResult result = device_.setPalette(paletteName(send));
                    if (!httpOk(result) && result.error.find("requires Windows") == std::string::npos) {
                        deviceNote(ledbasic::deviceResultMessage(result));
                    }
                    int next = devicePaletteTarget_.exchange(-1);
                    if (next < 0 || next == send) return;
                    send = next;
                }
            });
        }
    }
}

// ---------------------------------------------------------------- editor/session

bool App::reloadFromEditor() {
    bool ok = session_.loadSource(editor_->getText(), filePath_);
    session_.clearBreakpoints();
    for (int line : editor_->breakpoints()) session_.setBreakpoint(line, true);
    if (!ok) {
        auto diags = session_.diagnostics();
        if (!diags.empty()) {
            editor_->setErrorLine(diags[0].line);
            pushLog("error " + std::to_string(diags[0].line) + ": " +
                    std::string(diags[0].message.c_str()));
        } else {
            pushLog("load failed");
        }
        return false;
    }
    editor_->setErrorLine(0);
    return true;
}

// Paused on unchanged source: resume where it stopped. Otherwise (stopped,
// edited since load, or paused on a runtime error) reload and start over.
void App::runOrContinue() {
    auto snap = session_.snapshot();
    bool resumable = snap.state == ledbasic::Snapshot::Paused && !snap.runtimeError &&
                     editor_->getText() == session_.source();
    if (resumable || reloadFromEditor()) session_.run();
}

void App::step(bool over) {
    if (session_.snapshot().state == ledbasic::Snapshot::Stopped) {
        if (!reloadFromEditor()) return;
    }
    if (over) session_.stepOver();
    else session_.stepInto();
}

void App::saveTo(const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        pushLog("could not write " + path);
        return;
    }
    out << editor_->getText();
    filePath_ = path;
    editor_->clearDirty();
    pushLog("saved " + path);
}

void App::doOpen(const std::string& path) {
    session_.stop();
    std::ifstream in(path);
    if (!in) {
        pushLog("could not open " + path);
        return;
    }
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    editor_->setText(body);
    filePath_ = path;
    session_.clearBreakpoints();
    editor_->setBreakpoints({});
    if (!session_.loadFile(path)) {
        auto diags = session_.diagnostics();
        if (!diags.empty()) {
            editor_->setErrorLine(diags[0].line);
            pushLog("error " + std::to_string(diags[0].line) + ": " +
                    std::string(diags[0].message.c_str()));
        }
    } else {
        editor_->setErrorLine(0);
        pushLog("opened " + path);
    }
}

void App::doNew() {
    session_.stop();
    editor_->setText("setup\nend\n\nloop(time)\n  show()\nend\n");
    filePath_.clear();
    session_.loadSource(editor_->getText(), "");
    editor_->clearDirty();
    pushLog("new program");
}

void App::doSave() {
    if (filePath_.empty()) {
        showSaveAs_ = true;
        pathInput_ = "untitled.bas";
    } else {
        saveTo(filePath_);
    }
}

void App::showOpenDialog() {
    closeMenus();
    showOpen_ = true;
    pathInput_ = examplesDir() + "/";
    if (exampleMenu_) exampleMenu_->TakeFocus();
}

void App::onImmediateEnter() {
    std::string cmd = immediate_;
    immediate_.clear();
    if (cmd.empty()) return;
    std::string upper = cmd;
    for (char& c : upper) if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (upper == "RUN") {
        runOrContinue();
        return;
    }
    if (upper == "STOP") {
        session_.stop();
        return;
    }
    auto r = session_.evalImmediate(cmd);
    pushLog("? " + cmd);
    pushLog(r.text);
}

void App::applySpeed(float s) {
    speed_ = s;
    session_.setSpeedMultiplier(s);
}

void App::closeMenus() {
    showFileMenu_ = showViewMenu_ = showRunMenu_ = showDebugMenu_ = showDeviceMenu_ = false;
}

void App::toggleMenu(bool* which) {
    bool next = !*which;
    closeMenus();
    *which = next;
}

bool App::anyOverlay() const {
    return showHelp_ || showOpen_ || showSaveAs_ || showFileMenu_ || showViewMenu_ ||
           showRunMenu_ || showDebugMenu_ || showDeviceMenu_ || showDeviceConnect_ ||
           showDeviceName_ || showDevicePrograms_ || showDeviceLeds_;
}

void App::closeOverlays() {
    showHelp_ = showOpen_ = showSaveAs_ = false;
    showDeviceConnect_ = showDeviceName_ = showDevicePrograms_ = showDeviceLeds_ = false;
    closeMenus();
}

// ---------------------------------------------------------------- components

Components App::buildOverlays() {
    auto fileBtn = menuBtn(" File ", [this] { toggleMenu(&showFileMenu_); });
    auto viewBtn = menuBtn(" View ", [this] { toggleMenu(&showViewMenu_); });
    auto runBtn = menuBtn(" Run ", [this] { toggleMenu(&showRunMenu_); });
    auto debugBtn = menuBtn(" Debug ", [this] { toggleMenu(&showDebugMenu_); });
    auto deviceBtn = menuBtn(" Device ", [this] { toggleMenu(&showDeviceMenu_); });
    auto helpBtn = menuBtn(" Help ", [this] {
        closeMenus();
        showHelp_ = true;
    });
    menuBar_ = Container::Horizontal({fileBtn, viewBtn, runBtn, debugBtn, deviceBtn, helpBtn});

    auto fileMenu = wrapMenu("File", Container::Vertical({
        menuBtn(" New              Ctrl+N ", [this] { closeMenus(); doNew(); }),
        menuBtn(" Open…            Ctrl+O ", [this] { showOpenDialog(); }),
        menuBtn(" Save             Ctrl+S ", [this] { closeMenus(); doSave(); }),
        menuBtn(" Save As…                ", [this] {
            closeMenus();
            showSaveAs_ = true;
            pathInput_ = filePath_.empty() ? "untitled.bas" : filePath_;
        }),
        menuBtn(" Exit             Ctrl+Q ", [this] {
            closeMenus();
            if (onExit) onExit();
        }),
    }));

    auto setLeds = [this](int n) {
        closeMenus();
        session_.setLedCount(n);
        reloadFromEditor();
    };
    auto viewMenu = wrapMenu("View", Container::Vertical({
        menuBtn(" Speed 0.25x   (1) ", [this] { closeMenus(); applySpeed(0.25f); }),
        menuBtn(" Speed 1x      (2) ", [this] { closeMenus(); applySpeed(1.0f); }),
        menuBtn(" Speed 2x      (3) ", [this] { closeMenus(); applySpeed(2.0f); }),
        menuBtn(" Speed 4x      (4) ", [this] { closeMenus(); applySpeed(4.0f); }),
        menuBtn(" 30 LEDs           ", [setLeds] { setLeds(30); }),
        menuBtn(" 60 LEDs           ", [setLeds] { setLeds(60); }),
        menuBtn(" 120 LEDs          ", [setLeds] { setLeds(120); }),
    }));

    auto runMenu = wrapMenu("Run", Container::Vertical({
        menuBtn(" Start / Continue    F5  ", [this] {
            closeMenus();
            runOrContinue();
        }),
        menuBtn(" Stop                Esc ", [this] {
            closeMenus();
            session_.stop();
        }),
    }));

    auto debugMenu = wrapMenu("Debug", Container::Vertical({
        menuBtn(" Step Into           F8  ", [this] {
            closeMenus();
            step(false);
        }),
        menuBtn(" Step Over           F10 ", [this] {
            closeMenus();
            step(true);
        }),
        menuBtn(" Toggle Breakpoint   F9  ", [this] {
            closeMenus();
            editor_->toggleBreakpointAtCursor();
        }),
    }));

    auto deviceMenu = wrapMenu("Device", Container::Vertical({
        menuBtn(" Connect… ", [this] {
            closeMenus();
            deviceUrlInput_ = device_.baseUrl;
            showDeviceConnect_ = true;
        }),
        menuBtn(" Run on device ", [this] {
            closeMenus();
            runOnDevice();
        }),
        menuBtn(" Upload and run ", [this] {
            closeMenus();
            uploadCurrent();
        }),
        menuBtn(" Programs… ", [this] {
            closeMenus();
            showDevicePrograms_ = true;
            startDeviceJob([this] { fetchPrograms(); });
        }),
        menuBtn(" LEDs… ", [this] {
            closeMenus();
            showDeviceLeds_ = true;
            startDeviceJob([this] { fetchLeds(); });
        }),
    }));

    auto connectDialog = dialogFrame(Container::Vertical({
        Renderer([] { return text(" Connect") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        Input(&deviceUrlInput_, "http://192.168.4.1"),
        Container::Horizontal({
            menuBtn(" Save ", [this] {
                device_.setBaseUrl(deviceUrlInput_);
                deviceUrlInput_ = device_.baseUrl;
                if (ledbasic::saveDeviceUrl(device_.baseUrl)) pushLog("saved " + device_.baseUrl);
                else pushLog("could not save device url");
                showDeviceConnect_ = false;
                startDeviceJob([this] { refreshDeviceStatus(); });
            }),
            menuBtn(" Status ", [this] { startDeviceJob([this] { refreshDeviceStatus(); }); }),
            menuBtn(" Close ", [this] { showDeviceConnect_ = false; }),
        }),
    }), 52);

    auto nameDialog = dialogFrame(Container::Vertical({
        Renderer([] { return text(" Program name") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        Input(&deviceNameInput_, "program name"),
        Container::Horizontal({
            menuBtn(" Upload ", [this] {
                showDeviceName_ = false;
                uploadNamed(deviceNameInput_);
            }),
            menuBtn(" Cancel ", [this] { showDeviceName_ = false; }),
        }),
    }), 52);

    auto programList = Renderer([this] {
        Elements lines;
        lines.push_back(text(" Programs") | bold | color(theme::accent()));
        lines.push_back(separator());
        if (devicePrograms_.empty()) {
            lines.push_back(text("  (none)") | color(theme::dim()));
            return vbox(std::move(lines));
        }
        if (deviceIndex_ >= (int)devicePrograms_.size()) deviceIndex_ = (int)devicePrograms_.size() - 1;
        if (deviceIndex_ < 0) deviceIndex_ = 0;
        int begin = std::max(0, deviceIndex_ - 5);
        int end = std::min((int)devicePrograms_.size(), begin + 12);
        for (int i = begin; i < end; i++) {
            const auto& program = devicePrograms_[(size_t)i];
            std::string label = std::string(i == deviceIndex_ ? "> " : "  ") + program.name;
            if (program.builtin) label += "  built-in";
            if (program.active) label += "  running";
            auto line = text(label);
            if (i == deviceIndex_) line = line | inverted;
            lines.push_back(line);
        }
        return vbox(std::move(lines));
    });
    auto selectedProgram = [this]() -> const ledbasic::DeviceProgram* {
        if (deviceIndex_ < 0 || deviceIndex_ >= (int)devicePrograms_.size()) return nullptr;
        return &devicePrograms_[(size_t)deviceIndex_];
    };
    auto programsDialog = dialogFrame(Container::Vertical({
        programList,
        Container::Horizontal({
            menuBtn(" Prev ", [this] {
                if (deviceIndex_ > 0) deviceIndex_--;
            }),
            menuBtn(" Next ", [this] {
                if (deviceIndex_ + 1 < (int)devicePrograms_.size()) deviceIndex_++;
            }),
        }),
        Container::Horizontal({
            menuBtn(" Run ", [this, selectedProgram] {
                const ledbasic::DeviceProgram* program = selectedProgram();
                if (!program) return;
                std::string name = program->name;
                startDeviceJob([this, name] {
                    ledbasic::HttpResult result = device_.activate(name);
                    if (httpOk(result)) {
                        deviceNote("device running " + name);
                    } else {
                        deviceNote(ledbasic::deviceResultMessage(result));
                    }
                    refreshDeviceStatus();
                    fetchPrograms();
                });
            }),
            menuBtn(" Open ", [this, selectedProgram] {
                const ledbasic::DeviceProgram* program = selectedProgram();
                if (!program) return;
                std::string name = program->name;
                startDeviceJob([this, name] {
                    std::string source;
                    std::string error;
                    if (!device_.programSource(name, source, error)) {
                        deviceNote(error.empty() ? "could not open program" : error);
                        return;
                    }
                    std::lock_guard<std::mutex> lock(deviceMu_);
                    deviceOpenName_ = name;
                    deviceOpenSource_ = std::move(source);
                    deviceOpenReady_ = true;
                });
            }),
            menuBtn(" Delete ", [this, selectedProgram] {
                const ledbasic::DeviceProgram* program = selectedProgram();
                if (!program) return;
                if (program->builtin) {
                    pushLog("built-in program cannot be deleted");
                    return;
                }
                std::string name = program->name;
                startDeviceJob([this, name] {
                    ledbasic::HttpResult result = device_.removeProgram(name);
                    if (httpOk(result)) {
                        deviceNote("deleted " + name);
                    } else {
                        deviceNote(ledbasic::deviceResultMessage(result));
                    }
                    refreshDeviceStatus();
                    fetchPrograms();
                });
            }),
            menuBtn(" Refresh ", [this] { startDeviceJob([this] { fetchPrograms(); }); }),
            menuBtn(" Close ", [this] { showDevicePrograms_ = false; }),
        }),
    }), 56);

    auto ledsDialog = dialogFrame(Container::Vertical({
        Renderer([] { return text(" LEDs") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        Renderer([] { return text(" type ws2812 or ws2811") | color(theme::dim()); }),
        Input(&ledTypeInput_, "ws2812"),
        Renderer([] { return text(" order RGB RBG GRB GBR BRG BGR") | color(theme::dim()); }),
        Input(&ledOrderInput_, "GRB"),
        Renderer([] { return text(" length 1-1024") | color(theme::dim()); }),
        Input(&ledLengthInput_, "60"),
        Renderer([] { return text(" pin 16 14 13 12 4 2") | color(theme::dim()); }),
        Input(&ledPinInput_, "16"),
        Renderer([] { return text(" brightness 0-255") | color(theme::dim()); }),
        Input(&ledBrightInput_, "128"),
        Container::Horizontal({
            menuBtn(" Load ", [this] { startDeviceJob([this] { fetchLeds(); }); }),
            menuBtn(" Save ", [this] { saveLeds(); }),
            menuBtn(" Close ", [this] { showDeviceLeds_ = false; }),
        }),
    }), 52);

    {
        std::error_code ec;
        std::vector<fs::path> examples;
        for (auto& p : fs::directory_iterator(examplesDir(), ec)) {
            if (p.path().extension() == ".bas") examples.push_back(p.path());
        }
        std::sort(examples.begin(), examples.end());
        for (const auto& p : examples) {
            examplePaths_.push_back(p.string());
            exampleLabels_.push_back(" " + p.filename().string());
        }
    }
    // A scrolling one-row-per-entry list: there are dozens of examples, and a
    // column of bordered buttons outgrew the screen and rendered empty.
    // Arrows/PageUp/PageDown/wheel scroll, Return opens, one click opens.
    auto openExample = [this] {
        if (exampleIndex_ < 0 || exampleIndex_ >= (int)examplePaths_.size()) return;
        doOpen(examplePaths_[(size_t)exampleIndex_]);
        showOpen_ = false;
    };
    MenuOption exampleOpt = MenuOption::Vertical();
    exampleOpt.on_enter = openExample;
    auto exampleMenu = Menu(&exampleLabels_, &exampleIndex_, exampleOpt);
    exampleMenu_ = exampleMenu;
    auto exampleList = Renderer(exampleMenu, [this, exampleMenu] {
        return exampleMenu->Render() | vscroll_indicator | frame |
               size(HEIGHT, LESS_THAN, 12) | reflect(exampleBox_);
    });
    exampleList |= CatchEvent([this, exampleMenu, openExample](Event e) {
        if (!e.is_mouse() || e.mouse().button != Mouse::Left ||
            e.mouse().motion != Mouse::Pressed ||
            !exampleBox_.Contain(e.mouse().x, e.mouse().y)) {
            return false;
        }
        if (exampleMenu->OnEvent(e)) openExample();
        return true;
    });
    auto openDialog = dialogFrame(Container::Vertical({
        Renderer([] { return text(" Open") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        exampleList,
        Renderer([] { return separator(); }),
        Input(&pathInput_, "path to .bas file"),
        Container::Horizontal({
            menuBtn(" Open ", [this] {
                doOpen(pathInput_);
                showOpen_ = false;
            }),
            menuBtn(" Cancel ", [this] { showOpen_ = false; }),
        }),
    }), 52);

    auto saveDialog = dialogFrame(Container::Vertical({
        Renderer([] { return text(" Save as") | bold | color(theme::accent()); }),
        Renderer([] { return separator(); }),
        Input(&pathInput_, "path to .bas file"),
        Container::Horizontal({
            menuBtn(" Save ", [this] {
                saveTo(pathInput_);
                showSaveAs_ = false;
            }),
            menuBtn(" Cancel ", [this] { showSaveAs_ = false; }),
        }),
    }), 52);

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
                text(" Brightness slider scales the strip, and the board when connected"),
                text(" Palette menu colors the programs that call setpal"),
            });
        }),
        menuBtn(" Close ", [this] { showHelp_ = false; }),
    });
    helpDialog |= Renderer([](Element e) {
        return e | border | bgcolor(theme::chrome()) | size(WIDTH, EQUAL, 70) | center;
    });

    // Order matches the flags in build(); later entries draw on top.
    return {
        fileMenu, viewMenu, runMenu, debugMenu, deviceMenu, connectDialog, nameDialog,
        programsDialog, ledsDialog, openDialog, saveDialog, helpDialog,
    };
}

void App::build() {
    InputOption immOpt;
    immOpt.on_enter = [this] { onImmediateEnter(); };
    immediateInput_ = Input(&immediate_, "Immediate  (? expr  |  name = expr  |  RUN)", immOpt);
    paramsFocus_ = Make<FocusPad>();

    for (int i = 0; i < paletteCount(); i++) paletteLabels_.push_back(paletteName(i));
    auto paletteDrop = Dropdown(&paletteLabels_, &paletteIndex_);
    SliderOption<int> brightOpt;
    brightOpt.value = &brightnessValue_;
    brightOpt.min = 0;
    brightOpt.max = 255;
    brightOpt.increment = 1;
    brightOpt.on_change = [this] {
        session_.setMasterBrightness(brightnessValue_);
        if (!device_.baseUrl.empty()) deviceBrightTarget_.store(brightnessValue_);
    };
    dimmerRow_ = Container::Horizontal({
        Slider(brightOpt) | flex,
        paletteDrop | size(WIDTH, EQUAL, 18),
    });

    Components overlays = buildOverlays();

    auto workspace = Container::Vertical({
        menuBar_,
        dimmerRow_,
        editor_,
        paramsFocus_,
        immediateInput_,
    });

    root_ = Renderer(workspace, [this] { return renderWorkspace(); });
    bool* flags[] = {
        &showFileMenu_, &showViewMenu_, &showRunMenu_, &showDebugMenu_, &showDeviceMenu_,
        &showDeviceConnect_, &showDeviceName_, &showDevicePrograms_, &showDeviceLeds_,
        &showOpen_, &showSaveAs_, &showHelp_,
    };
    for (size_t i = 0; i < overlays.size(); i++) root_ |= Modal(overlays[i], flags[i]);
    root_ |= CatchEvent([this](Event e) { return handleKey(e); });
}

Element App::renderWorkspace() {
    drainDeviceResults();

    auto snap = session_.snapshot();
    editor_->setReadOnly(snap.state == ledbasic::Snapshot::Playing);
    editor_->setCurrentLine(snap.state == ledbasic::Snapshot::Paused ? snap.currentLine : 0);
    if (snap.runtimeError) editor_->setErrorLine(snap.currentLine);

    char title[256];
    std::snprintf(title, sizeof(title), " %s%s ",
                  filePath_.empty() ? "untitled.bas" : filePath_.c_str(),
                  editor_->dirty() ? " *" : "");

    Element menuRow = hbox(Elements{
        menuBar_->Render(),
        filler(),
        text(" LEDBasic ") | color(theme::accent()) | bold,
    });

    char stripHead[160];
    std::snprintf(stripHead, sizeof(stripHead),
                  " %d LEDs   brightness %u   t=%lums   %.2fx   %s ",
                  (int)snap.leds.size(), (unsigned)snap.brightness, snap.timeMs,
                  speed_, stateLabel(snap.state));

    syncDeviceTargets();

    Element strip = vbox(Elements{
        text(stripHead) | color(theme::accent()),
        dimmerRow_->Render(),
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
        text(paramsFocus_->Focused() ? " Params  (Tab, Left/Right)" : " Params") |
        bold | color(theme::accent()));
    if (snap.parameters.empty()) {
        watchLines.push_back(text("  (none)") | color(theme::dim()));
    } else {
        if (paramIndex_ >= (int)snap.parameters.size()) paramIndex_ = (int)snap.parameters.size() - 1;
        if (paramIndex_ < 0) paramIndex_ = 0;
        for (int i = 0; i < (int)snap.parameters.size(); i++) {
            const auto& p = snap.parameters[(size_t)i];
            char buf[128];
            std::snprintf(buf, sizeof(buf), " %s %s = %.4g",
                          (i == paramIndex_ ? ">" : " "), p.name.c_str(), p.value);
            auto line = text(buf);
            if (i == paramIndex_) line = line | inverted;
            watchLines.push_back(line);
        }
        watchLines.push_back(text("  Left/Right adjust") | color(theme::dim()));
    }

    Elements immLines;
    for (const auto& l : immediateLog_) {
        immLines.push_back(text(l) | color(theme::dim()));
    }
    if (immLines.empty()) {
        immLines.push_back(text("Type an expression, assignment, RUN, or STOP") | color(theme::dim()));
    }

    Element editorPane = vbox(Elements{
        text(title) | color(theme::dim()),
        separator(),
        editor_->Render() | flex,
    }) | border;

    Element side = vbox(std::move(watchLines)) | border | size(WIDTH, EQUAL, 28);
    Element imm = vbox(Elements{
        text(" Immediate") | color(theme::accent()),
        vbox(std::move(immLines)),
        immediateInput_->Render(),
    }) | border | size(HEIGHT, EQUAL, 8);

    return vbox(Elements{
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
            text(deviceLine_.size() > 42 ? deviceLine_.substr(0, 42) : deviceLine_) | color(theme::dim()),
            text(editor_->readOnly() ? " running " : " editing ") | color(theme::dim()),
        }) | bgcolor(theme::chrome()),
    }) | bgcolor(theme::bg());
}

bool App::handleKey(Event e) {
    if (e == Event::F1) {
        closeMenus();
        showHelp_ = true;
        return true;
    }
    if (e == Event::F4) {
        immediateInput_->TakeFocus();
        return true;
    }
    if (e == Event::F5) {
        runOrContinue();
        return true;
    }
    if (e.input() == "\x1B[15;2~") {
        runOnDevice();
        return true;
    }
    if (e == Event::Escape) {
        if (anyOverlay()) {
            closeOverlays();
            return true;
        }
        session_.stop();
        return true;
    }
    if (e == Event::F8) {
        step(false);
        return true;
    }
    if (e == Event::F10) {
        step(true);
        return true;
    }
    if (e == Event::F9) {
        editor_->toggleBreakpointAtCursor();
        return true;
    }
    if (e == Event::CtrlQ) {
        if (onExit) onExit();
        return true;
    }
    if (e == Event::CtrlS) {
        doSave();
        return true;
    }
    if (e == Event::CtrlO) {
        showOpenDialog();
        return true;
    }
    if (e == Event::CtrlN) {
        doNew();
        return true;
    }

    // Single-key shortcuts must not fire while the user types text: in the
    // editor, the Immediate pane, or any dialog's fields.
    bool typingElsewhere = anyOverlay() || editor_->Focused() || immediateInput_->Focused();
    if (e.is_character() && !typingElsewhere) {
        std::string ch = e.character();
        if (ch == "1") { applySpeed(0.25f); return true; }
        if (ch == "2") { applySpeed(1.0f); return true; }
        if (ch == "3") { applySpeed(2.0f); return true; }
        if (ch == "4") { applySpeed(4.0f); return true; }
    }

    auto snap = session_.snapshot();
    if (paramsFocus_->Focused() && !snap.parameters.empty()) {
        if (e == Event::ArrowUp) {
            if (paramIndex_ > 0) paramIndex_--;
            return true;
        }
        if (e == Event::ArrowDown) {
            if (paramIndex_ + 1 < (int)snap.parameters.size()) paramIndex_++;
            return true;
        }
        if (e == Event::ArrowLeft || e == Event::ArrowRight) {
            const auto& p = snap.parameters[(size_t)paramIndex_];
            float step = p.stepValue != 0 ? p.stepValue : 1.f;
            float v = p.value + (e == Event::ArrowRight ? step : -step);
            if (v < p.minValue) v = p.minValue;
            if (v > p.maxValue) v = p.maxValue;
            session_.setParameter(p.name, v);
            return true;
        }
    }

    if (!typingElsewhere && e.is_character() && e.character() == "[") {
        int n = session_.ledCount();
        if (n > 10) session_.setLedCount(n / 2);
        if (editor_->dirty() || !filePath_.empty()) reloadFromEditor();
        return true;
    }
    if (!typingElsewhere && e.is_character() && e.character() == "]") {
        int n = session_.ledCount();
        session_.setLedCount(std::min(n * 2, ledbasic::HostSession::kMaxLeds));
        if (editor_->dirty() || !filePath_.empty()) reloadFromEditor();
        return true;
    }

    return false;
}
