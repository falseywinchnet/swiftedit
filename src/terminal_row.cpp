#include "terminal_row.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace swiftedit {
TerminalRow terminal_row(TerminalBuffer &buffer, std::size_t line, std::size_t first_column,
                         std::size_t width) {
    if (!width || width > 1000 || first_column > std::numeric_limits<std::size_t>::max() - width)
        throw std::runtime_error("Terminal row width must be 1..1000 cells without overflow.");
    const SourceRange range = buffer.line_range(line);
    const TerminalSelection selection = buffer.selection();
    const SourceRange selected = buffer.selected_range();
    const std::string &source = buffer.session().text();
    TerminalRow result{};
    result.runs.reserve(width);
    std::size_t column = 0;
    const std::size_t visible_end = first_column + width;
    for (std::size_t offset = range.offset; offset < range.offset + range.length;) {
        const SourceRange grapheme = buffer.grapheme_range(offset);
        const std::string_view bytes(source.data() + grapheme.offset, grapheme.length);
        TerminalGlyph glyph = terminal_glyph(bytes, column);
        if (selection.caret == offset && column >= first_column && column < visible_end)
            result.caret_column = column - first_column;
        const std::size_t end = column + glyph.cells;
        if (column < visible_end && end > first_column) {
            const std::size_t visible_start = std::max(column, first_column);
            const std::size_t visible_stop = std::min(end, visible_end);
            TerminalRun run{};
            run.column = visible_start - first_column;
            run.cells = visible_stop - visible_start;
            run.source = grapheme;
            run.selected = selected.length && offset < selected.offset + selected.length &&
                           offset + grapheme.length > selected.offset;
            if (visible_start == column && visible_stop == end)
                run.text = std::move(glyph.text);
            else if (glyph.label || bytes == "\t")
                run.text = glyph.text.substr(visible_start - column, run.cells);
            else
                run.text.assign(run.cells, ' ');
            result.runs.push_back(std::move(run));
        }
        column = end;
        offset += grapheme.length;
    }
    if (selection.caret == range.offset + range.length && column >= first_column &&
        column < visible_end)
        result.caret_column = column - first_column;
    result.total_cells = column;
    result.clipped_left = first_column > 0 && column > 0;
    result.clipped_right = column > visible_end;
    return result;
}
} // namespace swiftedit
