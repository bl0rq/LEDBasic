#include "HostSession.h"

#include <chrono>
#include <fstream>
#include <sstream>

// Defined by native tests; the Arduino.h mock references this symbol.
unsigned long ledbasic_mock_millis = 0;

namespace ledbasic {

static String toArduino(const std::string& s) {
    return String(s.c_str());
}

std::string HostSession::toStd(const String& s) {
    return std::string(s.c_str());
}

std::string HostSession::formatValue(const Value& v) {
    if (v.type == VAL_STRING) {
        return std::string("\"") + toStd(v.stringValue) + "\"";
    }
    if (v.type == VAL_COLOR) {
        std::ostringstream os;
        os << "rgb(" << (int)v.red() << ", " << (int)v.green() << ", " << (int)v.blue() << ")";
        return os.str();
    }
    if (v.type == VAL_ARRAY) {
        std::ostringstream os;
        os << "[" << v.arrayValue.size() << " items]";
        return os.str();
    }
    std::ostringstream os;
    os << v.numberValue;
    return os.str();
}

HostSession::HostSession(int ledCount) {
    if (ledCount < 1) ledCount = 1;
    if (ledCount > kMaxLeds) ledCount = kMaxLeds;
    leds_.assign((size_t)ledCount, CRGB());
    controller_ = new BasicLEDController(leds_.data(), (int)leds_.size());
    controller_->setOwnsPhysicalOutput(false);
    controller_->setAutoShow(false);
    controller_->setClock(clockMillis, clockDelay, this);
    controller_->setDebugHook(debugHook, this);
    publishSnapshotLocked();
    startWorker();
}

HostSession::~HostSession() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        quit_ = true;
        cmdStop_ = true;
        if (controller_) controller_->interrupt();
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    delete controller_;
    controller_ = nullptr;
}

unsigned long HostSession::clockMillis(void* user) {
    auto* self = static_cast<HostSession*>(user);
    return self->simTime_;
}

void HostSession::clockDelay(int ms, void* user) {
    auto* self = static_cast<HostSession*>(user);
    if (ms <= 0) return;
    self->simTime_ += (unsigned long)ms;
}

void HostSession::debugHook(int line, int column, int depth, void* user) {
    auto* self = static_cast<HostSession*>(user);
    std::unique_lock<std::mutex> lock(self->mu_);
    if (self->quit_ || self->cmdStop_) {
        if (self->controller_) self->controller_->interrupt();
        return;
    }

    bool hitBreak = self->breakpoints_.count(line) != 0;
    bool hitStep = false;
    if (self->stepMode_ == StepInto) hitStep = true;
    if (self->stepMode_ == StepOver && depth <= self->stepOverDepth_) hitStep = true;

    if (!hitBreak && !hitStep && !self->cmdPause_) {
        return;
    }
    // Pause-between-frames uses cmdPause_ after runLoop; don't freeze every
    // statement unless we are stepping or hitting a breakpoint.
    if (!hitBreak && !hitStep) {
        return;
    }

    self->state_ = Snapshot::Paused;
    self->cmdRun_ = false;
    self->stepMode_ = StepNone;
    self->cmdPause_ = false;
    self->publishSnapshotLocked();
    self->cv_.notify_all();

    self->cv_.wait(lock, [self] {
        return self->quit_ || self->cmdStop_ || self->cmdRun_ ||
               self->cmdStepInto_ || self->cmdStepOver_ || self->cmdStepFrame_;
    });

    if (self->quit_ || self->cmdStop_) {
        if (self->controller_) self->controller_->interrupt();
        return;
    }
    if (self->cmdStepInto_) {
        self->cmdStepInto_ = false;
        self->stepMode_ = StepInto;
        self->state_ = Snapshot::Playing;
    } else if (self->cmdStepOver_) {
        self->cmdStepOver_ = false;
        self->stepMode_ = StepOver;
        self->stepOverDepth_ = depth;
        self->state_ = Snapshot::Playing;
    } else if (self->cmdRun_) {
        self->cmdRun_ = false;
        self->stepMode_ = StepNone;
        self->state_ = Snapshot::Playing;
    } else if (self->cmdStepFrame_) {
        // Finish this statement and the rest of the frame; worker pauses after runLoop.
        self->stepMode_ = StepNone;
        self->state_ = Snapshot::Playing;
    }
}

void HostSession::startWorker() {
    worker_ = std::thread([this] { workerMain(); });
}

void HostSession::publishSnapshotLocked() {
    lastSnap_ = buildSnapshotLocked();
}

