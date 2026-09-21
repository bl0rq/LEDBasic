#include "editor.h"
#include "theme.h"
#include "BasicInterpreter.h"

#include <algorithm>
#include <cstdio>
#include <memory>

using namespace ftxui;

namespace {

bool isKeywordToken(TokenType t) {
    return t == TOK_SETUP || t == TOK_LOOP || t == TOK_IF || t == TOK_ELSE ||
           t == TOK_WHILE || t == TOK_FOR || t == TOK_TO || t == TOK_STEP ||
           t == TOK_NEXT || t == TOK_FUNCTION || t == TOK_RETURN || t == TOK_END ||
           t == TOK_DIM || t == TOK_PARAM || t == TOK_AND || t == TOK_OR ||
           t == TOK_NOT || t == TOK_TRUE || t == TOK_FALSE;
}

bool isFuncToken(TokenType t) {
    return t == TOK_SIN || t == TOK_COS || t == TOK_TAN || t == TOK_SQRT ||
           t == TOK_POW || t == TOK_LOG || t == TOK_LN || t == TOK_ABS ||
           t == TOK_FLOOR || t == TOK_CEIL || t == TOK_ROUND || t == TOK_MIN ||
           t == TOK_MAX || t == TOK_RANDOM || t == TOK_MAP || t == TOK_MILLIS ||
           t == TOK_DELAY || t == TOK_HSV_TO_RGB || t == TOK_WHEEL ||
           t == TOK_SETLED || t == TOK_SETCOLOR || t == TOK_SETHSV || t == TOK_SHOW ||
           t == TOK_CLEAR || t == TOK_FILL || t == TOK_BRIGHTNESS || t == TOK_NUMLED ||
           t == TOK_HSV || t == TOK_RGB || t == TOK_GET_LED_R || t == TOK_GET_LED_G ||
           t == TOK_GET_LED_B || t == TOK_SET_LED || t == TOK_SET_ALL ||
           t == TOK_GET_LED_COUNT;
}

} // namespace

SourceEditor::SourceEditor() {
    lines_.push_back("");
}

