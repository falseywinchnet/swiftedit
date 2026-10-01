#pragma once
#include "terminal_wrap.hpp"

namespace swiftedit {
// Editable wrapped viewport. Retains only visible rows (at most 296), never a
// whole-document array of visual rows. Sparse source checkpoints cover up to
// 320 logical lines, spaced at least 4096 source bytes apart. First preparation
// scans a logical line synchronously; cooperative indexing remains separate.
class TerminalWrapView {
public:
    const std::vector<TerminalWrappedRow> &frame(TerminalBuffer &, std::size_t width,
                                                 std::size_t height);
    void move(TerminalBuffer &, TerminalMotion, bool extend, std::size_t count, std::size_t width);

private:
    struct Position {
        std::size_t line{}, offset{};
    };
    struct Entry {
        std::size_t line{}, last{};
        std::vector<std::size_t> starts{};
    };
    Entry &prepare(TerminalBuffer &, std::size_t line);
    static std::size_t checkpoint(const Entry &, std::size_t offset, bool strictly_before);
    void synchronize(TerminalBuffer &, std::size_t width);
    Position locate(TerminalBuffer &);
    Position next(TerminalBuffer &, Position);
    Position previous(TerminalBuffer &, Position);
    static bool same(Position, Position);
    DocumentStamp stamp_{};
    std::size_t width_{}, expected_caret_{};
    Position top_{};
    bool current_{};
    std::optional<std::size_t> desired_column_{};
    std::vector<TerminalWrappedRow> rows_{};
    std::vector<Entry> entries_{};
    std::size_t next_eviction_{};
};
} // namespace swiftedit