Snapshot HostSession::buildSnapshotLocked() const {
    Snapshot s;
    s.state = state_;
    s.timeMs = simTime_;
    s.sourcePath = sourcePath_;
    s.breakpoints = breakpoints_;
    s.programLoaded = controller_ && controller_->isProgramLoaded();
    if (controller_) {
        s.brightness = controller_->getOutputBrightness();
        s.currentLine = controller_->getCurrentLine();
        s.currentColumn = controller_->getCurrentColumn();
        s.statementDepth = controller_->getStatementDepth();
        s.runtimeError = controller_->hasRuntimeError();
        const int n = controller_->getNumLeds();
        const CRGB* leds = controller_->getLeds();
        s.leds.resize((size_t)n);
        for (int i = 0; i < n; i++) {
            s.leds[(size_t)i] = LedPixel{leds[i].r, leds[i].g, leds[i].b};
        }
        auto vars = controller_->getAllVariables();
        s.variables.reserve(vars.size());
        for (size_t i = 0; i < vars.size(); i++) {
            WatchVar w;
            w.name = toStd(vars[i].name);
            w.display = formatValue(vars[i].value);
            w.number = vars[i].value.asNumber();
            s.variables.push_back(w);
        }
        auto params = controller_->getAllParameters();
        s.parameters.reserve(params.size());
        for (size_t i = 0; i < params.size(); i++) {
            HostParameter p;
            p.name = toStd(params[i].name);
            p.type = params[i].type;
            p.value = params[i].currentValue.asNumber();
            p.minValue = params[i].minValue;
            p.maxValue = params[i].maxValue;
            p.stepValue = params[i].stepValue;
            for (size_t e = 0; e < params[i].enumValues.size(); e++) {
                p.enumValues.push_back(toStd(params[i].enumValues[e]));
            }
            s.parameters.push_back(p);
        }
    }
    return s;
}

void HostSession::applyPendingParamsLocked() {
    if (!controller_) return;
    for (size_t i = 0; i < pendingParams_.size(); i++) {
        controller_->setParameterValue(toArduino(pendingParams_[i].first),
                                       Value(pendingParams_[i].second));
    }
    pendingParams_.clear();
}

void HostSession::resetSimulationLocked() {
    simTime_ = 0;
    needSetup_ = true;
    stepMode_ = StepNone;
    cmdRun_ = false;
    cmdPause_ = false;
    cmdStepInto_ = false;
    cmdStepOver_ = false;
    cmdStepFrame_ = false;
    state_ = Snapshot::Stopped;
    if (controller_) {
        controller_->interrupt();
    }
}

bool HostSession::ensureLoadedLocked() {
    return controller_ && controller_->isProgramLoaded();
}

void HostSession::requestStopAndWait() {
    std::unique_lock<std::mutex> lock(mu_);
    cmdStop_ = true;
    cmdRun_ = false;
    cmdStepInto_ = false;
    cmdStepOver_ = false;
    cmdStepFrame_ = false;
    if (controller_) controller_->interrupt();
    cv_.notify_all();
    cv_.wait(lock, [this] { return quit_ || state_ == Snapshot::Stopped; });
    cmdStop_ = false;
}

