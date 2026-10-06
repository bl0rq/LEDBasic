#include "editor.h"
#include "tui_test.h"

#include <memory>

using namespace ftxui;

namespace {

std::shared_ptr<SourceEditor> makeEditor(const std::string& text) {
    auto e = std::make_shared<SourceEditor>();
    e->setText(text);
    return e;
}

void testSetGetText() {
    auto e = makeEditor("a\r\nb\r\n");
    CHECK(e->getText() == "a\nb\n", "setText strips CR and keeps trailing newline");
    CHECK(!e->dirty(), "setText leaves buffer clean");
    CHECK(e->cursorLine() == 1 && e->cursorColumn() == 1, "setText puts caret at 1:1");
}

void testTyping() {
    auto e = makeEditor("");
    typeText(*e, "fill()");
    CHECK(e->getText() == "fill()", "characters insert at caret");
    CHECK(e->dirty(), "typing marks buffer dirty");
    CHECK(e->cursorColumn() == 7, "caret advances past typed text");

    e->OnEvent(Event::Home);
    typeText(*e, "  ");
    CHECK(e->getText() == "  fill()", "Home then type inserts at line start");

    e->OnEvent(Event::Tab);
    CHECK(e->getText() == "    fill()", "Tab inserts two spaces");
}

void testNewlineAndJoin() {
    auto e = makeEditor("abcd");
    e->OnEvent(Event::ArrowRight);
    e->OnEvent(Event::ArrowRight);
    e->OnEvent(Event::Return);
    CHECK(e->getText() == "ab\ncd", "Return splits the line at the caret");
    CHECK(e->cursorLine() == 2 && e->cursorColumn() == 1, "caret moves to start of new line");

    e->OnEvent(Event::Backspace);
    CHECK(e->getText() == "abcd", "Backspace at column 1 joins with previous line");
    CHECK(e->cursorLine() == 1 && e->cursorColumn() == 3, "caret lands at the join point");

    e->OnEvent(Event::Backspace);
    CHECK(e->getText() == "acd", "Backspace deletes the character before the caret");

    e->OnEvent(Event::Delete);
    CHECK(e->getText() == "ad", "Delete removes the character at the caret");

    auto f = makeEditor("x\ny");
    f->OnEvent(Event::End);
    f->OnEvent(Event::Delete);
    CHECK(f->getText() == "xy", "Delete at end of line joins the next line");
}

void testNavigation() {
    auto e = makeEditor("one\ntwo\nthree");
    e->OnEvent(Event::End);
    e->OnEvent(Event::ArrowRight);
    CHECK(e->cursorLine() == 2 && e->cursorColumn() == 1, "Right at end of line wraps to next line");
    e->OnEvent(Event::ArrowLeft);
    CHECK(e->cursorLine() == 1 && e->cursorColumn() == 4, "Left at line start wraps to previous end");

    e->OnEvent(Event::ArrowDown);
    e->OnEvent(Event::ArrowDown);
    e->OnEvent(Event::ArrowDown);
    CHECK(e->cursorLine() == 3, "Down clamps at last line");
    CHECK(e->cursorColumn() == 4, "column is kept when it fits the line");

    e->OnEvent(Event::PageUp);
    CHECK(e->cursorLine() == 1, "PageUp clamps at first line");

    e->goToLine(99);
    CHECK(e->cursorLine() == 3, "goToLine clamps past the end");
    e->goToLine(0);
    CHECK(e->cursorLine() == 1, "goToLine clamps before the start");
}

void testUndo() {
    auto e = makeEditor("ab");
    e->OnEvent(Event::End);
    typeText(*e, "c");
    e->OnEvent(Event::Return);
    CHECK(e->getText() == "abc\n", "edits applied before undo");
    e->OnEvent(Event::CtrlZ);
    CHECK(e->getText() == "abc", "Ctrl+Z undoes the newline");
    e->OnEvent(Event::CtrlZ);
    CHECK(e->getText() == "ab", "Ctrl+Z undoes the insert");
    e->OnEvent(Event::CtrlZ);
    CHECK(e->getText() == "ab", "Ctrl+Z with empty history is a no-op");
}

void testReadOnly() {
    auto e = makeEditor("abc");
    e->setReadOnly(true);
    typeText(*e, "x");
    e->OnEvent(Event::Return);
    e->OnEvent(Event::Delete);
    CHECK(e->getText() == "abc", "read-only buffer ignores edits");
    CHECK(!e->dirty(), "read-only buffer stays clean");
    e->OnEvent(Event::ArrowRight);
    CHECK(e->cursorColumn() == 2, "caret still moves when read-only");
}

void testBreakpoints() {
    auto e = makeEditor("a\nb\nc");
    int changes = 0;
    e->onBreakpointChanged = [&] { changes++; };
    e->goToLine(2);
    CHECK(e->toggleBreakpointAtCursor(), "toggle on returns true");
    CHECK(e->breakpoints().count(2) == 1, "breakpoint recorded on caret line");
    CHECK(!e->toggleBreakpointAtCursor(), "toggle off returns false");
    CHECK(e->breakpoints().empty(), "breakpoint removed");
    CHECK(changes == 2, "onBreakpointChanged fires on every toggle");
}

void testRender() {
    auto e = makeEditor("setup\n  clear()\nend");
    e->setBreakpoints({2});
    e->setCurrentLine(3);
    std::string screen = renderText(*e, 30, 5);
    CHECK(contains(screen, "   1 setup"), "line numbers render in the gutter");
    CHECK(contains(screen, "●    2   clear()"), "breakpoint glyph renders on its line");
    CHECK(contains(screen, "►    3 end"), "current-line marker renders");
}

void testRenderScrollsToCaret() {
    std::string text;
    for (int i = 1; i <= 20; i++) text += "x" + std::to_string(i) + "\n";
    auto e = makeEditor(text);
    renderText(*e, 20, 4);
    e->goToLine(15);
    std::string screen = renderText(*e, 20, 4);
    CHECK(contains(screen, "  15 x15"), "view scrolls to keep the caret visible");
    CHECK(!contains(screen, "   1 x1"), "top lines scroll out of view");
}

void testMouse() {
    auto e = makeEditor("aaaa\nbbbb\ncccc");
    renderText(*e, 30, 5);
    e->OnEvent(click(9, 1));
    CHECK(e->cursorLine() == 2, "click selects the clicked row");
    CHECK(e->cursorColumn() == 3, "click places caret after the 7-column gutter");

    e->OnEvent(click(0, 2));
    CHECK(e->breakpoints().count(3) == 1, "click in gutter toggles a breakpoint");

    CHECK(!e->OnEvent(click(50, 1)), "click outside the editor is not consumed");
}

} // namespace

void runEditorTests() {
    testSetGetText();
    testTyping();
    testNewlineAndJoin();
    testNavigation();
    testUndo();
    testReadOnly();
    testBreakpoints();
    testRender();
    testRenderScrollsToCaret();
    testMouse();
}
