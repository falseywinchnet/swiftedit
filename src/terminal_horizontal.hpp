#pragma once
#include "terminal_logical_page.hpp"
#include "terminal_page.hpp"

namespace swiftedit {
struct TerminalHorizontalCaret {
    std::uint64_t source_offset{}, column{};
};
struct TerminalHorizontalFrame {
    std::vector<TerminalPageRun> runs{};
    // Absolute logical-line columns at legal source grapheme boundaries.
    // Edge guards may lie outside the visible horizontal interval.
    std::vector<TerminalHorizontalCaret> carets{};
    std::optional<std::uint64_t> total_cells{};
    bool clipped_left{}, clipped_right{};
};
// Renders one row from a completed, current logical page. Reads at most 8 KiB
// per step and retains at most 64 KiB of source context for a complete grapheme.
// Dropping the owner cancels work. Failure never publishes a partial frame.
class TerminalHorizontalLine final {
public:
    TerminalHorizontalLine(const Session &, const TerminalLogicalPage &, std::size_t row,
                           std::uint64_t left, std::size_t width);
    [[nodiscard]] bool step(const Session &, std::size_t budget = 8192);
    // Borrows the completed frame until task destruction; revalidate source
    // identity before a later viewport publication.
    [[nodiscard]] const TerminalHorizontalFrame &result(const Session &) const;
    [[nodiscard]] std::uint64_t read_offset() const { return read_offset_; }

private:
    void validate(const Session &) const;
    void retain_caret(std::uint64_t offset, std::uint64_t column);
    void prepare();
    DocumentStamp stamp_{};
    std::uint64_t size_{}, offset_{}, read_offset_{}, end_{}, column_{}, left_{}, right_{};
    std::string source_{};
    TerminalHorizontalFrame frame_{};
    bool complete_{}, failed_{};
};
} // namespace swiftedit
