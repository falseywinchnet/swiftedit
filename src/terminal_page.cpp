#include "terminal_page.hpp"
#include "terminal_cells.hpp"
#include <algorithm>
#include <gui_forms/text.hpp>
#include <stdexcept>
namespace swiftedit {
namespace gf = gui_forms;
TerminalPageFrame terminal_page(const Session &session, TerminalPageCursor cursor,
                                std::size_t width, std::size_t rows) {
    if (!width || width > 1000 || !rows || rows > 300)
        throw std::runtime_error("Terminal page dimensions exceed their bounded range.");
    const Page source = session.page(cursor.offset, maximum_page);
    const bool eof = source.next == source.size;
    std::string metadata = source.bytes;
    for (std::size_t i = 0; i < metadata.size();) {
        const std::size_t length = utf8_sequence_length(metadata, i);
        if (length) {
            i += length;
            continue;
        }
        const unsigned char byte = static_cast<unsigned char>(metadata[i]);
        const std::size_t expected = byte >= 0xc2 && byte <= 0xdf   ? 2
                                     : byte >= 0xe0 && byte <= 0xef ? 3
                                     : byte >= 0xf0 && byte <= 0xf4 ? 4
                                                                    : 0;
        if (!eof && expected && metadata.size() - i < expected) {
            metadata.resize(i);
            break;
        }
        metadata[i] = '\x01';
        ++i;
    }
    const gf::TextStore text(metadata);
    std::size_t count = text.grapheme_count().value();
    if (!eof && count)
        --count;
    if (!eof && !count)
        throw std::runtime_error(
            "A complete grapheme exceeds the terminal page context limit. Source is unchanged.");
    TerminalPageFrame result{};
    result.runs.reserve(std::min<std::size_t>(metadata.size(), width * rows));
    result.next = cursor;
    result.row_starts.push_back(cursor);
    std::size_t row = 0;
    std::size_t column = 0;
    bool after_wrap = cursor.after_wrap;
    for (std::size_t index = 0; index < count && row < rows; ++index) {
        const gf::Utf8Range range = text.grapheme_range(gf::GraphemeIndex(index));
        const std::size_t offset = range.start.value();
        const std::size_t length = range.end.value() - offset;
        const std::string_view bytes(source.bytes.data() + offset, length);
        if (bytes == "\r" || bytes == "\n" || bytes == "\r\n") {
            if (index == 0 && cursor.label_cell)
                throw std::runtime_error("Invalid terminal label continuation.");
            if (!after_wrap)
                ++row;
            after_wrap = false;
            column = 0;
            result.next = {cursor.offset + offset + length, 0, false};
            if (row > result.row_starts.size() - 1)
                result.row_starts.push_back(result.next);
            continue;
        }
        after_wrap = false;
        TerminalGlyph glyph = terminal_glyph(bytes, column);
        std::size_t consumed = index == 0 ? cursor.label_cell : 0;
        if (consumed && (!glyph.label || consumed >= glyph.cells))
            throw std::runtime_error("Invalid terminal label continuation.");
        if (!glyph.label && glyph.cells > width)
            throw std::runtime_error("A printable grapheme is wider than this terminal. Enlarge "
                                     "the window; source is unchanged.");
        if (!glyph.label && column + glyph.cells > width) {
            ++row;
            column = 0;
            result.row_starts.push_back({cursor.offset + offset, 0, false});
            if (row == rows)
                break;
            // Tab stops depend on the row column after wrapping.
            if (bytes == "\t")
                glyph = terminal_glyph(bytes, 0);
        }
        while (consumed < glyph.cells && row < rows) {
            const std::size_t available = width - column;
            const std::size_t take = std::min(available, glyph.cells - consumed);
            TerminalPageRun run{};
            run.row = row;
            run.column = column;
            run.text = glyph.label ? glyph.text.substr(consumed, take) : glyph.text;
            result.runs.push_back(std::move(run));
            consumed += take;
            column += take;
            if (consumed == glyph.cells)
                result.next = {cursor.offset + offset + length, 0};
            else
                result.next = {cursor.offset + offset, consumed};
            if (column == width) {
                ++row;
                column = 0;
                after_wrap = true;
                result.next.after_wrap = true;
                result.row_starts.push_back(result.next);
            }
        }
    }
    result.more = result.next.offset < source.size || result.next.label_cell != 0;
    return result;
}
void TerminalPager::reset(const Session &session) {
    stamp_ = session.stamp();
    cursor_ = {};
    history_.clear();
    page_history_.clear();
    frame_ = {};
    ready_ = false;
}
const TerminalPageFrame &TerminalPager::frame(const Session &session, std::size_t width,
                                              std::size_t rows) {
    const DocumentStamp current = session.stamp();
    if (current.identity != stamp_.identity || current.revision != stamp_.revision)
        throw std::runtime_error("Terminal page belongs to an older document. Reopen the view.");
    if (!ready_ || width != width_ || rows != rows_) {
        TerminalPageFrame prepared = terminal_page(session, cursor_, width, rows);
        frame_ = std::move(prepared);
        width_ = width;
        rows_ = rows;
        ready_ = true;
    }
    return frame_;
}
void TerminalPager::next() {
    if (!ready_ || !frame_.more || frame_.next == cursor_)
        return;
    page_history_.push_back(cursor_);
    if (page_history_.size() > 1024)
        page_history_.pop_front();
    for (const TerminalPageCursor start : frame_.row_starts)
        if (!(start == frame_.next))
            retain(start);
    cursor_ = frame_.next;
    ready_ = false;
}
void TerminalPager::previous() {
    if (history_.empty()) {
        if (cursor_.offset || cursor_.label_cell)
            throw std::runtime_error(
                "Earlier page history is no longer retained. Ctrl+Home returns to the first page.");
        return;
    }
    std::size_t count = std::min(rows_, history_.size());
    if (!page_history_.empty()) {
        const TerminalPageCursor target = page_history_.back();
        count = 0;
        bool found = false;
        for (std::size_t index = history_.size(); index > 0; --index) {
            ++count;
            if (history_[index - 1] == target) {
                found = true;
                break;
            }
        }
        if (!found)
            throw std::runtime_error(
                "Earlier page history is no longer retained. Ctrl+Home returns to the first page.");
        page_history_.pop_back();
    }
    for (std::size_t row = 0; row < count; ++row) {
        cursor_ = history_.back();
        history_.pop_back();
    }
    ready_ = false;
}
void TerminalPager::retain(TerminalPageCursor cursor) {
    history_.push_back(cursor);
    // Bound history by row cursors, independent of window height.
    if (history_.size() > 32768)
        history_.pop_front();
}
void TerminalPager::down() {
    if (!ready_ || frame_.row_starts.size() < 2)
        return;
    const TerminalPageCursor next = frame_.row_starts[1];
    // An EOF boundary is not another visual row to scroll into.
    if (!frame_.more && next == frame_.next)
        return;
    retain(cursor_);
    cursor_ = next;
    ready_ = false;
}
void TerminalPager::up() {
    if (history_.empty()) {
        if (cursor_.offset || cursor_.label_cell)
            throw std::runtime_error(
                "Earlier row history is no longer retained. Ctrl+Home returns to the first page.");
        return;
    }
    cursor_ = history_.back();
    history_.pop_back();
    if (!page_history_.empty() && cursor_ == page_history_.back())
        page_history_.pop_back();
    ready_ = false;
}
void TerminalPager::first() {
    cursor_ = {};
    history_.clear();
    page_history_.clear();
    ready_ = false;
}
} // namespace swiftedit
