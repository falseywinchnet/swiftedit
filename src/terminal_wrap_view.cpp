#include "terminal_wrap_view.hpp"
#include <stdexcept>

namespace swiftedit {
bool TerminalWrapView::same(Position left, Position right) {
    const bool result = left.line == right.line && left.offset == right.offset;
    return result;
}
void TerminalWrapView::synchronize(TerminalBuffer &buffer, std::size_t width) {
    if (!width || width > 1000)
        throw std::runtime_error("Wrapped viewport width must be 1..1000 cells.");
    const DocumentStamp stamp = buffer.session().stamp();
    const std::size_t caret = buffer.selection().caret;
    if (!current_ || width_ != width || stamp_.identity != stamp.identity ||
        stamp_.revision != stamp.revision) {
        current_ = false;
        entries_.clear();
        next_eviction_ = 0;
        width_ = width;
        stamp_ = stamp;
        top_ = locate(buffer);
        desired_column_.reset();
        current_ = true;
    } else if (caret != expected_caret_) {
        desired_column_.reset();
    }
    expected_caret_ = caret;
}
TerminalWrapView::Position TerminalWrapView::locate(TerminalBuffer &buffer) {
    const std::size_t line = buffer.line_index();
    const std::size_t caret = buffer.selection().caret;
    const Entry &entry = prepare(buffer, line);
    Position result{line, checkpoint(entry, caret, false)};
    for (;;) {
        const TerminalWrapSpan span = terminal_wrap_span(buffer, line, result.offset, width_);
        const std::size_t end = span.source.offset + span.source.length;
        if (caret < end || span.logical_end)
            return result;
        result.offset = end;
    }
}
TerminalWrapView::Entry &TerminalWrapView::prepare(TerminalBuffer &buffer, std::size_t line) {
    for (Entry &entry : entries_) {
        if (entry.line == line)
            return entry;
    }
    const SourceRange source = buffer.line_range(line);
    Entry prepared{};
    prepared.line = line;
    prepared.starts.reserve(source.length / 4096 + 1);
    prepared.starts.push_back(source.offset);
    std::size_t offset = source.offset;
    for (;;) {
        if (offset - prepared.starts.back() >= 4096)
            prepared.starts.push_back(offset);
        const TerminalWrapSpan span = terminal_wrap_span(buffer, line, offset, width_);
        if (span.logical_end) {
            prepared.last = offset;
            break;
        }
        offset += span.source.length;
    }
    if (entries_.size() < 320) {
        entries_.push_back(std::move(prepared));
        return entries_.back();
    }
    const std::size_t replacement = next_eviction_;
    entries_[replacement] = std::move(prepared);
    next_eviction_ = (next_eviction_ + 1) % 320;
    return entries_[replacement];
}
std::size_t TerminalWrapView::checkpoint(const Entry &entry, std::size_t offset,
                                         bool strictly_before) {
    std::size_t first = 0, last = entry.starts.size();
    while (first < last) {
        const std::size_t middle = first + (last - first) / 2;
        const std::size_t candidate = entry.starts[middle];
        if (candidate < offset || (!strictly_before && candidate == offset))
            first = middle + 1;
        else
            last = middle;
    }
    const std::size_t index = first ? first - 1 : 0;
    return entry.starts[index];
}
TerminalWrapView::Position TerminalWrapView::next(TerminalBuffer &buffer, Position position) {
    const TerminalWrapSpan span =
        terminal_wrap_span(buffer, position.line, position.offset, width_);
    if (!span.logical_end)
        position.offset += span.source.length;
    else if (position.line + 1 < buffer.line_count()) {
        ++position.line;
        position.offset = buffer.line_range(position.line).offset;
    }
    return position;
}
TerminalWrapView::Position TerminalWrapView::previous(TerminalBuffer &buffer, Position position) {
    const SourceRange line = buffer.line_range(position.line);
    if (position.offset == line.offset) {
        if (!position.line)
            return position;
        --position.line;
        const Entry &prior = prepare(buffer, position.line);
        const Position result{position.line, prior.last};
        return result;
    }
    const Entry &entry = prepare(buffer, position.line);
    Position result{position.line, checkpoint(entry, position.offset, true)};
    for (;;) {
        const Position following = next(buffer, result);
        if (following.offset >= position.offset)
            return result;
        result = following;
    }
}
const std::vector<TerminalWrappedRow> &
TerminalWrapView::frame(TerminalBuffer &buffer, std::size_t width, std::size_t height) {
    if (!height || height > 296)
        throw std::runtime_error("Wrapped viewport height must be 1..296 rows.");
    synchronize(buffer, width);
    const Position caret = locate(buffer);
    bool visible = false;
    Position probe = top_;
    for (std::size_t index = 0; index < height; ++index) {
        if (same(probe, caret)) {
            visible = true;
            break;
        }
        const Position following = next(buffer, probe);
        if (same(probe, following))
            break;
        probe = following;
    }
    if (!visible) {
        top_ = caret;
        // Reveal a caret below the viewport on its bottom row; one above it
        // becomes the top row. Source ordering also orders visual rows.
        if (caret.offset > probe.offset) {
            for (std::size_t index = 1; index < height; ++index)
                top_ = previous(buffer, top_);
        }
    }
    rows_.clear();
    rows_.reserve(height);
    Position position = top_;
    for (std::size_t index = 0; index < height; ++index) {
        rows_.push_back(terminal_wrapped_row(buffer, position.line, position.offset, width_));
        const Position following = next(buffer, position);
        if (same(following, position))
            break;
        position = following;
    }
    return rows_;
}
void TerminalWrapView::move(TerminalBuffer &buffer, TerminalMotion motion, bool extend,
                            std::size_t count, std::size_t width) {
    if (!count || count > 1000)
        throw std::runtime_error("Wrapped navigation count must be 1..1000 rows.");
    synchronize(buffer, width);
    Position position = locate(buffer);
    const bool vertical = motion == TerminalMotion::up || motion == TerminalMotion::down;
    if (!vertical && motion != TerminalMotion::home && motion != TerminalMotion::end) {
        desired_column_.reset();
        buffer.move(motion, extend, count);
        expected_caret_ = buffer.selection().caret;
        return;
    }
    std::size_t column = 0;
    if (vertical) {
        if (!desired_column_) {
            const TerminalWrappedRow row =
                terminal_wrapped_row(buffer, position.line, position.offset, width_);
            if (!row.display.caret_column)
                throw std::runtime_error("Wrapped row does not own its source caret.");
            desired_column_ = *row.display.caret_column;
        }
        column = *desired_column_;
        for (std::size_t index = 0; index < count; ++index) {
            const Position target =
                motion == TerminalMotion::up ? previous(buffer, position) : next(buffer, position);
            if (same(target, position))
                break;
            position = target;
        }
    } else {
        desired_column_.reset();
        column = motion == TerminalMotion::end ? width_ : 0;
    }
    const std::size_t target =
        terminal_wrap_source(buffer, position.line, position.offset, width_, column);
    buffer.move_to(target, stamp_, extend);
    expected_caret_ = target;
}
} // namespace swiftedit
