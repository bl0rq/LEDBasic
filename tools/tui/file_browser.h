#ifndef LEDBASIC_TUI_FILE_BROWSER_H
#define LEDBASIC_TUI_FILE_BROWSER_H

#include "file_browser_model.h"

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include <functional>
#include <string>
#include <vector>

// Yazi-style three-column file browser (parent / current / preview) used
// by the Open and Save As dialogs.
class FileBrowser : public ftxui::ComponentBase {
public:
    enum class Mode { Open, Save };

    FileBrowser(Mode mode, std::string basDir);

    // Resets the browser to dir (the bas folder if dir can't be listed),
    // with selectName highlighted. currentFile is the file open in the
    // editor: saving over it needs no confirmation. saveName fills the
    // Name field in Save mode.
    void show(const std::string& dir, const std::string& selectName,
              const std::string& currentFile, const std::string& saveName = "");
    // Terminal size from the last frame; the dialog sizes itself from it.
    void setScreenSize(int width, int height);

    std::function<void(const std::string&)> onOpen;       // Open: file chosen
    std::function<void(const std::string&)> onSave;       // Save: target chosen and confirmed
    std::function<void()> onCancel;
    std::function<void(const std::string&)> onDirChanged; // user moved to another folder

    FileBrowserModel& model() { return model_; }
    const std::string& name() const { return name_; }
    const std::string& status() const { return status_; }
    void setStatus(const std::string& s) { status_ = s; }

    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event event) override;
    bool Focusable() const override { return true; }

private:
    enum class Prompt { None, Filter, Path, Confirm };

    bool handleKey(const ftxui::Event& e);
    bool handleNavKey(const ftxui::Event& e);
    bool handleMouse(ftxui::Event e);
    bool editText(std::string& text, const ftxui::Event& e);
    void activate();
    void openFile(const std::string& path);
    void goToPath(std::string text);
    void trySave();
    void cancel();
    int listRows() const;

    ftxui::Element renderParent(int width, int height);
    ftxui::Element renderList(int width, int height);
    ftxui::Element renderPreview(int width, int height);
    ftxui::Element renderFooter();

    Mode mode_;
    std::string basDir_;
    FileBrowserModel model_;
    Prompt prompt_ = Prompt::None;
    std::string promptText_;
    std::string confirmPath_;
    std::string name_;
    bool nameFocus_ = false;
    std::string status_;
    std::string currentFile_;
    int screenW_ = 100;
    int screenH_ = 30;
    int lastListRows_ = 10;

    std::vector<ftxui::Box> rowBoxes_;
    ftxui::Box listBox_;
    ftxui::Box nameBox_;
    std::string previewFor_;
    FilePreview preview_;
    std::string parentFor_ = "\x01"; // never a real path, forces the first load
    std::vector<FileEntry> parentEntries_;
};

#endif
