#include "terminal_horizontal.hpp"
#include "terminal_row.hpp"
#include "platform.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(const bool value, const char *const message) {
    if (!value)
        throw std::runtime_error(message);
}
struct Fixture {
    std::filesystem::path directory = std::filesystem::temp_directory_path() /
        ("swiftedit-horizontal-" + std::to_string(test_process_id()));
    Fixture() {
        require(std::filesystem::create_directory(directory), "Reserve unique horizontal fixture");
    }
    ~Fixture() {
        std::error_code ignored{};
        std::filesystem::remove_all(directory, ignored);
    }
    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;
    std::filesystem::path write(const std::string &name, const std::string &bytes) const {
        const std::filesystem::path path = directory / name;
        std::ofstream output(path, std::ios::binary);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        require(static_cast<bool>(output), "Write exact horizontal fixture");
        return path;
    }
};
void finish_logical(swiftedit::TerminalLogicalPage &page, const swiftedit::Session &session) {
    std::size_t steps = 0;
    while (!page.step(session)) {
        ++steps;
        require(steps <= session.size() / 8192 + 2, "Logical indexing progress");
    }
}
void compare_rows(const Fixture &fixture, const std::string &source) {
    const std::filesystem::path path = fixture.write("short.txt", source);
    swiftedit::TerminalBuffer buffer{};
    buffer.open(path);
    const swiftedit::Session &session = buffer.session();
    swiftedit::TerminalLogicalPage page(session, 0, 300);
    finish_logical(page, session);
    const std::vector<swiftedit::TerminalLogicalRow> &rows = page.result(session);
    for (std::size_t row = 0; row < rows.size(); ++row) {
        for (std::size_t left = 0; left < 32; ++left) {
            for (const std::size_t width : {1U, 2U, 7U}) {
                const swiftedit::TerminalRow expected = swiftedit::terminal_row(buffer, row, left, width);
                for (const std::size_t budget : {1U, 2U, 7U, 8192U}) {
                    swiftedit::TerminalHorizontalLine line(session, page, row, left, width);
                    bool complete = false;
                    std::size_t steps = 0;
                    while (!complete) {
                        const std::uint64_t before = line.read_offset();
                        complete = line.step(session, budget);
                        require(line.read_offset() - before <= budget, "Horizontal source read bound");
                        ++steps;
                        require(steps <= source.size() + 2, "Horizontal task makes progress");
                    }
                    const swiftedit::TerminalHorizontalFrame &actual = line.result(session);
                    require(actual.runs.size() == expected.runs.size(), "Same visible grapheme runs");
                    for (std::size_t index = 0; index < actual.runs.size(); ++index) {
                        const swiftedit::TerminalPageRun &run = actual.runs[index];
                        const swiftedit::TerminalRun &wanted = expected.runs[index];
                        require(run.row == 0 && run.column == wanted.column && run.cells == wanted.cells &&
                                    run.text == wanted.text && run.source_offset == wanted.source.offset &&
                                    run.source_length == wanted.source.length,
                                "Paged horizontal rendering matches established editable clipping policy");
                        require(run.text.find('\x1b') == std::string::npos &&
                                    run.text.find('\0') == std::string::npos,
                                "Control source never becomes an executable terminal sequence");
                    }
                    require(actual.clipped_left == expected.clipped_left &&
                                actual.clipped_right == expected.clipped_right,
                            "Horizontal clipping flags match visible content");
                    if (actual.total_cells)
                        require(*actual.total_cells == expected.total_cells, "Completed line width is exact");
                    for (const swiftedit::TerminalHorizontalCaret &caret : actual.carets) {
                        std::uint64_t offset = rows[row].offset;
                        std::uint64_t column = 0;
                        const std::uint64_t end = offset + rows[row].length;
                        while (offset < caret.source_offset && offset < end) {
                            const swiftedit::SourceRange grapheme = buffer.grapheme_range(
                                static_cast<std::size_t>(offset));
                            const std::string_view bytes(source.data() + grapheme.offset, grapheme.length);
                            const swiftedit::TerminalGlyph glyph = swiftedit::terminal_glyph(
                                bytes, static_cast<std::size_t>(column % 4));
                            offset += grapheme.length;
                            column += glyph.cells;
                        }
                        require(offset == caret.source_offset && column == caret.column,
                                "Every retained caret is an exact source grapheme boundary");
                    }
                    require(line.step(session, budget), "Completed horizontal task is idempotent");
                }
            }
        }
    }
}
void check_refusals(const Fixture &fixture) {
    swiftedit::Session session{};
    session.open(fixture.write("refusal.txt", "a\tbc"));
    swiftedit::TerminalLogicalPage pending(session, 0, 1);
    bool refused = false;
    try {
        const swiftedit::TerminalHorizontalLine invalid(session, pending, 0, 0, 10);
    } catch (const std::runtime_error &) {
        refused = true;
    }
    require(refused, "Incomplete logical page cannot be rendered");
    finish_logical(pending, session);
    for (const std::size_t width : {0U, 1001U}) {
        refused = false;
        try {
            const swiftedit::TerminalHorizontalLine invalid(session, pending, 0, 0, width);
        } catch (const std::runtime_error &) {
            refused = true;
        }
        require(refused, "Invalid horizontal width refused");
    }
    refused = false;
    try {
        const swiftedit::TerminalHorizontalLine invalid(
            session, pending, 0, std::numeric_limits<std::uint64_t>::max(), 1);
    } catch (const std::runtime_error &) {
        refused = true;
    }
    require(refused, "Horizontal endpoint overflow refused");
    swiftedit::TerminalHorizontalLine line(session, pending, 0, 0, 10);
    refused = false;
    try {
        static_cast<void>(line.result(session));
    } catch (const std::runtime_error &) {
        refused = true;
    }
    require(refused, "Incomplete horizontal frame is private");
    for (const std::size_t budget : {0U, 8193U}) {
        refused = false;
        try {
            static_cast<void>(line.step(session, budget));
        } catch (const std::runtime_error &) {
            refused = true;
        }
        require(refused && line.read_offset() == 0, "Invalid budget preserves work");
    }
    require(!line.step(session, 1), "Unknown trailing grapheme remains unpublished");
    session.replace_ranges({{0, 1}}, "z", session.stamp());
    refused = false;
    try {
        static_cast<void>(line.step(session));
    } catch (const std::runtime_error &) {
        refused = true;
    }
    require(refused && line.read_offset() == 1, "Stale horizontal task cannot consume new bytes");
}
void check_context_limit(const Fixture &fixture) {
    std::string source = "a";
    for (std::size_t index = 0; index < 33000; ++index)
        source += "\xcc\x81";
    source += 'b';
    swiftedit::Session session{};
    session.open(fixture.write("cluster.txt", source));
    swiftedit::TerminalLogicalPage page(session, 0, 1);
    finish_logical(page, session);
    swiftedit::TerminalHorizontalLine line(session, page, 0, 0, 10);
    bool refused = false;
    try {
        for (std::size_t step = 0; step < 9; ++step)
            static_cast<void>(line.step(session));
    } catch (const std::runtime_error &) {
        refused = true;
    }
    require(refused && line.read_offset() == 65536, "Oversized grapheme refuses at bounded context");
    refused = false;
    try {
        static_cast<void>(line.result(session));
    } catch (const std::runtime_error &) {
        refused = true;
    }
    require(refused && session.text() == source, "Failure cannot publish partial text or alter source");
}
void check_large_file(const Fixture &fixture) {
    const std::filesystem::path path = fixture.directory / "large.txt";
    {
        const std::string chunk(8192, 'x');
        std::ofstream output(path, std::ios::binary);
        for (std::size_t index = 0; index < 2048; ++index)
            output.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        output << "\r\nend";
        output.close();
        require(static_cast<bool>(output), "Write read-only horizontal fixture");
    }
    swiftedit::Session session{};
    session.open(path);
    require(session.read_only(), "Horizontal renderer uses an actual read-only Session");
    swiftedit::TerminalLogicalPage page(session, 0, 2);
    finish_logical(page, session);
    {
        swiftedit::TerminalHorizontalLine cancelled(session, page, 0, 8190, 12);
        require(!cancelled.step(session) && cancelled.read_offset() == 8192,
                "Horizontal seek yields at the read budget before its target");
    }
    require(!session.dirty(), "Dropping unfinished horizontal work preserves source");
    swiftedit::TerminalHorizontalLine line(session, page, 0, 8190, 12);
    require(!line.step(session) && line.step(session), "Viewport prefix needs only two bounded reads");
    const swiftedit::TerminalHorizontalFrame &frame = line.result(session);
    require(frame.runs.size() == 12 && line.read_offset() == 16384 && !frame.total_cells &&
                frame.clipped_left && frame.clipped_right,
            "Long-line rendering stops after the visible horizontal window");
    for (std::size_t index = 0; index < frame.runs.size(); ++index) {
        const swiftedit::TerminalPageRun &run = frame.runs[index];
        require(run.column == index && run.source_offset == 8190 + index &&
                    run.source_length == 1 && run.cells == 1 && run.text == "x",
                "Large-file horizontal source mapping remains exact");
    }
    swiftedit::TerminalHorizontalLine following(session, page, 1, 0, 10);
    require(following.step(session) && following.result(session).total_cells == 3,
            "Following short logical row renders independently");
    session.reset();
    bool refused = false;
    try {
        static_cast<void>(line.result(session));
    } catch (const std::runtime_error &) {
        refused = true;
    }
    require(refused, "Completed horizontal result cannot outlive its source identity");
}
void check_viewport(const Fixture &fixture) {
    const std::string source = "abcdef\r\n\tZ\nlast\n";
    swiftedit::Session session{};
    session.open(fixture.write("viewport.txt", source));
    swiftedit::TerminalHorizontalPage page(session, 0, 2, 2, 3);
    bool refused = false;
    try {
        static_cast<void>(page.result(session));
    } catch (const std::runtime_error &) {
        refused = true;
    }
    require(refused, "Incomplete viewport remains unpublished");
    std::size_t steps = 0;
    while (!page.step(session, 1)) {
        ++steps;
        require(steps < 50, "Cooperative viewport advances");
    }
    const std::vector<swiftedit::TerminalHorizontalFrame> &frames = page.result(session);
    require(frames.size() == 2 && frames[0].runs.size() == 3 &&
                frames[0].runs[0].text == "c" && frames[0].runs[2].text == "e",
            "Viewport retains exact horizontal slice");
    require(frames[1].runs.size() == 2 && frames[1].runs[0].text == "  " &&
                frames[1].runs[1].text == "Z" && frames[1].runs[1].column == 2,
            "Rows share horizontal coordinates and inert tab expansion");
    require(page.rows(session)[1].offset == 8 && page.next(session) == 11 && page.more(session),
            "Viewport pagination retains logical CRLF offsets");
    swiftedit::TerminalHorizontalPage tail(session, page.next(session), 2, 0, 10);
    while (!tail.step(session, 2)) {}
    require(tail.result(session).size() == 2 && tail.result(session)[1].total_cells == 0 &&
                !tail.more(session) && tail.next(session) == source.size(),
            "Last separator retains empty EOF row");
    {
        swiftedit::TerminalHorizontalPage cancelled(session, 0, 2, 0, 10);
        require(!cancelled.step(session, 1), "Viewport cancellation occurs between bounded steps");
    }
    require(page.result(session)[0].runs[0].text == "c" && session.text() == source,
            "Cancelling replacement leaves published viewport and source intact");
    session.reset();
    refused = false;
    try {
        static_cast<void>(page.result(session));
    } catch (const std::runtime_error &) {
        refused = true;
    }
    require(refused, "Viewport refuses stale source identity");
}
} // namespace
int main() {
    try {
        const Fixture fixture{};
        compare_rows(fixture, "");
        compare_rows(fixture, "ordinary text\r\n\tend\n");
        compare_rows(fixture, "a\t\xe6\xbc\xa2" "e\xcc\x81-\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb");
        compare_rows(fixture, std::string("\0\xff\x1b\tX", 5));
        compare_rows(fixture, "\xf0\x9f\x87\xba\xf0\x9f\x87\xb8\xf0\x9f\x87\xa8\xf0\x9f\x87\xa6-end");
        check_refusals(fixture);
        check_context_limit(fixture);
        check_large_file(fixture);
        check_viewport(fixture);
        std::cout << "Bounded horizontal rendering matches source, width and inert-control policies.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
