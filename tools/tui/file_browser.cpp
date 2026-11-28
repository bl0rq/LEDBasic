#include "file_browser.h"
#include "editor.h"
#include "theme.h"
#include "Utf8Path.h"

#include <ftxui/component/mouse.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>

using namespace ftxui;
namespace fs = std::filesystem;

namespace {

std::string homeDir() {
    std::string home = ledbasic::envUtf8("USERPROFILE");
    if (home.empty()) home = ledbasic::envUtf8("HOME");
    return home;
}

std::string sizeLabel(std::uintmax_t n) {
    char buf[16];
    if (n < 1024) std::snprintf(buf, sizeof(buf), "%u", (unsigned)n);
    else if (n < 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1fK", n / 1024.0);
    else std::snprintf(buf, sizeof(buf), "%.1fM", n / (1024.0 * 1024.0));
    return buf;
}

bool isBas(const std::string& name) {
    std::string ext = ledbasic::utf8FromPath(ledbasic::pathFromUtf8(name).extension());
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    return ext == ".bas";
}

std::string displayName(const FileEntry& e) {
    if (e.isDir && !e.name.empty() && e.name.back() != '/') return e.name + "/";
    return e.name;
}

Color entryColor(const FileEntry& e) {
    if (e.isDir) return theme::keyword();
    if (isBas(e.name)) return theme::accent();
    return theme::fg();
}

} // namespace

FileBrowser::FileBrowser(Mode mode, std::string basDir)
    : mode_(mode), basDir_(FileBrowserModel::normalize(basDir)) {}

void FileBrowser::show(const std::string& dir, const std::string& selectName,
                       const std::string& currentFile, const std::string& saveName) {
    if (!model_.chdir(dir)) model_.chdir(basDir_);
    if (!selectName.empty()) model_.selectName(selectName);
    currentFile_ = currentFile.empty() ? "" : FileBrowserModel::normalize(currentFile);
    name_ = saveName;
    nameFocus_ = mode_ == Mode::Save;
    prompt_ = Prompt::None;
    promptText_.clear();
    status_.clear();
    previewFor_ = "\x01";
    previewHeight_ = 0;
    parentFor_ = "\x01";
}

void FileBrowser::setScreenSize(int width, int height) {
    if (width > 0) screenW_ = width;
    if (height > 0) screenH_ = height;
}

int FileBrowser::listRows() const {
    return std::max(1, lastListRows_);
}

// ---------------------------------------------------------------- actions

void FileBrowser::activate() {
    const FileEntry* e = model_.selectedEntry();
    if (!e) return;
    if (e->isDir) {
        if (!model_.enterSelected()) status_ = model_.error();
        return;
    }
    if (mode_ == Mode::Open) {
        openFile(model_.selectedPath());
    } else {
        name_ = e->name;
        nameFocus_ = true;
    }
}

void FileBrowser::openFile(const std::string& path) {
    FilePreview p = FileBrowserModel::preview(path, 1);
    if (p.kind == FilePreview::Binary) {
        status_ = "not a text file";
        return;
    }
    if (p.kind == FilePreview::TooLarge) {
        status_ = "file is too large to edit";
        return;
    }
    if (onOpen) onOpen(path);
}

void FileBrowser::goToPath(std::string text) {
    if (text.empty()) return;
    if (text == "~" || (text.size() >= 2 && text[0] == '~' && (text[1] == '/' || text[1] == '\\'))) {
        std::string home = homeDir();
        if (home.empty()) {
            status_ = "home folder unavailable";
            return;
        }
        text = home + text.substr(1);
    }
    fs::path p = ledbasic::pathFromUtf8(text);
    if (p.empty()) {
        status_ = "not found: " + text;
        return;
    }
    if (p.is_relative() && !model_.cwd().empty()) p = ledbasic::pathFromUtf8(model_.cwd()) / p;
    std::string full = FileBrowserModel::normalize(ledbasic::genericUtf8FromPath(p));
    fs::path fullPath = ledbasic::pathFromUtf8(full);

    std::error_code ec;
    if (fs::is_directory(fullPath, ec)) {
        if (!model_.chdir(full)) status_ = model_.error();
        return;
    }
    std::string parent = ledbasic::genericUtf8FromPath(fullPath.parent_path());
    std::string file = ledbasic::utf8FromPath(fullPath.filename());
    if (fs::is_regular_file(fullPath, ec)) {
        if (mode_ == Mode::Open) {
            model_.chdir(parent);
            model_.selectName(file);
            openFile(full);
        } else {
            if (!model_.chdir(parent)) {
                status_ = model_.error();
                return;
            }
            model_.selectName(file);
            name_ = file;
            nameFocus_ = true;
        }
        return;
    }
    if (mode_ == Mode::Save && fs::is_directory(fullPath.parent_path(), ec)) {
        if (!model_.chdir(parent)) {
            status_ = model_.error();
            return;
        }
        name_ = file;
        nameFocus_ = true;
        return;
    }
    status_ = "not found: " + text;
}

void FileBrowser::trySave() {
    if (model_.cwd().empty()) {
        status_ = "pick a folder first";
        return;
    }
    if (name_.empty()) {
        status_ = "enter a file name";
        return;
    }
    if (name_.find('/') != std::string::npos || name_.find('\\') != std::string::npos ||
        name_ == "." || name_ == "..") {
        status_ = "enter a file name (use Ctrl+L for folders)";
        return;
    }
    std::error_code ec;
    std::string candidate = name_;
    fs::path rawTarget = ledbasic::pathFromUtf8(model_.pathOf(candidate));
    if (fs::is_directory(rawTarget, ec)) {
        status_ = candidate + " is a folder";
        return;
    }
    if (ledbasic::pathFromUtf8(candidate).extension().empty()) candidate += ".bas";
    std::string target = model_.pathOf(candidate);
    fs::path targetPath = ledbasic::pathFromUtf8(target);
    if (fs::is_directory(targetPath, ec)) {
        status_ = candidate + " is a folder";
        return;
    }
    bool same = !currentFile_.empty() && fs::equivalent(targetPath, ledbasic::pathFromUtf8(currentFile_), ec);
    fs::file_status st = fs::symlink_status(targetPath, ec);
    bool targetExists = !ec && fs::exists(st);
    if (targetExists && !same) {
        confirmPath_ = target;
        prompt_ = Prompt::Confirm;
        return;
    }
    name_ = candidate;
    if (onSave) onSave(target);
}

void FileBrowser::cancel() {
    if (!model_.filter().empty()) {
        model_.setFilter("");
        return;
    }
    if (onCancel) onCancel();
}

// ---------------------------------------------------------------- events

bool FileBrowser::OnEvent(Event event) {
    std::string before = model_.cwd();
    bool handled = event.is_mouse() ? handleMouse(event) : handleKey(event);
    if (model_.cwd() != before && onDirChanged) onDirChanged(model_.cwd());
    return handled;
}

// Single-line text editing for prompts and the Name field.
bool FileBrowser::editText(std::string& text, const Event& e) {
    if (e == Event::Backspace) {
        // Drop the whole last UTF-8 character: its continuation bytes
        // (10xxxxxx), then its lead byte.
        while (!text.empty() && ((unsigned char)text.back() & 0xC0) == 0x80) text.pop_back();
        if (!text.empty()) text.pop_back();
        return true;
    }
    if (e.is_character()) {
        std::string ch = e.character();
        if (!ch.empty() && (unsigned char)ch[0] >= 32) {
            text += ch;
            return true;
        }
    }
    return false;
}

bool FileBrowser::handleNavKey(const Event& e) {
    if (e == Event::ArrowUp) { model_.move(-1); return true; }
    if (e == Event::ArrowDown) { model_.move(1); return true; }
    if (e == Event::PageUp) { model_.move(-(listRows() - 1)); return true; }
    if (e == Event::PageDown) { model_.move(listRows() - 1); return true; }
    return false;
}

bool FileBrowser::handleKey(const Event& e) {
    if (e.is_character() || e == Event::Return || e == Event::Escape) status_.clear();

    switch (prompt_) {
        case Prompt::Confirm:
            if (e == Event::Character('y') || e == Event::Character('Y')) {
                prompt_ = Prompt::None;
                if (onSave) onSave(confirmPath_);
            } else if (e == Event::Character('n') || e == Event::Character('N') ||
                       e == Event::Escape) {
                prompt_ = Prompt::None;
            }
            return true;
        case Prompt::Filter:
            if (e == Event::Escape) {
                model_.setFilter("");
                prompt_ = Prompt::None;
            } else if (e == Event::Return) {
                prompt_ = Prompt::None;
            } else if (editText(promptText_, e)) {
                model_.setFilter(promptText_);
            } else {
                handleNavKey(e);
            }
            return true;
        case Prompt::Path:
            if (e == Event::Escape) {
                prompt_ = Prompt::None;
            } else if (e == Event::Return) {
                prompt_ = Prompt::None;
                goToPath(promptText_);
            } else {
                editText(promptText_, e);
            }
            return true;
        case Prompt::None:
            break;
    }

    auto prefillCwd = [&]() -> std::string {
        const std::string& cwd = model_.cwd();
        if (cwd.empty()) return "";
        return cwd.back() == '/' ? cwd : cwd + "/";
    };

    if (e == Event::CtrlL) {
        prompt_ = Prompt::Path;
        promptText_ = prefillCwd();
        return true;
    }

    if (mode_ == Mode::Save && nameFocus_) {
        if (e == Event::Return) { trySave(); return true; }
        if (e == Event::Tab) { nameFocus_ = false; return true; }
        if (e == Event::Escape) { cancel(); return true; }
        if (handleNavKey(e)) return true;
        return editText(name_, e) || e.is_character();
    }

    if (handleNavKey(e)) return true;
    auto is = [&](char c) { return e == Event::Character(c); };
    if (is('k')) { model_.move(-1); return true; }
    if (is('j')) { model_.move(1); return true; }
    if (e == Event::Home || is('g')) { model_.moveTo(0); return true; }
    if (e == Event::End || is('G')) { model_.moveTo((int)model_.entries().size() - 1); return true; }
    if (e == Event::ArrowLeft || e == Event::Backspace || is('h')) {
        model_.up();
        if (!model_.error().empty()) status_ = model_.error();
        return true;
    }
    if (e == Event::ArrowRight || e == Event::Return || is('l')) { activate(); return true; }
    if (is('/')) {
        prompt_ = Prompt::Filter;
        promptText_ = model_.filter();
        return true;
    }
    if (is('.')) { model_.toggleHidden(); return true; }
    if (is('~')) {
        std::string home = homeDir();
        if (home.empty()) status_ = "home folder unavailable";
        else if (!model_.chdir(home)) status_ = model_.error();
        return true;
    }
    if (is('b')) {
        if (!model_.chdir(basDir_)) status_ = model_.error();
        return true;
    }
    if (is(':')) {
        prompt_ = Prompt::Path;
        promptText_ = prefillCwd();
        return true;
    }
    if (e == Event::Tab && mode_ == Mode::Save) { nameFocus_ = true; return true; }
    if (e == Event::Escape) { cancel(); return true; }
    // The dialog is modal: swallow stray typing rather than leak it.
    return e.is_character();
}

bool FileBrowser::handleMouse(Event e) {
    if (prompt_ != Prompt::None) return true;
    const Mouse& m = e.mouse();
    if (m.button == Mouse::WheelUp || m.button == Mouse::WheelDown) {
        if (!listBox_.Contain(m.x, m.y)) return false;
        model_.move(m.button == Mouse::WheelDown ? 3 : -3);
        return true;
    }
    if (m.button != Mouse::Left || m.motion != Mouse::Pressed) return false;
    if (mode_ == Mode::Save && nameBox_.Contain(m.x, m.y)) {
        TakeFocus();
        nameFocus_ = true;
        return true;
    }
    if (!listBox_.Contain(m.x, m.y)) return false;
    TakeFocus();
    for (size_t i = 0; i < rowBoxes_.size() && i < model_.entries().size(); i++) {
        if (!rowBoxes_[i].Contain(m.x, m.y)) continue;
        if ((int)i == model_.selected()) {
            activate();
        } else {
            model_.moveTo((int)i);
            const FileEntry* sel = model_.selectedEntry();
            if (sel && sel->isDir && !model_.enterSelected()) status_ = model_.error();
        }
        return true;
    }
    return true;
}

// ---------------------------------------------------------------- render

Element FileBrowser::renderParent(int width, int height) {
    std::string parent = FileBrowserModel::parentOf(model_.cwd());
    if (parent != parentFor_) {
        parentFor_ = parent;
        parentEntries_.clear();
        if (parent != model_.cwd()) FileBrowserModel::list(parent, false, parentEntries_);
    }
    std::string here = model_.cwd().empty() ? "" : ledbasic::utf8FromPath(ledbasic::pathFromUtf8(model_.cwd()).filename());
    if (here.empty()) here = model_.cwd(); // drive root
    Elements rows;
    for (const auto& e : parentEntries_) {
        Element row = text(" " + displayName(e)) | color(entryColor(e));
        if (e.name == here) row = row | inverted | focus;
        rows.push_back(row);
    }
    if (rows.empty()) rows.push_back(text(""));
    return vbox(std::move(rows)) | yframe | size(WIDTH, EQUAL, width) | size(HEIGHT, EQUAL, height);
}

Element FileBrowser::renderList(int width, int height) {
    lastListRows_ = height;
    const auto& entries = model_.entries();
    rowBoxes_.resize(entries.size());
    Elements rows;
    for (size_t i = 0; i < entries.size(); i++) {
        const FileEntry& e = entries[i];
        Element row = hbox(Elements{
            text(" " + displayName(e)) | color(entryColor(e)) | flex,
            text(e.isDir ? "" : sizeLabel(e.size) + " ") | color(theme::dim()),
        });
        if ((int)i == model_.selected()) {
            row = row | inverted | focus;
            if (mode_ == Mode::Save && nameFocus_) row = row | dim;
        }
        rows.push_back(row | reflect(rowBoxes_[i]));
    }
    if (rows.empty()) {
        std::string msg = model_.filter().empty() ? "  (empty)" : "  (no match)";
        rows.push_back(text(msg) | color(theme::dim()));
    }
    return vbox(std::move(rows)) | vscroll_indicator | yframe | size(WIDTH, EQUAL, width) |
           size(HEIGHT, EQUAL, height) | reflect(listBox_);
}

Element FileBrowser::renderPreview(int width, int height) {
    std::string path = model_.selectedPath();
    if (path != previewFor_ || height != previewHeight_) {
        previewFor_ = path;
        previewHeight_ = height;
        preview_ = path.empty() ? FilePreview{} : FileBrowserModel::preview(path, height);
    }
    Elements rows;
    switch (preview_.kind) {
        case FilePreview::Text: {
            bool bas = isBas(path);
            for (const auto& l : preview_.lines) {
                rows.push_back(bas ? highlightBasicLine(l) : text(l));
            }
            break;
        }
        case FilePreview::Dir:
            for (const auto& l : preview_.lines) {
                bool dir = !l.empty() && l.back() == '/';
                rows.push_back(text(l) | color(dir ? theme::keyword() : theme::fg()));
            }
            if (rows.empty()) rows.push_back(text("(empty)") | color(theme::dim()));
            break;
        case FilePreview::Binary:
            rows.push_back(text("binary file") | color(theme::dim()));
            break;
        case FilePreview::TooLarge:
            rows.push_back(text("too large to preview") | color(theme::dim()));
            break;
        case FilePreview::Error:
            break;
    }
    if (rows.empty()) rows.push_back(text(""));
    // frame clips long lines at the edge; without it hbox shrinks each
    // colored span of a highlighted line and drops characters mid-line.
    return vbox(std::move(rows)) | frame | size(WIDTH, EQUAL, width) | size(HEIGHT, EQUAL, height);
}

Element FileBrowser::renderFooter() {
    switch (prompt_) {
        case Prompt::Filter:
            return text(" /" + promptText_ + "█") | color(theme::accent());
        case Prompt::Path:
            return text(" Go to: " + promptText_ + "█") | color(theme::accent());
        case Prompt::Confirm:
            return text(" Overwrite " + ledbasic::utf8FromPath(ledbasic::pathFromUtf8(confirmPath_).filename()) + "? y/n") |
                   color(theme::error()) | bold;
        case Prompt::None:
            break;
    }
    if (!status_.empty()) return text(" " + status_) | color(theme::error());
    if (!model_.error().empty()) return text(" " + model_.error()) | color(theme::error());
    if (mode_ == Mode::Save) {
        return text(" Enter save  Tab name/list  ↑↓ move  ^L path  Esc cancel") |
               color(theme::dim());
    }
    return text(" j/k h/l move  / filter  . hidden  ~ home  b bas  ^L path  Esc") |
           color(theme::dim());
}

Element FileBrowser::OnRender() {
    int w = std::clamp(screenW_ * 9 / 10, 50, 150);
    int h = std::clamp(screenH_ * 8 / 10, 14, 45);
    int inner = w - 2;
    int colsWidth = inner - 2; // two column separators
    int pw = std::max(8, colsWidth / 8);
    int cw = colsWidth * 4 / 8;
    int vw = colsWidth - pw - cw;
    int fixedRows = 2 + 1 + 1 + 1 + 1 + (mode_ == Mode::Save ? 2 : 0);
    int colH = std::max(3, h - fixedRows);

    std::string where = model_.cwd().empty() ? "Drives" : model_.cwd();
    std::string count = std::to_string(model_.entries().size()) + " items ";
    if (!model_.filter().empty()) count = "filter: " + model_.filter() + "  " + count;
    if (model_.showHidden()) count = "hidden shown  " + count;
    Element header = hbox(Elements{
        text(" " + where) | bold,
        filler(),
        text(count) | color(theme::dim()),
    });
    if (!model_.error().empty() && model_.cwd().empty()) {
        header = text(" " + model_.error()) | color(theme::error());
    }

    Elements body{
        header,
        separator(),
        hbox(Elements{
            renderParent(pw, colH) | color(theme::dim()),
            separator(),
            renderList(cw, colH),
            separator(),
            renderPreview(vw, colH),
        }),
        separator(),
    };
    if (mode_ == Mode::Save) {
        Element field = text(name_ + (nameFocus_ ? "█" : "")) | flex;
        if (nameFocus_) field = field | bgcolor(theme::current());
        body.push_back(hbox(Elements{text(" Name: ") | bold, field}) | reflect(nameBox_));
        body.push_back(separator());
    }
    body.push_back(renderFooter());

    const char* title = mode_ == Mode::Open ? " Open " : " Save as ";
    Element dialog = window(text(title) | bold | color(theme::accent()), vbox(std::move(body))) |
                     bgcolor(theme::chrome()) | size(WIDTH, EQUAL, w) | size(HEIGHT, EQUAL, h);
    // A blank margin keeps the border from merging with lines behind it.
    return hbox(Elements{text(" "), vbox(Elements{text(" "), dialog, text(" ")}), text(" ")}) |
           bgcolor(theme::bg());
}
