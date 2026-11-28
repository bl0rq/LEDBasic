#include "app.h"
#include "tui_test.h"

#include <fstream>
#include <iterator>
#include <memory>

using namespace ftxui;
using ledbasic::Snapshot;

namespace {

constexpr int kWidth = 120;
constexpr int kHeight = 40;

// Rainbow.bas: line 13 is the sethsv call inside the for loop.
constexpr int kSethsvLine = 13;

std::unique_ptr<App> makeApp(const std::string& file = defaultProgramPath()) {
    AppOptions options;
    options.startFile = file;
    return std::make_unique<App>(options);
}

std::string frame(App& app) {
    return renderText(*app.root(), kWidth, kHeight);
}

bool press(App& app, const Event& e) {
    return app.root()->OnEvent(e);
}

Snapshot::State state(App& app) {
    return app.session().snapshot().state;
}

// Runs a command in the Immediate pane and returns the last log line.
std::string immediate(App& app, const std::string& cmd) {
    press(app, Event::F4);
    for (char c : cmd) press(app, Event::Character(c));
    press(app, Event::Return);
    return app.log().empty() ? "" : app.log().back();
}

bool pausedOn(App& app, int line) {
    return waitFor([&] {
        auto s = app.session().snapshot();
        return s.state == Snapshot::Paused && s.currentLine == line;
    });
}

// Waits for a breakpoint hit on line with loop index i == value. After F5
// from a pause, run() only queues the command, so the old snapshot can still
// be Paused on the same line for a moment; pausedOn() alone would return on
// that stale hit.
bool pausedOnWithI(App& app, int line, float value) {
    return waitFor([&] {
        auto s = app.session().snapshot();
        if (s.state != Snapshot::Paused || s.currentLine != line) return false;
        for (const auto& v : s.variables) {
            if (v.name == "i") return v.number == value;
        }
        return false;
    });
}

bool anyLedLit(const Snapshot& s) {
    for (const auto& p : s.leds) {
        if (p.r || p.g || p.b) return true;
    }
    return false;
}

float paramValue(App& app, const std::string& name) {
    for (const auto& p : app.session().snapshot().parameters) {
        if (p.name == name) return p.value;
    }
    return -1;
}

// tools/README.md "Manual TUI checklist", step for step.
void testManualChecklist() {
    auto app = makeApp();

    // 1. Opens examples/bas/Rainbow.bas by default.
    std::string screen = frame(*app);
    CHECK(contains(app->filePath(), "Rainbow.bas"), "1: Rainbow.bas is the default file");
    CHECK(contains(screen, "Rainbow.bas"), "1: title shows Rainbow.bas");
    CHECK(contains(screen, "param speed"), "1: editor shows the program");
    CHECK(contains(screen, "STOP"), "1: simulator starts stopped");

    // 2. F5 animates the LEDs.
    press(*app, Event::F5);
    CHECK(waitFor([&] { return state(*app) == Snapshot::Playing; }), "2: F5 starts playing");
    unsigned long t0 = app->session().snapshot().timeMs;
    CHECK(waitFor([&] { return app->session().snapshot().timeMs > t0 + 50; }), "2: sim time advances");
    CHECK(anyLedLit(app->session().snapshot()), "2: LEDs are lit");
    screen = frame(*app);
    CHECK(contains(screen, "PLAY"), "2: strip header shows PLAY");
    CHECK(app->editor().readOnly(), "2: editor is read-only while playing");

    // 3. Esc stops; F9 on a sethsv line, F5 hits the breakpoint.
    press(*app, Event::Escape);
    CHECK(state(*app) == Snapshot::Stopped, "3: Esc stops");
    app->editor().goToLine(kSethsvLine);
    press(*app, Event::F9);
    CHECK(app->session().isBreakpoint(kSethsvLine), "3: F9 sets a session breakpoint");
    press(*app, Event::F5);
    CHECK(waitFor([&] { return state(*app) == Snapshot::Paused; }), "3: run pauses");
    CHECK(app->session().snapshot().currentLine == kSethsvLine, "3: paused on the breakpoint line");
    screen = frame(*app);
    CHECK(contains(screen, "PAUSE"), "3: strip header shows PAUSE");

    // 4. Immediate "? i" prints the loop index.
    press(*app, Event::F4);
    CHECK(app->immediateInput()->Focused(), "4: F4 focuses Immediate");
    for (char c : std::string("? i")) press(*app, Event::Character(c));
    press(*app, Event::Return);
    const auto& log = app->log();
    CHECK(log.size() >= 2 && contains(log[log.size() - 2], "? i"), "4: command echoed to the log");
    CHECK(!log.empty() && log.back() == "0", "4: first hit prints i = 0");

    // 5. F8 advances one statement.
    int before = app->session().snapshot().currentLine;
    press(*app, Event::F8);
    CHECK(waitFor([&] {
        auto s = app->session().snapshot();
        return s.state == Snapshot::Paused && s.currentLine != before;
    }), "5: F8 pauses on a different line");

    // F5 while paused continues instead of restarting: the next hit is i = 1.
    press(*app, Event::F5);
    CHECK(pausedOnWithI(*app, kSethsvLine, 1), "5: F5 runs on to the breakpoint again");
    CHECK(immediate(*app, "? i") == "1", "5: F5 continued the loop rather than restarting");

    // 6. Params list: Left/Right adjust the effect.
    press(*app, Event::Escape);
    app->paramsFocus()->TakeFocus();
    frame(*app);
    // Params list in declaration order: speed, saturation, brightness_level.
    CHECK(paramValue(*app, "speed") == 20.0f, "6: speed starts at 20");
    press(*app, Event::ArrowRight);
    CHECK(paramValue(*app, "speed") == 21.0f, "6: Right steps the first param (speed) up by 1");
    CHECK(contains(frame(*app), "> speed = 21"), "6: params list shows the new value");
    press(*app, Event::ArrowDown);
    CHECK(app->paramIndex() == 1, "6: Down selects the next param");
    press(*app, Event::ArrowLeft);
    CHECK(paramValue(*app, "saturation") == 250.0f, "6: Left steps saturation down by 5");
    press(*app, Event::ArrowRight);
    press(*app, Event::ArrowRight);
    CHECK(paramValue(*app, "saturation") == 255.0f, "6: param clamps at its max");

    // 7. F1 opens the help overlay; Esc closes it without stopping anything.
    press(*app, Event::F1);
    screen = frame(*app);
    CHECK(app->helpVisible(), "7: F1 shows help");
    CHECK(contains(screen, "LEDBasic TUI"), "7: help overlay renders");
    press(*app, Event::Escape);
    CHECK(!app->helpVisible(), "7: Esc closes help");
}

void testF5AfterEditRestarts() {
    auto app = makeApp();
    app->editor().goToLine(kSethsvLine);
    press(*app, Event::F9);
    press(*app, Event::F5);
    CHECK(pausedOn(*app, kSethsvLine), "edit+F5: paused on breakpoint");
    press(*app, Event::F5);
    CHECK(pausedOnWithI(*app, kSethsvLine, 1), "edit+F5: second hit");
    CHECK(immediate(*app, "? i") == "1", "edit+F5: continued to i = 1");

    app->editor().setText(app->editor().getText() + "\n");
    press(*app, Event::F5);
    CHECK(pausedOnWithI(*app, kSethsvLine, 0), "edit+F5: paused after reload");
    CHECK(immediate(*app, "? i") == "0", "edit+F5: edited source restarts from i = 0");
}

void testLoadErrorReported() {
    auto app = makeApp("");
    app->editor().setText("setup\n  clear(\nend\n");
    press(*app, Event::F5);
    CHECK(state(*app) == Snapshot::Stopped, "load error: does not run");
    CHECK(!app->log().empty() && contains(app->log().back(), "error 3"),
          "load error: logged with the line the parser stopped on");
}

void testNewAndExit() {
    auto app = makeApp();
    press(*app, Event::CtrlN);
    CHECK(app->filePath().empty(), "Ctrl+N clears the file path");
    CHECK(contains(frame(*app), "untitled.bas"), "Ctrl+N titles the buffer untitled.bas");

    bool exited = false;
    app->onExit = [&] { exited = true; };
    press(*app, Event::CtrlQ);
    CHECK(exited, "Ctrl+Q calls onExit");
}

void testSpeedAndLedKeys() {
    auto app = makeApp();
    app->paramsFocus()->TakeFocus();
    press(*app, Event::Character('3'));
    CHECK(app->speed() == 2.0f, "'3' sets 2x speed");
    CHECK(contains(frame(*app), "2.00x"), "strip header shows the speed");

    int n = app->session().ledCount();
    press(*app, Event::Character('['));
    CHECK(app->session().ledCount() == n / 2, "'[' halves the LED count");
    press(*app, Event::Character(']'));
    CHECK(app->session().ledCount() == n, "']' doubles the LED count");

    app->editor().TakeFocus();
    press(*app, Event::Character('4'));
    CHECK(app->speed() == 2.0f, "speed keys are ignored while the editor has focus");
}

} // namespace

