#pragma once
#include "session.hpp"
#include <deque>
#include <optional>
#include <memory>
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
// Normally reads 8 KiB per step. Incomplete first graphemes grow context up to
// 64 KiB (at most 120 KiB total reads for the default adaptive step). Retains the final
// viewport and at most 32768 preceding row cursors for backward navigation.
// The caller owns cancellation by dropping this task.
class TerminalPageEnd final {
public:
    TerminalPageEnd(const Session &, std::size_t width, std::size_t rows,
                    std::size_t source_budget = 8192);
    // Reconstruct the rows immediately before an existing visual row cursor.
    TerminalPageEnd(const Session &, std::size_t width, std::size_t rows,
                    TerminalPageCursor before);
    [[nodiscard]] bool step(const Session &);
    [[nodiscard]] TerminalPageCursor result(const Session &) const;
private:
    friend class TerminalPager;
    void validate(const Session &) const;
    DocumentStamp stamp_{};
    std::uint64_t size_{};
    std::size_t width_{}, rows_{};
    std::size_t source_budget_{}, column_{};
    TerminalPageCursor cursor_{};
    std::optional<TerminalPageCursor> before_{};
    std::deque<TerminalPageCursor> tail_{};
    bool complete_{};
};
class TerminalPager {
public:
    void reset(const Session &);
    const TerminalPageFrame &frame(const Session &, std::size_t width, std::size_t rows);
    void next();
    void previous();
    void down();
    void up();
    void first();
    // Returns work only when retained history cannot satisfy Up/Page Up.
    // The caller must retain the viewport until successful finish_end publication.
    [[nodiscard]] std::unique_ptr<TerminalPageEnd> prepare_previous(const Session &, bool page) const;
    void finish_end(const Session &, const TerminalPageEnd &);
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
