#include "terminal_wrap.hpp"
#include "platform.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool good, const char *message) {
    if (!good)
        throw std::runtime_error(message);
}
void verify(swiftedit::TerminalBuffer &buffer, std::size_t line, std::size_t width,
            const std::vector<std::string> &expected) {
    const swiftedit::DocumentStamp stamp = buffer.session().stamp();
    const swiftedit::SourceRange logical = buffer.line_range(line);
    std::size_t offset = logical.offset;
    bool finished = false;
    for (std::size_t index = 0; index < expected.size(); ++index) {
        check(!finished, "Unexpected extra expected visual row");
        const swiftedit::TerminalWrappedRow row =
            swiftedit::terminal_wrapped_row(buffer, line, offset, width);
        check(row.span.source.offset == offset, "Visual rows are contiguous source spans");
        check(buffer.session().text().substr(offset, row.span.source.length) == expected[index],
              "Wrapped row differs from expected word/grapheme boundary");
        check(row.span.logical_end || row.span.source.length != 0,
              "A continuing visual row must make source progress");
        for (const swiftedit::TerminalRun &run : row.display.runs) {
            check(run.column + run.cells <= width, "Rendered run exceeds viewport");
            check(run.source.offset >= offset &&
                      run.source.offset + run.source.length <= offset + row.span.source.length,
                  "Rendered grapheme escapes its visual row");
            check(run.text.find('\x1b') == std::string::npos &&
                      run.text.find('\t') == std::string::npos,
                  "Terminal controls must remain inert");
        }
        offset += row.span.source.length;
        finished = row.span.logical_end;
    }
    check(finished && offset == logical.offset + logical.length,
          "Wrapping must conserve every source byte in the logical line");
    check(buffer.session().stamp().identity == stamp.identity &&
              buffer.session().stamp().revision == stamp.revision,
          "Visual wrapping must not edit the document");
}
} // namespace
int main() {
    try {
        swiftedit::TerminalBuffer buffer{};
        verify(buffer, 0, 5, {""});
        buffer.insert("one two three");
        verify(buffer, 0, 8, {"one two ", "three"});
        verify(buffer, 0, 6, {"one ", "two ", "three"});
        buffer.reset(true);
        buffer.insert("abcdef");
        verify(buffer, 0, 3, {"abc", "def", ""});
        buffer.move(swiftedit::TerminalMotion::document_end);
        const swiftedit::TerminalWrappedRow full = swiftedit::terminal_wrapped_row(buffer, 0, 3, 3);
        const swiftedit::TerminalWrappedRow end = swiftedit::terminal_wrapped_row(buffer, 0, 6, 3);
        check(!full.display.caret_column && end.display.caret_column == 0,
              "Full-width end caret belongs to the empty continuation row");
        buffer.reset(true);
        buffer.insert("a\tb");
        verify(buffer, 0, 4, {"a\t", "b"});
        buffer.reset(true);
        buffer.insert("e\xcc\x81\xe7\x95\x8c");
        verify(buffer, 0, 2, {"e\xcc\x81", "\xe7\x95\x8c", ""});
        verify(buffer, 0, 1, {"e\xcc\x81", "\xe7\x95\x8c", ""});
        bool refused = false;
        try {
            static_cast<void>(swiftedit::terminal_wrap_span(buffer, 0, 1, 5));
        } catch (const std::exception &) {
            refused = true;
        }
        check(refused, "A wrap start inside a combining grapheme is refused");
        buffer.reset(true);
        buffer.insert("x\r\ny\nz");
        verify(buffer, 0, 4, {"x"});
        verify(buffer, 1, 4, {"y"});
        verify(buffer, 2, 4, {"z"});
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("swiftedit-wrap-" + std::to_string(test_process_id()) + ".txt");
        check(!std::filesystem::exists(path), "Unique malformed fixture");
        struct Cleanup {
            std::filesystem::path path{};
            ~Cleanup() {
                std::error_code error{};
                std::filesystem::remove(path, error);
            }
        } cleanup{path};
        {
            std::ofstream output(path, std::ios::binary);
            output << "a\xff\x1b";
        }
        buffer.reset(true);
        buffer.open(path);
        verify(buffer, 0, 1, {"a", "\xff", "\x1b", ""});
        check(buffer.session().illegal_bytes() == 1 && !buffer.session().dirty(),
              "Malformed byte and control remain literal unmodified source");
        std::cout << "Terminal wrap layout passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
