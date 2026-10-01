#pragma once
#include "session.hpp"
#include <deque>
namespace swiftedit {
struct TerminalPageCursor {
    std::uint64_t offset{};
    std::size_t label_cell{};
    bool after_wrap{};
    bool operator==(const TerminalPageCursor &other) const {
        const bool equal = offset == other.offset && label_cell == other.label_cell &&
                           after_wrap == other.after_wrap;
        return equal;
    }
};
struct TerminalPageRun {
    std::size_t row{}, column{};
    std::uint64_t source_offset{}, source_length{};
    std::string text{};
};
struct TerminalPageFrame {
    std::vector<TerminalPageRun> runs{};
    std::vector<TerminalPageCursor> row_starts{};
    TerminalPageCursor next{};
    bool more{};
};
// Reads at most 64 KiB per frame; never treats an unfinished trailing grapheme
// as complete. Source-independent labels may continue across rows/pages.
[[nodiscard]] TerminalPageFrame terminal_page(const Session &, TerminalPageCursor,
                                              std::size_t width, std::size_t rows);
class TerminalPager {
public:
    void reset(const Session &);
    const TerminalPageFrame &frame(const Session &, std::size_t width, std::size_t rows);
    void next();
    void previous();
    void down();
    void up();
    void first();
    [[nodiscard]] std::uint64_t source_offset() const { return cursor_.offset; }

private:
    void retain(TerminalPageCursor);
    DocumentStamp stamp_{};
    TerminalPageCursor cursor_{};
    std::deque<TerminalPageCursor> history_{};
    std::deque<TerminalPageCursor> page_history_{};
    TerminalPageFrame frame_{};
    std::size_t width_{}, rows_{};
    bool ready_{};
};
} // namespace swiftedit
