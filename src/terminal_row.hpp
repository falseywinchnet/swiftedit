#pragma once
#include "terminal_buffer.hpp"
#include "terminal_cells.hpp"
namespace swiftedit {
struct TerminalRun {
    std::size_t column{}, cells{};
    std::string text{};
    SourceRange source{};
    bool selected{};
};
struct TerminalRow {
    std::vector<TerminalRun> runs{};
    std::optional<std::size_t> caret_column{};
    std::size_t total_cells{};
    bool clipped_left{}, clipped_right{};
};
// Logical-line viewport, with horizontal clipping in cells. Never slices a
// printable source grapheme. A partially visible wide glyph occupies blanks.
// Labels may be visually clipped; their source/edit range stays atomic.
[[nodiscard]] TerminalRow terminal_row(TerminalBuffer &, std::size_t line, std::size_t first_column,
                                       std::size_t width);
} // namespace swiftedit
