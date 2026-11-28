#include "app.h"
#include "tui_test.h"
#include "Utf8Path.h"

#include <cstdlib>

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

FileBrowserModel& openModel(App& app) {
    return app.openBrowser().model();
}

std::string selectedName(FileBrowser& b) {
    const FileEntry* e = b.model().selectedEntry();
    return e ? e->name : "";
}

void typeKeys(App& app, const std::string& s) {
    for (char c : s) press(app, Event::Character(c));
}

// Ctrl+L, replace the prefilled folder with path, Enter.
void goTo(App& app, const fs::path& path) {
    press(app, Event::CtrlL);
    for (int i = 0; i < 300; i++) press(app, Event::Backspace);
    typeKeys(app, path.generic_string());
    press(app, Event::Return);
}

void clearName(App& app) {
    for (int i = 0; i < 40; i++) press(app, Event::Backspace);
}

std::string norm(const fs::path& p) {
    return FileBrowserModel::normalize(ledbasic::genericUtf8FromPath(p));
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

// Mouse: first click selects and previews, a second click opens.
void testOpenExampleWithMouse() {
    auto app = makeApp();
    fs::path breathing = ledbasic::pathFromUtf8(examplesDir()) / "Breathing.bas";
    app->editor().goToLine(3);
    press(*app, Event::F9);

    CHECK(menuCommand(*app, "Open"), "open: menu item clicked");
    CHECK(app->openVisible(), "open: browser shown");
    CHECK(openModel(*app).cwd() == norm(examplesDir()), "open: starts in the bas folder");
    CHECK(selectedName(app->openBrowser()) == "Rainbow.bas", "open: cursor on the open file");
    std::string screen = frame(*app);
    CHECK(contains(screen, "examples/bas") && contains(screen, "items"), "open: header shows the folder");
    CHECK(!contains(screen, "Aurora.bas"), "open: list scrolled down to the cursor");
    press(*app, Event::Character('g'));
    CHECK(contains(frame(*app), "Breathing.bas"), "open: g jumps to the top of the list");

    CHECK(clickText(*app, "Breathing.bas"), "open: example clicked once");
    CHECK(app->openVisible(), "open: one click on a file only selects it");
    CHECK(contains(frame(*app), "param intensity"), "open: preview shows the selected file");
    CHECK(clickText(*app, "Breathing.bas"), "open: example clicked again");
    CHECK(!app->openVisible(), "open: second click opens and closes the browser");
    CHECK(fs::equivalent(ledbasic::pathFromUtf8(app->filePath()), breathing), "open: file path is the example");
    CHECK(app->editor().getText() == readFile(breathing), "open: editor shows the file");
    CHECK(app->session().source() == readFile(breathing), "open: session loaded the file");
    CHECK(app->editor().breakpoints().empty(), "open: old breakpoints are cleared");
    CHECK(!app->editor().dirty(), "open: buffer is clean");
    CHECK(contains(lastLog(*app), "opened"), "open: logged");
}

void testOpenScrollsWithWheel() {
    auto app = makeApp();
    press(*app, Event::CtrlO);
    press(*app, Event::Character('g'));
    CHECK(!contains(frame(*app), "Wipe.bas"), "open scroll: last example starts off screen");
    int x = 0, y = 0;
    findText(renderCells(*app->root(), kWidth, kHeight), "Aurora.bas", &x, &y);
    Mouse wheel;
    wheel.button = Mouse::WheelDown;
    wheel.motion = Mouse::Pressed;
    wheel.x = x;
    wheel.y = y;
    for (int i = 0; i < 40; i++) press(*app, Event::Mouse("", wheel));
    CHECK(contains(frame(*app), "Wipe.bas"), "open scroll: wheel scrolls to the last example");
}

void testOpenFromKeyboard() {
    auto app = makeApp();
    press(*app, Event::CtrlO);
    press(*app, Event::Character('g'));
    press(*app, Event::Character('j'));
    std::string second = openModel(*app).entries()[1].name;
    CHECK(selectedName(app->openBrowser()) == second, "open keys: g then j selects the second entry");
    press(*app, Event::Return);
    CHECK(contains(app->filePath(), second), "open keys: Return opens it");

    press(*app, Event::CtrlO);
    CHECK(selectedName(app->openBrowser()) == second, "open keys: reopening remembers the file");
    press(*app, Event::Character('G'));
    CHECK(contains(frame(*app), "Wipe.bas"), "open keys: G scrolls to the last example");
    press(*app, Event::Character('l'));
    CHECK(contains(app->filePath(), "Wipe.bas"), "open keys: l opens it");
}

void testOpenFilter() {
    auto app = makeApp();
    press(*app, Event::CtrlO);
    press(*app, Event::Character('/'));
    typeKeys(*app, "zzz");
    CHECK(contains(frame(*app), "(no match)"), "filter: no match shown");
    press(*app, Event::Escape);
    CHECK(app->openVisible() && openModel(*app).filter().empty(), "filter: Esc clears the filter first");

    press(*app, Event::Character('/'));
    typeKeys(*app, "BREATH");
    CHECK(openModel(*app).entries().size() == 1, "filter: incremental, case-insensitive");
    press(*app, Event::Return);
    CHECK(app->openVisible() && openModel(*app).filter() == "BREATH", "filter: Enter keeps it");
    press(*app, Event::Return);
    CHECK(contains(app->filePath(), "Breathing.bas"), "filter: Enter opens the match");

    press(*app, Event::CtrlO);
    press(*app, Event::Escape);
    CHECK(!app->openVisible(), "filter: Esc with no filter closes the browser");
}

// tempDir()/proj/{sub/inside.bas, top.bas}: browse into another folder,
// and the next Open and Save As start where we left off.
void testBrowseAndRemember() {
    fs::path proj = tempDir() / "proj";
    fs::create_directories(proj / "sub");
    writeFile(proj / "sub" / "inside.bas", "setup\nend\n\nloop(time)\n  show()\nend\n");
    writeFile(proj / "top.bas", "setup\nend\n");

    auto app = makeApp();
    press(*app, Event::CtrlO);
    goTo(*app, proj);
    CHECK(openModel(*app).cwd() == norm(proj), "browse: Ctrl+L goes to a folder");
    CHECK(selectedName(app->openBrowser()) == "sub", "browse: folders listed first");
    CHECK(contains(frame(*app), "inside.bas"), "browse: preview lists the folder");
    press(*app, Event::Character('l'));
    CHECK(openModel(*app).cwd() == norm(proj / "sub"), "browse: l enters the folder");
    CHECK(app->browseDir() == norm(proj / "sub"), "browse: location remembered while browsing");
    press(*app, Event::Return);
    CHECK(app->filePath() == norm(proj / "sub" / "inside.bas"), "browse: opened a file in another folder");

    press(*app, Event::CtrlO);
    CHECK(openModel(*app).cwd() == norm(proj / "sub"), "remember: Open starts in the last folder");
    CHECK(selectedName(app->openBrowser()) == "inside.bas", "remember: cursor on the last file");
    press(*app, Event::Character('h'));
    CHECK(openModel(*app).cwd() == norm(proj), "remember: h goes up");
    CHECK(selectedName(app->openBrowser()) == "sub", "remember: h re-selects the folder");
    press(*app, Event::Escape);
    press(*app, Event::CtrlO);
    CHECK(openModel(*app).cwd() == norm(proj), "remember: cancelled browsing is remembered too");
    press(*app, Event::Escape);

    CHECK(menuCommand(*app, "Save As"), "remember: Save As clicked");
    CHECK(app->saveBrowser().model().cwd() == norm(proj), "remember: Save As starts there as well");
    CHECK(app->saveBrowser().name() == "inside.bas", "remember: Save As suggests the current name");
    press(*app, Event::Escape);

    press(*app, Event::CtrlO);
    press(*app, Event::Character('b'));
    CHECK(openModel(*app).cwd() == norm(examplesDir()), "browse: b returns to the bas folder");
}

void testOpenTypedPath() {
    fs::path file = tempDir() / "typed.bas";
    writeFile(file, "setup\n  clear()\nend\n\nloop(time)\n  show()\nend\n");
    auto app = makeApp();
    press(*app, Event::CtrlO);
    goTo(*app, file);
    CHECK(app->filePath() == norm(file), "open typed: Ctrl+L to a file opens it");
    CHECK(app->editor().getText() == readFile(file), "open typed: editor shows the file");
}

void testOpenProblems() {
    auto app = makeApp();
    std::string before = app->editor().getText();
    std::string path = app->filePath();

    press(*app, Event::CtrlO);
    goTo(*app, tempDir() / "nope.bas");
    CHECK(contains(app->openBrowser().status(), "not found"), "open missing: reported in the browser");
    CHECK(contains(frame(*app), "not found"), "open missing: shown in the footer");
    CHECK(app->openVisible(), "open missing: browser stays open");

    fs::path bin = tempDir() / "blob.dat";
    writeFile(bin, std::string("ab\0cd", 5));
    goTo(*app, bin);
    CHECK(app->openBrowser().status() == "not a text file", "open binary: refused");
    CHECK(app->editor().getText() == before && app->filePath() == path, "open problems: editor unchanged");

    fs::path broken = tempDir() / "broken.bas";
    writeFile(broken, "setup\n  clear(\nend\n");
    goTo(*app, broken);
    CHECK(app->editor().getText() == readFile(broken), "open broken: editor still shows the file");
    CHECK(contains(lastLog(*app), "error"), "open broken: load error logged");
}

void testSaveUntitledPromptsForName() {
    auto app = makeApp();
    press(*app, Event::CtrlN);
    press(*app, Event::CtrlS);
    CHECK(app->saveVisible(), "save untitled: Ctrl+S opens Save As");
    CHECK(app->saveBrowser().model().cwd() == norm(examplesDir()), "save untitled: starts in the bas folder");
    CHECK(contains(frame(*app), "Name: untitled.bas"), "save untitled: name defaults to untitled.bas");
    press(*app, Event::Escape);
    CHECK(!app->saveVisible(), "save untitled: Esc closes Save As");
    CHECK(app->filePath().empty(), "save untitled: Esc saves nothing");
}

void testSaveAs() {
    auto app = makeApp("");
    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    CHECK(contains(frame(*app), " *"), "save as: title marks the dirty buffer");

    CHECK(menuCommand(*app, "Save As"), "save as: menu item clicked");
    goTo(*app, tempDir());
    CHECK(app->saveBrowser().model().cwd() == norm(tempDir()), "save as: Ctrl+L picks the folder");
    clearName(*app);
    typeKeys(*app, "saved2");
    press(*app, Event::Return);
    fs::path file = tempDir() / "saved2.bas";
    CHECK(fs::exists(file), "save as: file written with .bas added");
    CHECK(readFile(file) == app->editor().getText(), "save as: file holds the buffer");
    CHECK(app->filePath() == norm(file), "save as: file path updated");
    CHECK(!app->editor().dirty(), "save as: buffer is clean");
    CHECK(!app->saveVisible(), "save as: dialog closes");
    CHECK(app->browseDir() == norm(tempDir()), "save as: location remembered");
    std::string screen = frame(*app);
    CHECK(contains(screen, "saved2.bas") && !contains(screen, "saved2.bas *"),
          "save as: title shows the new name without *");
    CHECK(app->speed() == 1.0f, "save as: digits typed in the name do not change speed");
}

void testSaveAsOverwrite() {
    fs::path other = tempDir() / "other.bas";
    writeFile(other, "old");
    auto app = makeApp("");
    press(*app, Event::CtrlS);
    goTo(*app, tempDir());

    press(*app, Event::Tab);
    CHECK(clickText(*app, "other.bas") && clickText(*app, "other.bas"), "overwrite: file clicked twice");
    CHECK(app->saveBrowser().name() == "other.bas", "overwrite: picking a file copies its name");
    press(*app, Event::Return);
    CHECK(contains(frame(*app), "Overwrite other.bas? y/n"), "overwrite: asks first");
    CHECK(readFile(other) == "old", "overwrite: nothing written yet");
    // Mouse clicks in the list must be ignored while the overwrite prompt is open.
    clickText(*app, "other.bas");
    CHECK(contains(frame(*app), "Overwrite other.bas? y/n"), "overwrite: mouse click does not dismiss prompt");
    press(*app, Event::Character('n'));
    CHECK(app->saveVisible() && readFile(other) == "old", "overwrite: n keeps the old file");

    press(*app, Event::Return);
    press(*app, Event::Character('y'));
    CHECK(readFile(other) == app->editor().getText(), "overwrite: y replaces it");
    CHECK(!app->saveVisible(), "overwrite: dialog closes");

    // Reject path separators and folder names without mutating Name to folder.bas.
    fs::create_directories(tempDir() / "subdir");
    auto app2 = makeApp("");
    press(*app2, Event::CtrlS);
    goTo(*app2, tempDir());
    clearName(*app2);
    typeKeys(*app2, "../escape");
    press(*app2, Event::Return);
    CHECK(app2->saveVisible() && contains(app2->saveBrowser().status(), "file name"),
          "save validation: rejects path separators in Name");
    clearName(*app2);
    typeKeys(*app2, "subdir");
    press(*app2, Event::Return);
    CHECK(app2->saveVisible() && app2->saveBrowser().name() == "subdir" &&
          contains(app2->saveBrowser().status(), "folder"),
          "save validation: rejects folder name without appending .bas");
    press(*app2, Event::Escape);
}

void testSaveAsSameFileNoPrompt() {
    fs::path file = tempDir() / "mine.bas";
    writeFile(file, "setup\nend\n");
    auto app = makeApp(ledbasic::genericUtf8FromPath(file));
    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    CHECK(menuCommand(*app, "Save As"), "same file: Save As clicked");
    goTo(*app, tempDir());
    press(*app, Event::Return);
    CHECK(!app->saveVisible(), "same file: saving over the open file does not ask");
    CHECK(readFile(file)[0] == '#', "same file: written");
}

void testSaveExisting() {
    fs::path file = tempDir() / "existing.bas";
    writeFile(file, "setup\nend\n\nloop(time)\n  show()\nend\n");
    auto app = makeApp(ledbasic::genericUtf8FromPath(file));
    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    CHECK(menuCommand(*app, "Save   "), "save: menu item clicked");
    CHECK(readFile(file) == app->editor().getText(), "save: file rewritten with the buffer");
    CHECK(readFile(file)[0] == '#', "save: edit is on disk");
    CHECK(!app->editor().dirty(), "save: buffer is clean");
    CHECK(!app->saveVisible(), "save: no prompt for a named file");

    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    press(*app, Event::CtrlS);
    CHECK(readFile(file).rfind("##", 0) == 0, "save: Ctrl+S writes too");
}

void testSaveFails() {
#ifdef _WIN32
    // '?' is not allowed in Windows file names, so the write fails.
    auto app = makeApp("");
    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    press(*app, Event::CtrlS);
    goTo(*app, tempDir());
    clearName(*app);
    typeKeys(*app, "bad?name");
    press(*app, Event::Return);
    CHECK(contains(app->saveBrowser().status(), "could not write"), "save fails: shown in the browser");
    CHECK(app->saveVisible(), "save fails: browser stays open");
    CHECK(app->filePath().empty(), "save fails: file path unchanged");
    CHECK(app->editor().dirty(), "save fails: buffer still dirty");
#endif
}

void setHomeUtf8(const std::string& utf8) {
#ifdef _WIN32
    _wputenv_s(L"USERPROFILE", ledbasic::pathFromUtf8(utf8).c_str());
#else
    setenv("HOME", utf8.c_str(), 1);
#endif
}

void testUnicodePaths() {
    const std::string cafe = "caf\xC3\xA9";            // café
    const std::string naive = "na\xC3\xAFve.bas";      // naïve.bas
    fs::path dir = tempDir() / ledbasic::pathFromUtf8(cafe);
    fs::create_directories(dir);
    std::ofstream(dir / ledbasic::pathFromUtf8(naive)) << kNewProgram;
    std::string dirUtf8 = FileBrowserModel::normalize(ledbasic::genericUtf8FromPath(dir));

    // Open by typed path: the UTF-8 path must reach the filesystem intact.
    auto app = makeApp("");
    press(*app, Event::CtrlO);
    press(*app, Event::CtrlL);
    for (int i = 0; i < 300; i++) press(*app, Event::Backspace);
    typeKeys(*app, dirUtf8 + "/" + naive);
    press(*app, Event::Return);
    CHECK(app->filePath() == dirUtf8 + "/" + naive, "unicode: opened the non-ASCII file");
    CHECK(app->editor().getText() == kNewProgram, "unicode: file contents loaded");

    // Save As a non-ASCII name: the file on disk has exactly that name.
    app->editor().TakeFocus();
    press(*app, Event::Character('#'));
    CHECK(menuCommand(*app, "Save As"), "unicode: Save As clicked");
    clearName(*app);
    typeKeys(*app, "caf\xC3\xA9-2");
    press(*app, Event::Return);
    CHECK(fs::exists(dir / ledbasic::pathFromUtf8("caf\xC3\xA9-2.bas")),
          "unicode: saved under the exact UTF-8 name");
    CHECK(!app->editor().dirty(), "unicode: save reported success");

    // ~ goes to a non-ASCII home folder without throwing.
    setHomeUtf8(ledbasic::utf8FromPath(dir));
    press(*app, Event::CtrlO);
    press(*app, Event::CtrlL);
    for (int i = 0; i < 300; i++) press(*app, Event::Backspace);
    typeKeys(*app, "~");
    press(*app, Event::Return);
    CHECK(openModel(*app).cwd() == dirUtf8, "unicode: ~ opens a non-ASCII home folder");
    press(*app, Event::Escape);
}

void testBackspaceUtf8() {
    auto app = makeApp("");
    press(*app, Event::CtrlS);
    clearName(*app);
    typeKeys(*app, "ab");
    press(*app, Event::Character("\xC3\xA9"));      // é (2 bytes)
    press(*app, Event::Character("\xE2\x82\xAC"));  // € (3 bytes)
    CHECK(app->saveBrowser().name() == "ab\xC3\xA9\xE2\x82\xAC", "utf8 backspace: typed name");
    press(*app, Event::Backspace);
    CHECK(app->saveBrowser().name() == "ab\xC3\xA9", "utf8 backspace: removes all of the 3-byte char");
    press(*app, Event::Backspace);
    CHECK(app->saveBrowser().name() == "ab", "utf8 backspace: removes all of the 2-byte char");
    press(*app, Event::Escape);
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
    testOpenExampleWithMouse();
    testOpenScrollsWithWheel();
    testOpenFromKeyboard();
    testOpenFilter();
    testBrowseAndRemember();
    testOpenTypedPath();
    testOpenProblems();
    testSaveUntitledPromptsForName();
    testSaveAs();
    testSaveAsOverwrite();
    testSaveAsSameFileNoPrompt();
    testSaveExisting();
    testSaveFails();
    {
        std::string savedHome = ledbasic::envUtf8("USERPROFILE");
        std::string savedPosixHome = ledbasic::envUtf8("HOME");
        testUnicodePaths();
#ifdef _WIN32
        setHomeUtf8(savedHome);
#else
        setHomeUtf8(savedPosixHome);
#endif
        (void)savedPosixHome;
        (void)savedHome;
    }
    testBackspaceUtf8();
    testExit();

    fs::remove_all(tempDir(), ec);
}