void SourceEditor::splitFrom(const std::string& text) {
    lines_.clear();
    std::string cur;
    for (size_t i = 0; i < text.size(); i++) {
        char c = text[i];
        if (c == '\r') continue;
        if (c == '\n') {
            lines_.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    lines_.push_back(cur);
    if (lines_.empty()) lines_.push_back("");
}

void SourceEditor::setText(const std::string& text) {
    splitFrom(text);
    cursorRow_ = 0;
    cursorCol_ = 0;
    scrollRow_ = 0;
    dirty_ = false;
    undo_.clear();
}

std::string SourceEditor::getText() const {
    std::string s;
    for (size_t i = 0; i < lines_.size(); i++) {
        s += lines_[i];
        if (i + 1 < lines_.size()) s += '\n';
    }
    return s;
}

void SourceEditor::goToLine(int line1) {
    cursorRow_ = std::max(0, line1 - 1);
    ensureCursor();
}

void SourceEditor::ensureCursor() {
    if (lines_.empty()) lines_.push_back("");
    if (cursorRow_ < 0) cursorRow_ = 0;
    if (cursorRow_ >= (int)lines_.size()) cursorRow_ = (int)lines_.size() - 1;
    int len = (int)lines_[(size_t)cursorRow_].size();
    if (cursorCol_ < 0) cursorCol_ = 0;
    if (cursorCol_ > len) cursorCol_ = len;
}

void SourceEditor::pushUndo() {
    undo_.push_back(getText());
    if (undo_.size() > 40) undo_.erase(undo_.begin());
}

void SourceEditor::undo() {
    if (undo_.empty() || readOnly_) return;
    std::string prev = undo_.back();
    undo_.pop_back();
    int row = cursorRow_, col = cursorCol_;
    splitFrom(prev);
    cursorRow_ = row;
    cursorCol_ = col;
    ensureCursor();
    dirty_ = true;
}

void SourceEditor::insert(const std::string& s) {
    if (readOnly_ || s.empty()) return;
    pushUndo();
    ensureCursor();
    lines_[(size_t)cursorRow_].insert((size_t)cursorCol_, s);
    cursorCol_ += (int)s.size();
    dirty_ = true;
}

void SourceEditor::backspace() {
    if (readOnly_) return;
    ensureCursor();
    if (cursorCol_ > 0) {
        pushUndo();
        lines_[(size_t)cursorRow_].erase((size_t)cursorCol_ - 1, 1);
        cursorCol_--;
        dirty_ = true;
    } else if (cursorRow_ > 0) {
        pushUndo();
        cursorCol_ = (int)lines_[(size_t)cursorRow_ - 1].size();
        lines_[(size_t)cursorRow_ - 1] += lines_[(size_t)cursorRow_];
        lines_.erase(lines_.begin() + cursorRow_);
        cursorRow_--;
        dirty_ = true;
    }
}

void SourceEditor::del() {
    if (readOnly_) return;
    ensureCursor();
    auto& line = lines_[(size_t)cursorRow_];
    if (cursorCol_ < (int)line.size()) {
        pushUndo();
        line.erase((size_t)cursorCol_, 1);
        dirty_ = true;
    } else if (cursorRow_ + 1 < (int)lines_.size()) {
        pushUndo();
        line += lines_[(size_t)cursorRow_ + 1];
        lines_.erase(lines_.begin() + cursorRow_ + 1);
        dirty_ = true;
    }
}

void SourceEditor::newLine() {
    if (readOnly_) return;
    pushUndo();
    ensureCursor();
    std::string& line = lines_[(size_t)cursorRow_];
    std::string rest = line.substr((size_t)cursorCol_);
    line.erase((size_t)cursorCol_);
    lines_.insert(lines_.begin() + cursorRow_ + 1, rest);
    cursorRow_++;
    cursorCol_ = 0;
    dirty_ = true;
}

bool SourceEditor::toggleBreakpointAtCursor() {
    ensureCursor();
    int line = cursorRow_ + 1;
    if (breakpoints_.count(line)) breakpoints_.erase(line);
    else breakpoints_.insert(line);
    if (onBreakpointChanged) onBreakpointChanged();
    return breakpoints_.count(line) != 0;
}

Color SourceEditor::tokenColor(int type) const {
    auto t = (TokenType)type;
    if (isKeywordToken(t)) return theme::keyword();
    if (isFuncToken(t)) return theme::func();
    if (t == TOK_NUMBER || t == TOK_TRUE || t == TOK_FALSE) return theme::number();
    if (t == TOK_STRING) return theme::string();
    return theme::fg();
}

Element SourceEditor::renderLine(int index, int width) const {
    const std::string& raw = lines_[(size_t)index];
    int line1 = index + 1;
    bool isCurrent = currentLine_ == line1;
    bool isError = errorLine_ == line1;
    bool isBreak = breakpoints_.count(line1) != 0;

    std::string gutter = isBreak ? "●" : " ";
    if (isCurrent) gutter = "►";
    if (isBreak && isCurrent) gutter = "●";

    char num[8];
    std::snprintf(num, sizeof(num), "%4d ", line1);
    auto g = hbox(
        ftxui::text(gutter) | color(isBreak ? theme::error() : theme::dim()) | size(WIDTH, EQUAL, 2),
        ftxui::text(num) | color(isCurrent ? theme::accent() : theme::dim())
    );

    Elements parts;
    BasicLexer lexer(String(raw.c_str()));
    auto tokens = lexer.tokenize();

    std::vector<Color> cols(raw.size(), theme::fg());
    std::vector<bool> set(raw.size(), false);
    for (const auto& tok : tokens) {
        if (tok.type == TOK_EOF || tok.type == TOK_NEWLINE) continue;
        int start = tok.column - 1;
        if (start < 0) start = 0;
        std::string val = tok.value.c_str();
        if (tok.type == TOK_STRING) val = std::string("\"") + val + "\"";
        Color c = tokenColor((int)tok.type);
        for (int i = 0; i < (int)val.size(); i++) {
            int p = start + i;
            if (p >= 0 && p < (int)raw.size()) {
                cols[(size_t)p] = c;
                set[(size_t)p] = true;
            }
        }
    }
    for (size_t i = 0; i < raw.size(); i++) {
        if (raw[i] == '#' || (raw[i] == '/' && i + 1 < raw.size() && raw[i + 1] == '/')) {
            bool inString = set[i] && cols[i] == theme::string();
            if (!inString) {
                for (size_t j = i; j < raw.size(); j++) cols[j] = theme::comment();
                break;
            }
        }
    }

    if (raw.empty()) {
        parts.push_back(ftxui::text(" "));
    } else {
        size_t i = 0;
        while (i < raw.size()) {
            Color c = cols[i];
            size_t j = i + 1;
            while (j < raw.size() && cols[j] == c) j++;
            parts.push_back(ftxui::text(raw.substr(i, j - i)) | color(c));
            i = j;
        }
    }

    Element body = hbox(std::move(parts));
    if (isCurrent) body = body | bgcolor(theme::current());
    if (isError) body = body | color(theme::error());

    Element row = hbox(g, body | flex);
    if (index == cursorRow_ && Focused()) {
        row = row | inverted;
    }
    (void)width;
    return row;
}

Element SourceEditor::OnRender() {
    ensureCursor();
    int height = std::max(1, box_.y_max - box_.y_min + 1);
    int width = std::max(1, box_.x_max - box_.x_min + 1);
    if (cursorRow_ < scrollRow_) scrollRow_ = cursorRow_;
    if (cursorRow_ >= scrollRow_ + height) scrollRow_ = cursorRow_ - height + 1;
    if (scrollRow_ < 0) scrollRow_ = 0;

    Elements rows;
    int last = std::min((int)lines_.size(), scrollRow_ + height);
    for (int i = scrollRow_; i < last; i++) {
        rows.push_back(renderLine(i, width));
    }
    while ((int)rows.size() < height) {
        rows.push_back(ftxui::text(" ") | color(theme::dim()));
    }
    return vbox(std::move(rows)) | bgcolor(theme::bg()) | reflect(box_);
}

bool SourceEditor::OnEvent(Event event) {
    if (event.is_mouse()) {
        auto m = event.mouse();
        if (!box_.Contain(m.x, m.y)) return false;
        TakeFocus();
        int row = scrollRow_ + (m.y - box_.y_min);
        if (row < 0) row = 0;
        if (row >= (int)lines_.size()) row = (int)lines_.size() - 1;
        cursorRow_ = row;
        int localX = m.x - box_.x_min;
        if (m.button == Mouse::Left && m.motion == Mouse::Pressed && localX <= 1) {
            toggleBreakpointAtCursor();
            return true;
        }
        int col = localX - 7;
        if (col < 0) col = 0;
        cursorCol_ = col;
        ensureCursor();
        return true;
    }

    if (event == Event::ArrowUp) {
        cursorRow_--;
        ensureCursor();
        return true;
    }
    if (event == Event::ArrowDown) {
        cursorRow_++;
        ensureCursor();
        return true;
    }
    if (event == Event::ArrowLeft) {
        if (cursorCol_ > 0) cursorCol_--;
        else if (cursorRow_ > 0) {
            cursorRow_--;
            cursorCol_ = (int)lines_[(size_t)cursorRow_].size();
        }
        return true;
    }
    if (event == Event::ArrowRight) {
        ensureCursor();
        if (cursorCol_ < (int)lines_[(size_t)cursorRow_].size()) cursorCol_++;
        else if (cursorRow_ + 1 < (int)lines_.size()) {
            cursorRow_++;
            cursorCol_ = 0;
        }
        return true;
    }
    if (event == Event::Home) {
        cursorCol_ = 0;
        return true;
    }
    if (event == Event::End) {
        ensureCursor();
        cursorCol_ = (int)lines_[(size_t)cursorRow_].size();
        return true;
    }
    if (event == Event::PageUp) {
        cursorRow_ -= 20;
        ensureCursor();
        return true;
    }
    if (event == Event::PageDown) {
        cursorRow_ += 20;
        ensureCursor();
        return true;
    }
    if (event == Event::Return) {
        newLine();
        return true;
    }
    if (event == Event::Backspace) {
        backspace();
        return true;
    }
    if (event == Event::Delete) {
        del();
        return true;
    }
    if (event == Event::Tab) {
        insert("  ");
        return true;
    }
    if (event == Event::CtrlZ || event == Event::Special("\x1a")) {
        undo();
        return true;
    }
    if (event.is_character()) {
        std::string ch = event.character();
        if (ch == "\t") {
            insert("  ");
            return true;
        }
        if (!ch.empty() && ch[0] >= 32) {
            insert(ch);
            return true;
        }
    }
    return false;
}

Component MakeEditor(std::shared_ptr<SourceEditor> editor) {
    return editor;
}
