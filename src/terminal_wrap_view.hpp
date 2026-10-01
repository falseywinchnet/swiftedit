#pragma once
#include "terminal_wrap.hpp"

namespace swiftedit {
// Editable wrapped viewport. Retains only visible rows (at most 296), never a
// whole-document array of visual rows. Initial/reverse row location currently
// scans its logical line synchronously; cooperative indexing remains separate.
class TerminalWrapView {
public:
    const std::vector<TerminalWrappedRow> &frame(TerminalBuffer &, std::size_t width,
                                                 std::size_t height);
    void move(TerminalBuffer &, TerminalMotion, bool extend, std::size_t count, std::size_t width);

private:
    struct Position {
        std::size_t line{}, offset{};
    };
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
};
} // namespace swiftedit
