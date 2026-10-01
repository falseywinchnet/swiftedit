#pragma once
#include "session.hpp"

namespace swiftedit {
// Interactive selection policy for editable sessions. Owns ordered ranges and
// a document stamp, never a borrowed Session or generated display labels.
// Character lengths are source graphemes; malformed bytes each count as one.
// Copy retains separate parts, leaving clipboard joining policy to the view.
class SelectionSet final {
public:
    SelectionSet(const Session &, const std::vector<SourceRange> &);
    [[nodiscard]] bool can_rewrite() const { return equal_lengths_; }
    [[nodiscard]] const std::vector<SourceRange> &ranges() const { return ranges_; }
    [[nodiscard]] std::vector<SourceClipboard> copy(const Session &) const;
    void rewrite(Session &, const std::string_view replacement) const;

private:
    void require_current(const Session &) const;
    DocumentStamp stamp_{};
    std::vector<SourceRange> ranges_{};
    bool equal_lengths_{true};
};
} // namespace swiftedit