void HostSession::workerMain() {
    std::unique_lock<std::mutex> lock(mu_);
    while (!quit_) {
        cv_.wait(lock, [this] {
            return quit_ || cmdStop_ || cmdRun_ || cmdStepInto_ || cmdStepOver_ || cmdStepFrame_;
        });
        if (quit_) break;
        if (cmdStop_) {
            resetSimulationLocked();
            cmdStop_ = false;
            publishSnapshotLocked();
            cv_.notify_all();
            continue;
        }
        if (!ensureLoadedLocked()) {
            cmdRun_ = cmdStepInto_ = cmdStepOver_ = cmdStepFrame_ = false;
            state_ = Snapshot::Stopped;
            publishSnapshotLocked();
            cv_.notify_all();
            continue;
        }

        const bool frameOnly = cmdStepFrame_;
        const bool stepping = cmdStepInto_ || cmdStepOver_;
        if (cmdStepInto_) {
            cmdStepInto_ = false;
            stepMode_ = StepInto;
        } else if (cmdStepOver_) {
            cmdStepOver_ = false;
            stepMode_ = StepOver;
            stepOverDepth_ = 1;
        } else if (cmdRun_) {
            cmdRun_ = false;
            stepMode_ = StepNone;
        }
        cmdStepFrame_ = false;
        cmdPause_ = false;
        cmdStop_ = false;

        if (needSetup_) {
            simTime_ = 0;
            applyPendingParamsLocked();
            state_ = stepping ? Snapshot::Playing : Snapshot::Playing;
            lock.unlock();
            controller_->runSetup();
            lock.lock();
            needSetup_ = false;
            publishSnapshotLocked();
            if (controller_->hasRuntimeError()) {
                state_ = Snapshot::Paused;
                stepMode_ = StepNone;
                cv_.notify_all();
                continue;
            }
            if (quit_ || cmdStop_) {
                resetSimulationLocked();
                cmdStop_ = false;
                publishSnapshotLocked();
                cv_.notify_all();
                continue;
            }
            // Statement-step during setup already paused inside the hook.
            if (state_ == Snapshot::Paused) {
                cv_.notify_all();
                continue;
            }
        }

        state_ = Snapshot::Playing;
        applyPendingParamsLocked();

        bool oneFrame = frameOnly || stepping;
        while (!quit_ && !cmdStop_) {
            applyPendingParamsLocked();
            const unsigned long t = simTime_;
            const unsigned dt = frameDtMs_;
            const float speed = speedMul_ < 0.01f ? 0.01f : speedMul_;
            lock.unlock();
            controller_->runLoop(t);
            lock.lock();

            simTime_ = t + dt;
            publishSnapshotLocked();

            if (controller_->hasRuntimeError()) {
                state_ = Snapshot::Paused;
                stepMode_ = StepNone;
                break;
            }
            if (quit_ || cmdStop_) break;
            if (state_ == Snapshot::Paused) break;
            if (oneFrame) {
                state_ = Snapshot::Paused;
                stepMode_ = StepNone;
                break;
            }
            if (cmdPause_) {
                cmdPause_ = false;
                state_ = Snapshot::Paused;
                break;
            }

            auto wait = std::chrono::duration<float, std::milli>((float)dt / speed);
            cv_.wait_for(lock, wait, [this] {
                return quit_ || cmdStop_ || cmdPause_ || cmdStepInto_ || cmdStepOver_;
            });
            if (cmdPause_) {
                cmdPause_ = false;
                state_ = Snapshot::Paused;
                break;
            }
            if (cmdStepInto_ || cmdStepOver_) {
                // Switch into statement-step on the next frame.
                if (cmdStepInto_) {
                    cmdStepInto_ = false;
                    stepMode_ = StepInto;
                } else {
                    cmdStepOver_ = false;
                    stepMode_ = StepOver;
                    stepOverDepth_ = 1;
                }
                oneFrame = true;
            }
        }

        if (cmdStop_ || quit_) {
            resetSimulationLocked();
            cmdStop_ = false;
        } else if (state_ != Snapshot::Paused) {
            state_ = Snapshot::Paused;
        }
        publishSnapshotLocked();
        cv_.notify_all();
    }
}

bool HostSession::loadSource(const std::string& source, const std::string& path) {
    requestStopAndWait();
    std::lock_guard<std::mutex> lock(mu_);
    source_ = source;
    sourcePath_ = path;
    bool ok = controller_->loadProgram(toArduino(source));
    needSetup_ = true;
    simTime_ = 0;
    state_ = Snapshot::Stopped;
    publishSnapshotLocked();
    return ok;
}

bool HostSession::loadFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        requestStopAndWait();
        std::lock_guard<std::mutex> lock(mu_);
        source_.clear();
        sourcePath_ = path;
        needSetup_ = true;
        state_ = Snapshot::Stopped;
        publishSnapshotLocked();
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return loadSource(ss.str(), path);
}

std::vector<Diagnostic> HostSession::diagnostics() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (!controller_) return {};
    return controller_->getDiagnostics();
}

bool HostSession::setLedCount(int n) {
    if (n < 1) n = 1;
    if (n > kMaxLeds) n = kMaxLeds;
    requestStopAndWait();
    std::lock_guard<std::mutex> lock(mu_);
    if ((int)leds_.size() == n) return true;
    leds_.assign((size_t)n, CRGB());
    delete controller_;
    controller_ = new BasicLEDController(leds_.data(), n);
    controller_->setOwnsPhysicalOutput(false);
    controller_->setAutoShow(false);
    controller_->setClock(clockMillis, clockDelay, this);
    controller_->setDebugHook(debugHook, this);
    bool ok = true;
    if (!source_.empty()) {
        ok = controller_->loadProgram(toArduino(source_));
    }
    needSetup_ = true;
    simTime_ = 0;
    publishSnapshotLocked();
    return ok;
}

int HostSession::ledCount() const {
    std::lock_guard<std::mutex> lock(mu_);
    return (int)leds_.size();
}

void HostSession::setFrameDtMs(unsigned dt) {
    std::lock_guard<std::mutex> lock(mu_);
    frameDtMs_ = dt == 0 ? 1 : dt;
}

unsigned HostSession::frameDtMs() const {
    std::lock_guard<std::mutex> lock(mu_);
    return frameDtMs_;
}

void HostSession::setSpeedMultiplier(float m) {
    std::lock_guard<std::mutex> lock(mu_);
    if (m < 0.01f) m = 0.01f;
    if (m > 16.0f) m = 16.0f;
    speedMul_ = m;
}