// Device > Programs > Open loads the board's text as is, which may be CRLF.
// Starting from Stopped reloads from the editor (which drops CRs), so F5 at
// the breakpoint must still continue rather than restart.
void testF5ContinuesWithCrlfSource() {
    std::ifstream in(defaultProgramPath(), std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string crlf;
    for (char c : body) {
        if (c == '\r') continue;
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    auto app = makeApp("");
    app->editor().setText(crlf);
    app->session().loadSource(crlf, "");
    app->editor().goToLine(kSethsvLine);
    press(*app, Event::F9);
    press(*app, Event::F5);
    CHECK(pausedOnWithI(*app, kSethsvLine, 0), "crlf: first hit");
    press(*app, Event::F5);
    CHECK(pausedOnWithI(*app, kSethsvLine, 1), "crlf: F5 continues instead of restarting");
    app->session().stop();
}

void testModalStackingAndNewCleanup() {
    auto app = makeApp();
    app->editor().goToLine(kSethsvLine);
    press(*app, Event::F9);
    CHECK(app->session().isBreakpoint(kSethsvLine), "cleanup: breakpoint set before New");
    press(*app, Event::CtrlN);
    CHECK(app->editor().breakpoints().empty(), "cleanup: Ctrl+N clears editor breakpoints");
    CHECK(!app->session().isBreakpoint(kSethsvLine), "cleanup: Ctrl+N clears session breakpoints");

    // Global shortcuts must not stack another modal under Help or break Esc.
    press(*app, Event::F1);
    CHECK(app->helpVisible(), "modal: F1 opens Help");
    press(*app, Event::CtrlO);
    CHECK(!app->openVisible() && app->helpVisible(), "modal: Ctrl+O does not stack Open under Help");
    press(*app, Event::Escape);
    CHECK(!app->helpVisible(), "modal: Esc still dismisses Help");
}

void testParamIndexClampedAndClickable() {
    auto app = makeApp();
    // Click Params header to focus it.
    auto cells = renderCells(*app->root(), kWidth, kHeight);
    int x = 0, y = 0;
    CHECK(findText(cells, "Params", &x, &y), "params: header is rendered");
    press(*app, click(x, y));
    CHECK(app->paramsFocus()->Focused(), "params: clicking Params focuses it");

    // Move to the 3rd param (index 2), then load a 1-param program and press ArrowRight
    // *without* rendering first — must clamp paramIndex_ instead of reading out of bounds.
    press(*app, Event::ArrowDown);
    press(*app, Event::ArrowDown);
    CHECK(app->paramIndex() == 2, "params: moved to index 2");
    app->session().loadSource(
        "param gain number(5.0, 0.0, 10.0, 1.0)\nsetup\nend\nloop(time)\n  show()\nend\n", "");
    press(*app, Event::ArrowRight);
    CHECK(app->paramIndex() == 0, "params: clamped to last valid index before indexing");
    CHECK(paramValue(*app, "gain") == 6.0f, "params: adjusted clamped param cleanly");
}

void runAppTests() {
    testManualChecklist();
    testF5AfterEditRestarts();
    testF5ContinuesWithCrlfSource();
    testLoadErrorReported();
    testNewAndExit();
    testSpeedAndLedKeys();
    testModalStackingAndNewCleanup();
    testParamIndexClampedAndClickable();
}
