#include "file_browser.h"
#include "editor.h"
#include "theme.h"

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
    if (const char* h = std::getenv("USERPROFILE")) return h;
    if (const char* h = std::getenv("HOME")) return h;
    return "";
}

std::string sizeLabel(std::uintmax_t n) {
    char buf[16];
    if (n < 1024) std::snprintf(buf, sizeof(buf), "%u", (unsigned)n);
    else if (n < 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1fK", n / 1024.0);
    else std::snprintf(buf, sizeof(buf), "%.1fM", n / (1024.0 * 1024.0));
    return buf;
}

bool isBas(const std::string& name) {
    std::string ext = fs::path(name).extension().string();
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
        model_.enterSelected();
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
    if (text[0] == '~') text = homeDir() + text.substr(1);
    fs::path p(text);
    if (p.is_relative() && !model_.cwd().empty()) p = fs::path(model_.cwd()) / p;
    std::string full = FileBrowserModel::normalize(p.generic_string());

    std::error_code ec;
    if (fs::is_directory(full, ec)) {
        if (!model_.chdir(full)) status_ = model_.error();
        return;
    }
    std::string parent = fs::path(full).parent_path().generic_string();
    std::string file = fs::path(full).filename().string();
    if (fs::is_regular_file(full, ec)) {
        model_.chdir(parent);
        model_.selectName(file);
        if (mode_ == Mode::Open) {
            openFile(full);
        } else {
            name_ = file;
            nameFocus_ = true;
        }
        return;
    }
    if (mode_ == Mode::Save && fs::is_directory(parent, ec)) {
        model_.chdir(parent);
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
    if (fs::path(name_).extension().empty()) name_ += ".bas";
    std::string target = model_.pathOf(name_);
    std::error_code ec;
    if (fs::is_directory(target, ec)) {
        status_ = name_ + " is a folder";
        return;
    }
    bool same = !currentFile_.empty() && fs::equivalent(target, currentFile_, ec);
    if (fs::exists(target, ec) && !same) {
        confirmPath_ = target;
        prompt_ = Prompt::Confirm;
        return;
    }
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

    if (e == Event::CtrlL) {
        prompt_ = Prompt::Path;
        promptText_ = model_.cwd().empty() ? "" : model_.cwd() + "/";
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
    if (e == Event::ArrowLeft || e == Event::Backspace || is('h')) { model_.up(); return true; }
    if (e == Event::ArrowRight || e == Event::Return || is('l')) { activate(); return true; }
    if (is('/')) {
        prompt_ = Prompt::Filter;
        promptText_ = model_.filter();
        return true;
    }
    if (is('.')) { model_.toggleHidden(); return true; }
    if (is('~')) {
        if (!model_.chdir(homeDir())) status_ = model_.error();
        return true;
    }
    if (is('b')) {
        if (!model_.chdir(basDir_)) status_ = model_.error();
        return true;
    }
    if (is(':')) {
        prompt_ = Prompt::Path;
        promptText_ = model_.cwd().empty() ? "" : model_.cwd() + "/";
        return true;
    }
    if (e == Event::Tab && mode_ == Mode::Save) { nameFocus_ = true; return true; }
    if (e == Event::Escape) { cancel(); return true; }
    // The dialog is modal: swallow stray typing rather than leak it.
    return e.is_character();
}

bool FileBrowser::handleMouse(Event e) {
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
            if (sel && sel->isDir) model_.enterSelected(); // one click enters a folder
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
    std::string here = model_.cwd().empty() ? "" : fs::path(model_.cwd()).filename().string();
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
    if (path != previewFor_) {
        previewFor_ = path;
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
            return text(" Overwrite " + fs::path(confirmPath_).filename().string() + "? y/n") |
                   color(theme::error()) | bold;
        case Prompt::None:
            break;
    }
    if (!status_.empty()) return text(" " + status_) | color(theme::error());
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