float HostSession::speedMultiplier() const {
    std::lock_guard<std::mutex> lock(mu_);
    return speedMul_;
}

void HostSession::run() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        cmdStop_ = false;
        cmdPause_ = false;
        cmdRun_ = true;
        if (state_ == Snapshot::Stopped) needSetup_ = true;
    }
    cv_.notify_all();
}

void HostSession::pause() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        cmdPause_ = true;
    }
    cv_.notify_all();
}

void HostSession::stop() {
    requestStopAndWait();
}

void HostSession::stepInto() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        cmdStop_ = false;
        cmdStepInto_ = true;
        if (state_ == Snapshot::Stopped) needSetup_ = true;
    }
    cv_.notify_all();
}

void HostSession::stepOver() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        cmdStop_ = false;
        cmdStepOver_ = true;
        if (state_ == Snapshot::Stopped) needSetup_ = true;
    }
    cv_.notify_all();
}

void HostSession::stepFrame() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        cmdStop_ = false;
        cmdStepFrame_ = true;
        if (state_ == Snapshot::Stopped) needSetup_ = true;
    }
    cv_.notify_all();
}

void HostSession::setBreakpoint(int line, bool on) {
    std::lock_guard<std::mutex> lock(mu_);
    if (on) breakpoints_.insert(line);
    else breakpoints_.erase(line);
    lastSnap_.breakpoints = breakpoints_;
}

bool HostSession::isBreakpoint(int line) const {
    std::lock_guard<std::mutex> lock(mu_);
    return breakpoints_.count(line) != 0;
}

void HostSession::clearBreakpoints() {
    std::lock_guard<std::mutex> lock(mu_);
    breakpoints_.clear();
    lastSnap_.breakpoints.clear();
}

bool HostSession::setParameter(const std::string& name, float value) {
    std::lock_guard<std::mutex> lock(mu_);
    pendingParams_.push_back({name, value});
    if (state_ != Snapshot::Playing && controller_) {
        applyPendingParamsLocked();
        publishSnapshotLocked();
    }
    return true;
}

EvalResult HostSession::evalImmediate(const std::string& text) {
    EvalResult out;
    std::string src = text;
    while (!src.empty() && (src[0] == ' ' || src[0] == '\t')) src.erase(src.begin());
    if (!src.empty() && src[0] == '?') {
        src.erase(src.begin());
        while (!src.empty() && (src[0] == ' ' || src[0] == '\t')) src.erase(src.begin());
    }

    std::lock_guard<std::mutex> lock(mu_);
    if (state_ == Snapshot::Playing) {
        out.ok = false;
        out.text = "Pause execution before using Immediate";
        return out;
    }
    if (!controller_) {
        out.ok = false;
        out.text = "No interpreter";
        return out;
    }
    Value result;
    Diagnostic err;
    if (!controller_->evalSnippet(toArduino(src), result, err)) {
        out.ok = false;
        out.error = err;
        out.text = toStd(err.message);
        return out;
    }
    out.ok = true;
    out.text = formatValue(result);
    publishSnapshotLocked();
    return out;
}

Snapshot HostSession::snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return lastSnap_;
}

bool HostSession::runFrames(int n, std::vector<std::vector<LedPixel>>* dumps) {
    if (n < 0) n = 0;
    requestStopAndWait();

    std::unique_lock<std::mutex> lock(mu_);
    if (!ensureLoadedLocked()) return false;

    controller_->setDebugHook(nullptr, nullptr);
    simTime_ = 0;
    applyPendingParamsLocked();
    lock.unlock();
    controller_->runSetup();
    lock.lock();
    if (controller_->hasRuntimeError()) {
        controller_->setDebugHook(debugHook, this);
        publishSnapshotLocked();
        return false;
    }
    if (dumps) {
        dumps->clear();
        dumps->reserve((size_t)n);
    }
    for (int i = 0; i < n; i++) {
        const unsigned long t = simTime_;
        lock.unlock();
        controller_->runLoop(t);
        lock.lock();
        simTime_ += frameDtMs_;
        if (dumps) {
            std::vector<LedPixel> frame;
            frame.resize(leds_.size());
            for (size_t p = 0; p < leds_.size(); p++) {
                frame[p] = LedPixel{leds_[p].r, leds_[p].g, leds_[p].b};
            }
            dumps->push_back(frame);
        }
        if (controller_->hasRuntimeError()) {
            controller_->setDebugHook(debugHook, this);
            publishSnapshotLocked();
            return false;
        }
    }
    controller_->setDebugHook(debugHook, this);
    state_ = Snapshot::Stopped;
    needSetup_ = true;
    publishSnapshotLocked();
    return true;
}

} // namespace ledbasic
