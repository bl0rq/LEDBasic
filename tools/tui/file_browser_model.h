#ifndef LEDBASIC_TUI_FILE_BROWSER_MODEL_H
#define LEDBASIC_TUI_FILE_BROWSER_MODEL_H

#include <cstdint>
#include <string>
#include <vector>

struct FileEntry {
    std::string name;
    bool isDir = false;
    bool hidden = false;
    std::uintmax_t size = 0;
};

struct FilePreview {
    enum Kind { Text, Dir, Binary, TooLarge, Error };
    Kind kind = Error;
    std::vector<std::string> lines; // file lines, or entry names for a folder
};

// Folder navigation state for the file browser, with no UI. Paths use
// forward slashes. On Windows an empty cwd() is the drive list, reached by
// going up from a drive root. Never throws; failures set error().
class FileBrowserModel {
public:
    static constexpr std::uintmax_t kMaxPreviewBytes = 1024 * 1024;

    // Lists dir (folders first, then files, case-insensitive). "" is the
    // drive list on Windows. Returns false and sets *error if unreadable.
    static bool list(const std::string& dir, bool showHidden, std::vector<FileEntry>& out,
                     std::string* error = nullptr);
    static FilePreview preview(const std::string& path, int maxLines);
    static std::string normalize(const std::string& path);
    // "" (drive list) for a Windows drive root; the root itself on POSIX.
    static std::string parentOf(const std::string& dir);

    // Keeps the current folder and sets error() when path can't be listed.
    bool chdir(const std::string& path);
    void refresh();
    void up();
    // Enters the selected folder. False when the selection is a file.
    bool enterSelected();

    void move(int delta);
    void moveTo(int index);
    bool selectName(const std::string& name);

    void setFilter(const std::string& filter);
    const std::string& filter() const { return filter_; }
    void toggleHidden();
    bool showHidden() const { return showHidden_; }

    const std::string& cwd() const { return cwd_; }
    const std::vector<FileEntry>& entries() const { return visible_; }
    int selected() const { return selected_; }
    const FileEntry* selectedEntry() const;
    std::string selectedPath() const;
    std::string pathOf(const std::string& name) const;
    const std::string& error() const { return error_; }

private:
    void applyFilter(const std::string& keepName);

    std::string cwd_;
    std::vector<FileEntry> all_;
    std::vector<FileEntry> visible_;
    int selected_ = 0;
    std::string filter_;
    bool showHidden_ = false;
    std::string error_;
};

#endif
