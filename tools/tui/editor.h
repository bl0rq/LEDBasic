#ifndef LEDBASIC_TUI_EDITOR_H
#define LEDBASIC_TUI_EDITOR_H

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

class SourceEditor : public ftxui::ComponentBase {
public:
    SourceEditor();

    void setText(const std::string& text);
    std::string getText() const;
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }

    int cursorLine() const { return cursorRow_ + 1; } // 1-based
    int cursorColumn() const { return cursorCol_ + 1; }
    void goToLine(int line1);

    void setReadOnly(bool ro) { readOnly_ = ro; }
    bool readOnly() const { return readOnly_; }

    void setBreakpoints(std::set<int> lines) { breakpoints_ = std::move(lines); }
    const std::set<int>& breakpoints() const { return breakpoints_; }
    bool toggleBreakpointAtCursor();

    void setCurrentLine(int line1) { currentLine_ = line1; }
    void setErrorLine(int line1) { errorLine_ = line1; }

    bool OnEvent(ftxui::Event event) override;
    ftxui::Element OnRender() override;
    bool Focusable() const override { return true; }

    std::function<void()> onBreakpointChanged;

private:
    void splitFrom(const std::string& text);
    void ensureCursor();
    void insert(const std::string& s);
    void backspace();
    void del();
    void newLine();
    void pushUndo();
    void undo();
    ftxui::Element renderLine(int index, int width) const;
    ftxui::Color tokenColor(int type) const;

    std::vector<std::string> lines_;
    std::vector<std::string> undo_;
    int cursorRow_ = 0;
    int cursorCol_ = 0;
    int scrollRow_ = 0;
    bool dirty_ = false;
    bool readOnly_ = false;
    int currentLine_ = 0;
    int errorLine_ = 0;
    std::set<int> breakpoints_;
    ftxui::Box box_;
};

ftxui::Component MakeEditor(std::shared_ptr<SourceEditor> editor);

#endif
