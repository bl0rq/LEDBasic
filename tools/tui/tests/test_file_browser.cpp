#include "file_browser_model.h"
#include "tui_test.h"

#include <filesystem>
#include <fstream>

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
    fs::remove_all(treeRoot(), ec);
}
