#include "file_browser_model.h"
#include "Utf8Path.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool isHidden(const fs::directory_entry& e) {
    std::string name = ledbasic::utf8FromPath(e.path().filename());
    if (!name.empty() && name[0] == '.') return true;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesW(e.path().c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN)) return true;
#endif
    return false;
}

bool isDriveRoot(const fs::path& p) {
    return p.has_root_path() && !p.has_relative_path();
}

} // namespace

std::string FileBrowserModel::normalize(const std::string& path) {
    if (path.empty()) return path;
    fs::path raw = ledbasic::pathFromUtf8(path);
    if (raw.empty()) return path;
    std::error_code ec;
    fs::path p = fs::absolute(raw, ec);
    if (ec) p = raw;
    p = p.lexically_normal();
    std::string s = ledbasic::genericUtf8FromPath(p);
    // "C:/dir/" -> "C:/dir", but keep "C:/" and "/".
    while (s.size() > 1 && s.back() == '/' && !isDriveRoot(ledbasic::pathFromUtf8(s))) s.pop_back();
    return s;
}

std::string FileBrowserModel::parentOf(const std::string& dir) {
    if (dir.empty()) return dir;
    fs::path p = ledbasic::pathFromUtf8(dir);
    if (isDriveRoot(p)) {
#ifdef _WIN32
        return "";
#else
        return dir;
#endif
    }
    return normalize(ledbasic::genericUtf8FromPath(p.parent_path()));
}

bool FileBrowserModel::list(const std::string& dir, bool showHidden, std::vector<FileEntry>& out,
                            std::string* error) {
    out.clear();
#ifdef _WIN32
    if (dir.empty()) {
        DWORD mask = GetLogicalDrives();
        for (int i = 0; i < 26; i++) {
            if (!(mask & (1u << i))) continue;
            FileEntry e;
            e.name = std::string(1, (char)('A' + i)) + ":/";
            e.isDir = true;
            out.push_back(e);
        }
        return true;
    }
#endif
    std::error_code ec;
    // No skip_permission_denied: it turns "can't open this folder" into an
    // empty listing, and the caller must see that as an error.
    fs::directory_iterator it(ledbasic::pathFromUtf8(dir), ec);
    if (ec) {
        if (error) *error = "cannot open " + dir + ": " + ec.message();
        return false;
    }
    for (; it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) break;
        FileEntry e;
        e.name = ledbasic::utf8FromPath(it->path().filename());
        e.hidden = isHidden(*it);
        if (e.hidden && !showHidden) continue;
        std::error_code sec;
        e.isDir = it->is_directory(sec);
        if (!e.isDir) {
            e.size = it->file_size(sec);
            if (sec) e.size = 0;
        }
        out.push_back(e);
    }
    if (ec) {
        if (error) *error = "cannot list " + dir + ": " + ec.message();
        out.clear();
        return false;
    }
    std::sort(out.begin(), out.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDir != b.isDir) return a.isDir;
        std::string la = lower(a.name), lb = lower(b.name);
        if (la != lb) return la < lb;
        return a.name < b.name;
    });
    return true;
}

FilePreview FileBrowserModel::preview(const std::string& path, int maxLines) {
    FilePreview out;
    std::error_code ec;
    fs::path p = ledbasic::pathFromUtf8(path);
    if (path.empty() || fs::is_directory(p, ec)) {
        std::vector<FileEntry> entries;
        if (!list(path, false, entries)) return out;
        out.kind = FilePreview::Dir;
        for (const auto& e : entries) {
            if ((int)out.lines.size() >= maxLines) break;
            out.lines.push_back(e.isDir ? e.name + (e.name.back() == '/' ? "" : "/") : e.name);
        }
        return out;
    }
    std::uintmax_t size = fs::file_size(p, ec);
    if (ec) return out;
    if (size > kMaxPreviewBytes) {
        out.kind = FilePreview::TooLarge;
        return out;
    }
    std::ifstream in(p, std::ios::binary);
    if (!in) return out;
    // Read the whole file (bounded by kMaxPreviewBytes above): a NUL anywhere
    // makes it binary, not just one in the first few KB.
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (body.find('\0') != std::string::npos) {
        out.kind = FilePreview::Binary;
        return out;
    }
    out.kind = FilePreview::Text;
    std::istringstream lines(body);
    std::string line;
    while ((int)out.lines.size() < maxLines && std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string expanded;
        for (char c : line) {
            if (c == '\t') expanded += "  ";
            else expanded += c;
        }
        out.lines.push_back(expanded);
    }
    return out;
}

