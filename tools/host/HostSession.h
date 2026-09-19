#ifndef LEDBASIC_HOST_SESSION_H
#define LEDBASIC_HOST_SESSION_H

#include "BasicInterpreter.h"

#include <condition_variable>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>

namespace ledbasic {

struct LedPixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

struct WatchVar {
    std::string name;
    std::string display;
    float number = 0;
};

struct HostParameter {
    std::string name;
    ParameterType type = PARAM_NUMBER;
    float value = 0;
    float minValue = 0;
    float maxValue = 1;
    float stepValue = 1;
    std::vector<std::string> enumValues;
};

struct Snapshot {
    enum State { Stopped, Playing, Paused };
    State state = Stopped;
    std::vector<LedPixel> leds;
    uint8_t brightness = 255;
    unsigned long timeMs = 0;
    int currentLine = 0;
    int currentColumn = 0;
    int statementDepth = 0;
    bool runtimeError = false;
    bool programLoaded = false;
    std::string sourcePath;
    std::vector<WatchVar> variables;
    std::vector<HostParameter> parameters;
    std::set<int> breakpoints;
};

struct EvalResult {
    bool ok = false;
    std::string text;
    Diagnostic error;
};

class HostSession {
public:
    static constexpr int kMaxLeds = 300;
    static constexpr int kDefaultLeds = 60;
    static constexpr unsigned kDefaultFrameDtMs = 16;

    explicit HostSession(int ledCount = kDefaultLeds);
    ~HostSession();

    HostSession(const HostSession&) = delete;
    HostSession& operator=(const HostSession&) = delete;

    bool loadSource(const std::string& source, const std::string& path = "");
    bool loadFile(const std::string& path);
    std::vector<Diagnostic> diagnostics() const;
    const std::string& source() const { return source_; }
    const std::string& sourcePath() const { return sourcePath_; }

    bool setLedCount(int n);
    int ledCount() const;
    void setFrameDtMs(unsigned dt);
    unsigned frameDtMs() const;
    void setSpeedMultiplier(float m);
    float speedMultiplier() const;

    void run();
    void pause();
    void stop();
    void stepInto();
    void stepOver();
    void stepFrame();

    void setBreakpoint(int line, bool on);
    bool isBreakpoint(int line) const;
    void clearBreakpoints();

    bool setParameter(const std::string& name, float value);
    EvalResult evalImmediate(const std::string& text);

    Snapshot snapshot() const;

    // Setup + N loop frames on the calling thread. Stops the worker first.
    bool runFrames(int n, std::vector<std::vector<LedPixel>>* dumps = nullptr);

private:
    enum StepMode { StepNone, StepInto, StepOver };

    void startWorker();
    void workerMain();
    void publishSnapshotLocked();
    void applyPendingParamsLocked();
    void resetSimulationLocked();
    bool ensureLoadedLocked();
    void requestStopAndWait();
    void captureController(Snapshot& s, std::vector<Diagnostic>& diags) const;
    void installCapture(Snapshot frozen, std::vector<Diagnostic> diags);

    static unsigned long clockMillis(void* user);
    static void clockDelay(int ms, void* user);
    static void debugHook(int line, int column, int depth, void* user);
    static std::string formatValue(const Value& v);
    static std::string toStd(const String& s);

    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::thread worker_;
    bool quit_ = false;
    bool cmdRun_ = false;
    bool cmdPause_ = false;
    bool cmdStop_ = false;
    bool cmdStepInto_ = false;
    bool cmdStepOver_ = false;
    bool cmdStepFrame_ = false;
    bool needSetup_ = true;
    StepMode stepMode_ = StepNone;
    int stepOverDepth_ = 0;
    Snapshot::State state_ = Snapshot::Stopped;

    std::vector<CRGB> leds_;
    BasicLEDController* controller_ = nullptr;
    std::string source_;
    std::string sourcePath_;
    unsigned long simTime_ = 0;
    unsigned frameDtMs_ = kDefaultFrameDtMs;
    float speedMul_ = 1.0f;
    std::set<int> breakpoints_;
    std::vector<std::pair<std::string, float>> pendingParams_;
    Snapshot lastSnap_;
    std::vector<Diagnostic> lastDiags_;
};

} // namespace ledbasic

#endif
