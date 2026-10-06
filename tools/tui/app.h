#ifndef LEDBASIC_TUI_APP_H
#define LEDBASIC_TUI_APP_H

#include "DeviceClient.h"
#include "HostSession.h"
#include "editor.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

std::string examplesDir();
std::string defaultProgramPath();

struct AppOptions {
    int leds = ledbasic::HostSession::kDefaultLeds;
    std::string startFile; // empty or unreadable: built-in starter program
    std::string deviceUrl; // empty: device features off
};

// The whole TUI as one component tree with no terminal attached. main()
// hands root() to ScreenInteractive; tests send it events and render it
// into an in-memory Screen.
class App {
public:
    explicit App(const AppOptions& options);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    ftxui::Component root() const { return root_; }
    // Background work that should begin once the UI loop is running.
    void start();
    // File > Exit and Ctrl+Q. main() wires this to the screen's exit.
    std::function<void()> onExit;

    // Inspection hooks for tests.
    ledbasic::HostSession& session() { return session_; }
    SourceEditor& editor() { return *editor_; }
    ftxui::Component paramsFocus() const { return paramsFocus_; }
    ftxui::Component immediateInput() const { return immediateInput_; }
    const std::vector<std::string>& log() const { return immediateLog_; }
    const std::string& filePath() const { return filePath_; }
    float speed() const { return speed_; }
    int paramIndex() const { return paramIndex_; }
    bool helpVisible() const { return showHelp_; }

private:
    void build();
    // Builds menuBar_ and returns the menus and dialogs shown as modals.
    ftxui::Components buildOverlays();
    ftxui::Element renderWorkspace();
    bool handleKey(ftxui::Event e);

    void pushLog(const std::string& line);
    void deviceNote(const std::string& line);
    void applyDeviceStatus(const ledbasic::DeviceStatus& status);
    void refreshDeviceStatus();
    void startDeviceJob(std::function<void()> fn);
    void drainDeviceResults();
    void syncDeviceTargets();
    void runOnDevice();
    void uploadNamed(const std::string& name);
    void uploadCurrent();
    void fetchPrograms();
    void fetchLeds();
    void saveLeds();

    bool reloadFromEditor();
    void runOrContinue();
    void step(bool over);
    void saveTo(const std::string& path);
    void doOpen(const std::string& path);
    void doNew();
    void doSave();
    void showOpenDialog();
    void onImmediateEnter();
    void applySpeed(float s);
    void closeMenus();
    void toggleMenu(bool* which);
    bool anyOverlay() const;
    void closeOverlays();

    ledbasic::DeviceClient device_;
    ledbasic::HostSession session_;
    std::shared_ptr<SourceEditor> editor_;
    std::string filePath_;

    std::string immediate_;
    std::vector<std::string> immediateLog_;
    int paramIndex_ = 0;
    float speed_ = 1.0f;
    int brightnessValue_ = 255;
    std::vector<std::string> paletteLabels_;
    int paletteIndex_ = 0;

    bool showHelp_ = false;
    bool showOpen_ = false;
    bool showSaveAs_ = false;
    bool showFileMenu_ = false;
    bool showViewMenu_ = false;
    bool showRunMenu_ = false;
    bool showDebugMenu_ = false;
    bool showDeviceMenu_ = false;
    bool showDeviceConnect_ = false;
    bool showDeviceName_ = false;
    bool showDevicePrograms_ = false;
    bool showDeviceLeds_ = false;

    std::string pathInput_;
    std::vector<std::string> exampleLabels_;
    std::vector<std::string> examplePaths_;
    int exampleIndex_ = 0;
    ftxui::Box exampleBox_;

    std::string deviceUrlInput_;
    std::string deviceNameInput_;
    std::string ledTypeInput_ = "ws2812";
    std::string ledOrderInput_ = "GRB";
    std::string ledLengthInput_ = "60";
    std::string ledPinInput_ = "16";
    std::string ledBrightInput_ = "128";
    std::string deviceLine_;
    std::vector<ledbasic::DeviceProgram> devicePrograms_;
    int deviceIndex_ = 0;
    ledbasic::DeviceLed loadedLed_;
    bool hasLoadedLed_ = false;

    // Shared with the device worker thread; guarded by deviceMu_.
    std::mutex deviceMu_;
    std::vector<std::string> deviceLogs_;
    bool deviceBusy_ = false;
    bool deviceProgramsReady_ = false;
    std::vector<ledbasic::DeviceProgram> deviceProgramsIncoming_;
    bool deviceLedReady_ = false;
    ledbasic::DeviceLed deviceLedIncoming_;
    bool deviceOpenReady_ = false;
    std::string deviceOpenName_;
    std::string deviceOpenSource_;
    std::string deviceStatusLive_;
    std::thread deviceThread_;
    std::atomic<int> deviceBrightTarget_{-1};
    std::atomic<int> devicePaletteTarget_{-1};

    ftxui::Component menuBar_;
    ftxui::Component exampleMenu_;
    ftxui::Component dimmerRow_;
    ftxui::Component paramsFocus_;
    ftxui::Component immediateInput_;
    ftxui::Component root_;
};

#endif