bool FileBrowserModel::chdir(const std::string& path) {
    std::string dir = normalize(path);
    std::vector<FileEntry> entries;
    std::string err;
    if (!list(dir, showHidden_, entries, &err)) {
        error_ = err;
        return false;
    }
    cwd_ = dir;
    all_ = std::move(entries);
    error_.clear();
    filter_.clear();
    applyFilter("");
    return true;
}

void FileBrowserModel::refresh() {
    const FileEntry* sel = selectedEntry();
    std::string keep = sel ? sel->name : "";
    std::vector<FileEntry> entries;
    std::string err;
    if (!list(cwd_, showHidden_, entries, &err)) {
        error_ = err;
        return;
    }
    all_ = std::move(entries);
    applyFilter(keep);
}

void FileBrowserModel::up() {
    std::string parent = parentOf(cwd_);
    if (parent == cwd_) return;
    std::string child = cwd_.empty() ? "" : ledbasic::genericUtf8FromPath(ledbasic::pathFromUtf8(cwd_).filename());
    if (child.empty() && !cwd_.empty()) child = cwd_; // drive root, e.g. "C:/"
    if (chdir(parent)) selectName(child);
}

bool FileBrowserModel::enterSelected() {
    const FileEntry* e = selectedEntry();
    if (!e || !e->isDir) return false;
    return chdir(selectedPath());
}

void FileBrowserModel::move(int delta) {
    moveTo(selected_ + delta);
}

void FileBrowserModel::moveTo(int index) {
    if (visible_.empty()) {
        selected_ = 0;
        return;
    }
    selected_ = std::clamp(index, 0, (int)visible_.size() - 1);
}

bool FileBrowserModel::selectName(const std::string& name) {
    for (size_t i = 0; i < visible_.size(); i++) {
        if (visible_[i].name == name) {
            selected_ = (int)i;
            return true;
        }
    }
    return false;
}

void FileBrowserModel::setFilter(const std::string& filter) {
    const FileEntry* sel = selectedEntry();
    std::string keep = sel ? sel->name : "";
    filter_ = filter;
    applyFilter(keep);
}

void FileBrowserModel::toggleHidden() {
    showHidden_ = !showHidden_;
    refresh();
}

void FileBrowserModel::applyFilter(const std::string& keepName) {
    visible_.clear();
    std::string needle = lower(filter_);
    for (const auto& e : all_) {
        if (needle.empty() || lower(e.name).find(needle) != std::string::npos) visible_.push_back(e);
    }
    selected_ = 0;
    if (!keepName.empty()) selectName(keepName);
}

const FileEntry* FileBrowserModel::selectedEntry() const {
    if (selected_ < 0 || selected_ >= (int)visible_.size()) return nullptr;
    return &visible_[(size_t)selected_];
}

std::string FileBrowserModel::pathOf(const std::string& name) const {
    if (cwd_.empty()) return name; // drive list entries are already paths
    return normalize(ledbasic::genericUtf8FromPath(ledbasic::pathFromUtf8(cwd_) / ledbasic::pathFromUtf8(name)));
}

std::string FileBrowserModel::selectedPath() const {
    const FileEntry* e = selectedEntry();
    return e ? pathOf(e->name) : "";
}
