#include "app.h"
#include "tui_test.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

using namespace ftxui;
using ledbasic::Snapshot;
namespace fs = std::filesystem;

namespace {

constexpr int kWidth = 120;
constexpr int kHeight = 40;

const char* kNewProgram = "setup\nend\n\nloop(time)\n  show()\nend\n";

fs::path tempDir() {
    return fs::temp_directory_path() / "ledbasic_tui_tests";
}

std::string readFile(const fs::path& p) {
    std::ifstream in(p);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void writeFile(const fs::path& p, const std::string& body) {
    std::ofstream out(p);
    out << body;
}

std::unique_ptr<App> makeApp(const std::string& file = defaultProgramPath()) {
    AppOptions options;
    options.startFile = file;
    return std::make_unique<App>(options);
}

std::string frame(App& app) {
    return renderText(*app.root(), kWidth, kHeight);
}

std::string lastLog(App& app) {
    return app.log().empty() ? "" : app.log().back();
}

bool press(App& app, const Event& e) {
    return app.root()->OnEvent(e);
}

// Clicks the first (or last) place label is drawn, like a user would.
bool clickText(App& app, const std::string& label, bool last = false) {
    auto cells = renderCells(*app.root(), kWidth, kHeight);
    int x = 0, y = 0;
    if (!findText(cells, label, &x, &y, last)) {
        std::printf("  note: \"%s\" not on screen\n", label.c_str());
        return false;
    }
    press(app, click(x, y));
    return true;
}

bool menuCommand(App& app, const std::string& item) {
    return clickText(app, " File ") && clickText(app, item);
}

// Clicks into a dialog's text field (found by the start of its current
// text, since long paths are clipped), clears it and types a new value.
void fillField(App& app, const std::string& current, const std::string& value) {
    // Last match: the same text can also be in the editor's title bar.
    clickText(app, current.substr(0, 12), true);
    press(app, Event::End);
    for (size_t i = 0; i < current.size() + 8; i++) press(app, Event::Backspace);
    for (char c : value) press(app, Event::Character(c));
}

void testMenuOpensAndCloses() {
    auto app = makeApp();
    CHECK(clickText(*app, " File "), "menu: File button is drawn");
    std::string screen = frame(*app);
    CHECK(contains(screen, "Ctrl+N") && contains(screen, "Ctrl+O") && contains(screen, "Ctrl+S") &&
          contains(screen, "Save As") && contains(screen, "Ctrl+Q"),
          "menu: lists New, Open, Save, Save As, Exit");
    press(*app, Event::Escape);
    CHECK(!contains(frame(*app), "Ctrl+N"), "menu: Esc closes it");
    CHECK(app->session().snapshot().state == Snapshot::Stopped, "menu: Esc on a menu does not stop/run");
}

void testNew() {
    auto app = makeApp();
    app->editor().goToLine(13);
    press(*app, Event::F9);
    press(*app, Event::F5);
    waitFor([&] { return app->session().snapshot().state != Snapshot::Stopped; });

    CHECK(menuCommand(*app, "New "), "new: menu item clicked");
    CHECK(app->editor().getText() == kNewProgram, "new: editor holds the starter program");
    CHECK(app->filePath().empty(), "new: no file path");
    CHECK(!app->editor().dirty(), "new: buffer is clean");
    CHECK(app->session().snapshot().state == Snapshot::Stopped, "new: running program is stopped");
    CHECK(app->session().source() == kNewProgram, "new: session loaded the starter program");
    CHECK(lastLog(*app) == "new program", "new: logged");
    std::string screen = frame(*app);
    CHECK(contains(screen, " untitled.bas "), "new: title is untitled.bas");
    CHECK(!contains(screen, "Ctrl+N"), "new: menu closes");
}

void testOpenExample() {
    auto app = makeApp();
    fs::path breathing = fs::path(examplesDir()) / "Breathing.bas";
    app->editor().goToLine(3);
    press(*app, Event::F9);

    CHECK(menuCommand(*app, "Open"), "open: menu item clicked");
    std::string screen = frame(*app);
    CHECK(contains(screen, "Aurora.bas") && contains(screen, "Breathing.bas"),
          "open: dialog lists the examples");
    CHECK(contains(screen, examplesDir().substr(0, 20)), "open: path field starts in the examples folder");
    CHECK(contains(screen, " Cancel "), "open: buttons fit on screen");

    CHECK(clickText(*app, "Breathing.bas"), "open: example clicked");
    CHECK(fs::equivalent(app->filePath(), breathing), "open: file path is the example");
    CHECK(app->editor().getText() == readFile(breathing), "open: editor shows the file");
    CHECK(app->session().source() == readFile(breathing), "open: session loaded the file");
    CHECK(app->editor().breakpoints().empty(), "open: old breakpoints are cleared");
    CHECK(!app->editor().dirty(), "open: buffer is clean");
    CHECK(!contains(frame(*app), "Rainbow.bas"), "open: dialog closes");
    CHECK(contains(lastLog(*app), "opened"), "open: logged");
}

// The example list is longer than the dialog; it must scroll to reach the end.
void testOpenScrollsExampleList() {
    auto app = makeApp();
    press(*app, Event::CtrlO);
    CHECK(!contains(frame(*app), "Wipe.bas"), "open scroll: last example starts off screen");

    int x = 0, y = 0;
    findText(renderCells(*app->root(), kWidth, kHeight), "Aurora.bas", &x, &y);
    Mouse wheel;
    wheel.button = Mouse::WheelDown;
    wheel.motion = Mouse::Pressed;
    wheel.x = x;
    wheel.y = y;
    for (int i = 0; i < 100; i++) press(*app, Event::Mouse("", wheel));
    CHECK(contains(frame(*app), "Wipe.bas"), "open scroll: wheel scrolls to the last example");
}

// The list has focus when the dialog opens, so it works from the keyboard.
void testOpenFromKeyboard() {
    auto app = makeApp();
    press(*app, Event::CtrlO);
    press(*app, Event::ArrowDown);
    press(*app, Event::Return);
    CHECK(contains(app->filePath(), "BPM.bas"), "open keys: Down, Return opens the second example");

    press(*app, Event::CtrlO);
    press(*app, Event::End);
    CHECK(contains(frame(*app), "Wipe.bas"), "open keys: End scrolls to the last example");
    press(*app, Event::Return);
    CHECK(contains(app->filePath(), "Wipe.bas"), "open keys: Return opens it");
}

void testOpenTypedPath() {
    fs::path file = tempDir() / "typed.bas";
    writeFile(file, "setup\n  clear()\nend\n\nloop(time)\n  show()\nend\n");

    auto app = makeApp();
    press(*app, Event::CtrlO);
    fillField(*app, examplesDir() + "/", file.generic_string());
    CHECK(clickText(*app, " Open ", true), "open typed: Open button clicked");
    CHECK(fs::equivalent(app->filePath(), file), "open typed: file path set");
    CHECK(app->editor().getText() == readFile(file), "open typed: editor shows the file");
}

void testOpenMissingFile() {
    auto app = makeApp();
    std::string before = app->editor().getText();
    std::string path = app->filePath();
    press(*app, Event::CtrlO);
    fillField(*app, examplesDir() + "/", (tempDir() / "nope.bas").generic_string());
    clickText(*app, " Open ", true);
    CHECK(contains(lastLog(*app), "could not open"), "open missing: logged");
    CHECK(app->editor().getText() == before, "open missing: editor unchanged");
    CHECK(app->filePath() == path, "open missing: file path unchanged");
}

void testOpenFileWithError() {
    fs::path file = tempDir() / "broken.bas";
    writeFile(file, "setup\n  clear(\nend\n");
    auto app = makeApp();
    press(*app, Event::CtrlO);
    fillField(*app, examplesDir() + "/", file.generic_string());
    clickText(*app, " Open ", true);
    CHECK(app->editor().getText() == readFile(file), "open broken: editor still shows the file");
    CHECK(contains(lastLog(*app), "error"), "open broken: load error logged");
}

void testOpenCancel() {
    auto app = makeApp();
    std::string path = app->filePath();
    press(*app, Event::CtrlO);
    CHECK(clickText(*app, " Cancel "), "open cancel: Cancel clicked");
    CHECK(!contains(frame(*app), "Breathing.bas"), "open cancel: dialog closes");
    CHECK(app->filePath() == path, "open cancel: file unchanged");
}

void testSaveUntitledPromptsForPath() {
    auto app = makeApp();
    press(*app, Event::CtrlN);
    press(*app, Event::CtrlS);
    std::string screen = frame(*app);
    CHECK(contains(screen, " Save as"), "save untitled: Ctrl+S opens Save As");
    CHECK(contains(screen, "untitled.bas"), "save untitled: path defaults to untitled.bas");
    CHECK(clickText(*app, " Cancel "), "save untitled: Cancel clicked");
    CHECK(!contains(frame(*app), " Save as"), "save untitled: Cancel closes the dialog");
    CHECK(app->filePath().empty(), "save untitled: Cancel saves nothing");
}

void testSaveAs() {
    fs::path file = tempDir() / "saved2.bas";
    auto app = makeApp("");
    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    CHECK(contains(frame(*app), " *"), "save as: title marks the dirty buffer");

    CHECK(menuCommand(*app, "Save As"), "save as: menu item clicked");
    fillField(*app, "untitled.bas", file.generic_string());
    CHECK(clickText(*app, " Save ", true), "save as: Save clicked");
    CHECK(fs::exists(file), "save as: file written");
    CHECK(readFile(file) == app->editor().getText(), "save as: file holds the buffer");
    CHECK(app->filePath() == file.generic_string(), "save as: file path updated");
    CHECK(!app->editor().dirty(), "save as: buffer is clean");
    std::string screen = frame(*app);
    CHECK(contains(screen, "saved2.bas") && !contains(screen, "saved2.bas *"),
          "save as: title shows the new name without *");
    CHECK(app->speed() == 1.0f, "save as: digits typed in the path do not change speed");
}

void testSaveExisting() {
    fs::path file = tempDir() / "existing.bas";
    writeFile(file, "setup\nend\n\nloop(time)\n  show()\nend\n");
    auto app = makeApp(file.string());
    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    CHECK(menuCommand(*app, "Save   "), "save: menu item clicked");
    CHECK(readFile(file) == app->editor().getText(), "save: file rewritten with the buffer");
    CHECK(readFile(file)[0] == '#', "save: edit is on disk");
    CHECK(!app->editor().dirty(), "save: buffer is clean");
    CHECK(!contains(frame(*app), " Save as"), "save: no prompt for a named file");

    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    press(*app, Event::CtrlS);
    CHECK(readFile(file).rfind("##", 0) == 0, "save: Ctrl+S writes too");
}

void testSaveFails() {
    fs::path bad = tempDir() / "no_such_dir" / "x.bas";
    auto app = makeApp("");
    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    press(*app, Event::CtrlS);
    fillField(*app, "untitled.bas", bad.generic_string());
    clickText(*app, " Save ", true);
    CHECK(!fs::exists(bad), "save fails: nothing written");
    CHECK(contains(lastLog(*app), "could not write"), "save fails: logged");
    CHECK(app->filePath().empty(), "save fails: file path unchanged");
    CHECK(app->editor().dirty(), "save fails: buffer still dirty");
}

void testExit() {
    auto app = makeApp();
    bool exited = false;
    app->onExit = [&] { exited = true; };
    CHECK(menuCommand(*app, "Exit"), "exit: menu item clicked");
    CHECK(exited, "exit: calls onExit");
}

} // namespace

void runFileMenuTests() {
    std::error_code ec;
    fs::remove_all(tempDir(), ec);
    fs::create_directories(tempDir());

    testMenuOpensAndCloses();
    testNew();
    testOpenExample();
    testOpenScrollsExampleList();
    testOpenFromKeyboard();
    testOpenTypedPath();
    testOpenMissingFile();
    testOpenFileWithError();
    testOpenCancel();
    testSaveUntitledPromptsForPath();
    testSaveAs();
    testSaveExisting();
    testSaveFails();
    testExit();

    fs::remove_all(tempDir(), ec);
}
