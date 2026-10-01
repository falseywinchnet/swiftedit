#include "terminal_buffer.hpp"
#include "terminal_row.hpp"
#include <iostream>
#include <fstream>
#include <windows.h>
#include <stdexcept>
namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        swiftedit::TerminalBuffer terminal{};
        terminal.move(swiftedit::TerminalMotion::left);
        terminal.move(swiftedit::TerminalMotion::right);
        terminal.erase(true);
        check(terminal.selection().caret == 0 && terminal.session().text().empty(),
              "Empty boundary navigation is inert");
        terminal.insert("e\xcc\x81\r\n\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0"
                        "\x9f\x91\xa7");
        terminal.move(swiftedit::TerminalMotion::left);
        check(terminal.selection().caret == 5, "Family emoji is one movement");
        terminal.move(swiftedit::TerminalMotion::left);
        check(terminal.selection().caret == 3, "CRLF is one movement");
        terminal.erase(true);
        check(terminal.session().text().starts_with("\r\n"),
              "Backspace removes complete combining grapheme");
        check(terminal.undo(), "Terminal edit uses Session undo");
        terminal.move(swiftedit::TerminalMotion::document_start);
        terminal.move(swiftedit::TerminalMotion::right, true);
        check(terminal.selected_text() == "e\xcc\x81", "Selection preserves complete grapheme");
        terminal.insert("Q");
        check(terminal.session().text().starts_with("Q\r\n"),
              "Selection replacement is one source edit");
        check(terminal.undo() && terminal.session().text().starts_with("e\xcc\x81"),
              "Replacement undo restores source");
        terminal.reset(true);
        terminal.insert("abcd\nx\nabcdef");
        terminal.move(swiftedit::TerminalMotion::document_start);
        terminal.move(swiftedit::TerminalMotion::end);
        terminal.move(swiftedit::TerminalMotion::down);
        check(terminal.selection().caret == 6, "Vertical movement clamps to short line");
        terminal.move(swiftedit::TerminalMotion::down);
        check(terminal.selection().caret == 11,
              "Vertical movement preserves desired grapheme column");
        terminal.move(swiftedit::TerminalMotion::up, true, 2);
        check(terminal.selected_text() == "\nx\nabcd",
              "Page-style selection traverses logical rows");
        terminal.move(swiftedit::TerminalMotion::left);
        check(terminal.selection().caret == 4 && terminal.selected_range().length == 0,
              "Left collapses selection to start");
        const std::string cut = terminal.cut();
        check(cut == "abcd\n" && terminal.session().text() == "x\nabcdef",
              "Cut without selection removes one full logical line");
        terminal.insert(cut);
        check(terminal.session().text() == "abcd\nx\nabcdef", "Internal cut text pastes exactly");
        terminal.reset(true);
        const std::string malformed("\xff\xcc\x81Z", 4);
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() /
            ("swiftedit-terminal-" + std::to_string(GetCurrentProcessId()));
        check(std::filesystem::create_directory(dir), "Unique terminal fixture");
        struct Cleanup {
            std::filesystem::path path{};
            ~Cleanup() {
                std::error_code error{};
                std::filesystem::remove_all(path, error);
            }
        } cleanup{dir};
        const std::filesystem::path path = dir / "malformed.txt";
        {
            std::ofstream file(path, std::ios::binary);
            file.write(malformed.data(), static_cast<std::streamsize>(malformed.size()));
            check(static_cast<bool>(file), "Malformed fixture written");
        }
        terminal.open(path);
        terminal.move(swiftedit::TerminalMotion::document_start);
        terminal.erase(false);
        check(
            terminal.session().text() == std::string("\xcc\x81Z", 3),
            "Illegal byte is independently deletable without swallowing following combining text");
        check(terminal.undo() && terminal.session().text() == malformed,
              "Illegal bytes survive undo");
        terminal.select_all();
        check(terminal.selected_text() == malformed, "Copy returns exact source bytes");
        terminal.move(swiftedit::TerminalMotion::document_end);
        terminal.insert("changed");
        const std::string dirty = terminal.session().text();
        bool discard_refused = false;
        try {
            terminal.reset();
        } catch (const std::exception &) {
            discard_refused = true;
        }
        check(discard_refused && terminal.session().text() == dirty,
              "New document requires explicit dirty discard");
        terminal.reset(true);
        terminal.insert("\xf0\x9f\x98\x80");
        terminal.select_all();
        terminal.insert("ab");
        check(terminal.undo() && terminal.selection().caret == 0,
              "Undo clamps a caret inside a restored multibyte grapheme to its start");
        terminal.move(swiftedit::TerminalMotion::document_end);
        terminal.enter();
        check(terminal.session().text().ends_with("\r\n"),
              "New terminal documents use native Enter endings");
        const std::filesystem::path saved = dir / "saved.txt";
        terminal.save_as(saved);
        check(!terminal.session().dirty() && !terminal.undo(),
              "Terminal save establishes shared undo boundary");
        check(swiftedit::terminal_glyph("\xe4\xb8\xad", 0).cells == 2,
              "CJK uses two terminal cells");
        check(swiftedit::terminal_glyph("e\xcc\x81", 0).cells == 1,
              "Combining mark does not add a cell");
        check(swiftedit::terminal_glyph("\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9", 0).cells ==
                  2,
              "Joined emoji occupies two cells");
        check(swiftedit::terminal_glyph("\t", 3).text == " ", "Tabs use four-cell stops");
        const swiftedit::TerminalGlyph escape = swiftedit::terminal_glyph("\x1b", 0);
        check(escape.label && escape.text == "[U+001B]", "Escape control is visible inert text");
        check(swiftedit::terminal_glyph("\xe2\x80\xae", 0).text == "[U+202E]",
              "Bidi override is visibly labeled");
        check(swiftedit::terminal_glyph(std::string("\xff", 1), 0).text == "[BYTE FF]",
              "Malformed byte is visibly labeled");
        terminal.reset();
        terminal.insert("A\xe4\xb8\xad\tZ");
        terminal.move(swiftedit::TerminalMotion::document_start);
        terminal.move(swiftedit::TerminalMotion::right);
        terminal.move(swiftedit::TerminalMotion::right, true);
        const swiftedit::TerminalRow row = swiftedit::terminal_row(terminal, 0, 0, 8);
        check(row.total_cells == 5 && row.caret_column == 3 && row.runs.size() == 4 &&
                  row.runs[1].selected && row.runs[1].cells == 2 && row.runs[2].cells == 1,
              "Source selection and caret map to terminal cell columns");
        const swiftedit::TerminalRow clipped = swiftedit::terminal_row(terminal, 0, 2, 2);
        check(clipped.runs[0].text == " " && clipped.runs[0].source.length == 3 &&
                  clipped.clipped_left && clipped.clipped_right && clipped.caret_column == 1,
              "Partial wide glyph never emits a broken character or changes its source range");
        std::cout << "Terminal navigation and shared edit tests passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
