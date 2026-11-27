#include "file_browser_model.h"
#include "tui_test.h"
#include "Utf8Path.h"

#include <filesystem>
#include <fstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sddl.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

fs::path treeRoot() {
    return fs::temp_directory_path() / "ledbasic_tui_browser_tests";
}

void writeFile(const fs::path& p, const std::string& body) {
    std::ofstream out(p, std::ios::binary);
    out << body;
}

// root/
//   A_dir/inner.bas
//   b_dir/
//   .hidden
//   B.txt  big.bas (> 1 MB)  bin.dat (has NUL)  c.bas
void buildTree() {
    std::error_code ec;
    fs::remove_all(treeRoot(), ec);
    fs::create_directories(treeRoot() / "A_dir");
    fs::create_directories(treeRoot() / "b_dir");
    writeFile(treeRoot() / "A_dir" / "inner.bas", "setup\nend\n");
    writeFile(treeRoot() / ".hidden", "x");
    writeFile(treeRoot() / "B.txt", "text\r\nline two\n");
    writeFile(treeRoot() / "c.bas", "setup\n\tclear()\nend\n");
    writeFile(treeRoot() / "big.bas", std::string(FileBrowserModel::kMaxPreviewBytes + 1, 'x'));
    writeFile(treeRoot() / "bin.dat", std::string("ab\0cd", 5));
}

// Folders and files outside buildTree(), so the listing tests stay as they are.
fs::path extraRoot() {
    return fs::temp_directory_path() / "ledbasic_tui_browser_extra";
}

#ifdef _WIN32
bool setDacl(const fs::path& p, const wchar_t* sddl) {
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, nullptr)) return false;
    BOOL ok = SetFileSecurityW(p.c_str(), DACL_SECURITY_INFORMATION, sd);
    LocalFree(sd);
    return ok != FALSE;
}
#endif

// Makes dir unreadable. False when this platform/user can't (e.g. root).
bool lockDir(const fs::path& dir) {
#ifdef _WIN32
    return setDacl(dir, L"D:P(D;OICI;FA;;;WD)");
#else
    if (geteuid() == 0) return false;
    return chmod(dir.c_str(), 0) == 0;
#endif
}

void unlockDir(const fs::path& dir) {
#ifdef _WIN32
    setDacl(dir, L"D:(A;OICI;FA;;;WD)");
#else
    chmod(dir.c_str(), 0755);
#endif
}

std::string names(const FileBrowserModel& m) {
    std::string s;
    for (const auto& e : m.entries()) s += (s.empty() ? "" : ",") + e.name;
    return s;
}

std::string root() {
    return FileBrowserModel::normalize(treeRoot().string());
}

void testListing() {
    FileBrowserModel m;
    CHECK(m.chdir(root()), "browser: chdir into tree");
    CHECK(m.cwd() == root(), "browser: cwd is normalized");
    CHECK(names(m) == "A_dir,b_dir,B.txt,big.bas,bin.dat,c.bas",
          "browser: folders first, then files, case-insensitive");
    CHECK(m.entries()[0].isDir && !m.entries()[2].isDir, "browser: folder flags");
    CHECK(m.entries()[5].name == "c.bas" && m.entries()[5].size == 19, "browser: file sizes");

    m.toggleHidden();
    CHECK(names(m) == "A_dir,b_dir,.hidden,B.txt,big.bas,bin.dat,c.bas", "browser: . shows dotfiles");
    m.toggleHidden();
    CHECK(names(m).find(".hidden") == std::string::npos, "browser: . hides them again");
}

void testFilterAndSelection() {
    FileBrowserModel m;
    m.chdir(root());
    m.selectName("c.bas");
    m.setFilter("BAS");
    CHECK(names(m) == "big.bas,c.bas", "browser: filter is a case-insensitive substring");
    CHECK(m.selectedEntry() && m.selectedEntry()->name == "c.bas", "browser: filter keeps the selection");
    m.setFilter("zzz");
    CHECK(m.entries().empty() && m.selectedEntry() == nullptr, "browser: no match is empty");
    m.setFilter("");
    CHECK(m.entries().size() == 6, "browser: clearing the filter restores the list");

    m.moveTo(0);
    m.move(-5);
    CHECK(m.selected() == 0, "browser: move clamps at the top");
    m.move(100);
    CHECK(m.selected() == 5, "browser: move clamps at the bottom");
}

void testEnterAndUp() {
    FileBrowserModel m;
    m.chdir(root());
    CHECK(m.enterSelected(), "browser: enter the selected folder");
    CHECK(m.cwd() == root() + "/A_dir", "browser: now inside A_dir");
    CHECK(names(m) == "inner.bas", "browser: lists A_dir");
    m.up();
    CHECK(m.cwd() == root(), "browser: up returns to the parent");
    CHECK(m.selectedEntry() && m.selectedEntry()->name == "A_dir", "browser: up re-selects the folder");

    m.selectName("c.bas");
    CHECK(!m.enterSelected(), "browser: enter on a file is refused");
    CHECK(m.selectedPath() == root() + "/c.bas", "browser: selected path");

    m.setFilter("bas");
    m.chdir(root() + "/b_dir");
    CHECK(m.filter().empty(), "browser: changing folder clears the filter");
}

void testErrors() {
    FileBrowserModel m;
    m.chdir(root());
    CHECK(!m.chdir(root() + "/missing"), "browser: missing folder is refused");
    CHECK(!m.error().empty(), "browser: error is reported");
    CHECK(m.cwd() == root(), "browser: stays in the old folder");
    CHECK(m.chdir(root()) && m.error().empty(), "browser: error clears on success");
}

void testPreview() {
    FilePreview p = FileBrowserModel::preview(root() + "/c.bas", 10);
    CHECK(p.kind == FilePreview::Text && p.lines.size() == 3, "preview: text lines");
    CHECK(p.lines.size() == 3 && p.lines[1] == "  clear()", "preview: tabs expand to two spaces");
    p = FileBrowserModel::preview(root() + "/B.txt", 1);
    CHECK(p.lines.size() == 1 && p.lines[0] == "text", "preview: CR stripped, maxLines honored");
    p = FileBrowserModel::preview(root() + "/bin.dat", 10);
    CHECK(p.kind == FilePreview::Binary, "preview: NUL byte means binary");
    p = FileBrowserModel::preview(root() + "/big.bas", 10);
    CHECK(p.kind == FilePreview::TooLarge, "preview: over 1 MB is too large");
    p = FileBrowserModel::preview(root() + "/A_dir", 10);
    CHECK(p.kind == FilePreview::Dir && p.lines.size() == 1 && p.lines[0] == "inner.bas",
          "preview: folder lists its contents");
    p = FileBrowserModel::preview(root() + "/nope.bas", 10);
    CHECK(p.kind == FilePreview::Error, "preview: missing file is an error");
}

void testUnreadableFolder() {
    fs::path locked = extraRoot() / "locked";
    fs::create_directories(locked);
    writeFile(locked / "secret.bas", "setup\nend\n");
    if (!lockDir(locked)) {
        std::printf("  note: cannot make a folder unreadable here, skipping\n");
        return;
    }
    std::string lockedPath = FileBrowserModel::normalize(ledbasic::genericUtf8FromPath(locked));
    std::vector<FileEntry> entries;
    std::string error;
    CHECK(!FileBrowserModel::list(lockedPath, false, entries, &error),
          "unreadable: list reports failure, not an empty folder");
    CHECK(!error.empty(), "unreadable: list says why");
    FileBrowserModel m;
    m.chdir(root());
    CHECK(!m.chdir(lockedPath), "unreadable: chdir is refused");
    CHECK(m.cwd() == root() && !m.error().empty(), "unreadable: stays put and shows the error");
    unlockDir(locked);
}

void testLateNulIsBinary() {
    fs::create_directories(extraRoot());
    std::string body(5000, 'a');
    body += '\0';
    body += "tail\n";
    writeFile(extraRoot() / "late_nul.bas", body);
    FilePreview p = FileBrowserModel::preview(
        ledbasic::genericUtf8FromPath(extraRoot() / "late_nul.bas"), 10);
    CHECK(p.kind == FilePreview::Binary, "preview: a NUL past the first 4 KB is still binary");
}

void testUnicodeNames() {
    // Built from wide/UTF-8 explicitly so the test doesn't depend on the ANSI code page.
    fs::path dir = extraRoot() / ledbasic::pathFromUtf8("caf\xC3\xA9");
    fs::create_directories(dir);
    writeFile(dir / ledbasic::pathFromUtf8("na\xC3\xAFve.bas"), "setup\nend\n");
    std::string dirUtf8 = FileBrowserModel::normalize(ledbasic::genericUtf8FromPath(dir));
    CHECK(dirUtf8.find("caf\xC3\xA9") != std::string::npos, "unicode: normalize keeps UTF-8");
    FileBrowserModel m;
    CHECK(m.chdir(dirUtf8), "unicode: chdir into a non-ASCII folder");
    CHECK(names(m) == "na\xC3\xAFve.bas", "unicode: entry names are UTF-8");
    FilePreview p = FileBrowserModel::preview(m.selectedPath(), 10);
    CHECK(p.kind == FilePreview::Text && p.lines.size() == 2, "unicode: preview reads the file");
    m.up();
    CHECK(m.selectedEntry() && m.selectedEntry()->name == "caf\xC3\xA9",
          "unicode: up re-selects the non-ASCII folder");
}

void testRoot() {
    FileBrowserModel m;
    std::string driveRoot = FileBrowserModel::normalize(treeRoot().root_path().string());
    m.chdir(driveRoot);
    CHECK(m.cwd() == driveRoot, "browser: can list the filesystem root");
#ifdef _WIN32
    m.up();
    CHECK(m.cwd().empty(), "browser: up from a drive root shows the drive list");
    CHECK(m.selectedEntry() && m.selectedEntry()->name == driveRoot,
          "browser: drive list selects the drive we came from");
    m.up();
    CHECK(m.cwd().empty(), "browser: up from the drive list stays there");
    CHECK(m.enterSelected() && m.cwd() == driveRoot, "browser: entering a drive opens its root");
#else
    m.up();
    CHECK(m.cwd() == driveRoot, "browser: up from / stays at /");
#endif
}

} // namespace

void runFileBrowserTests() {
    buildTree();
    testListing();
    testFilterAndSelection();
    testEnterAndUp();
    testErrors();
    testPreview();
    testRoot();
    std::error_code ec;
    fs::remove_all(extraRoot(), ec);
    testUnreadableFolder();
    testLateNulIsBinary();
    testUnicodeNames();
    fs::remove_all(treeRoot(), ec);
    fs::remove_all(extraRoot(), ec);
}
