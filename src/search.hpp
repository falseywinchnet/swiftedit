#pragma once
#include "session.hpp"
#include <gui_forms/text.hpp>

namespace swiftedit {
struct SearchSlot {
    std::string literal{};
    bool wildcard{};
};
// Wildcards are flags on grapheme slots, never punctuation interpreted as syntax.
class SearchPattern {
public:
    explicit SearchPattern(std::string_view query);
    void toggle(std::size_t slot);
    const std::vector<SearchSlot> &slots() const { return slots_; }

private:
    std::vector<SearchSlot> slots_{};
};
struct SearchProgress {
    std::optional<SourceRange> match{};
    std::size_t next_grapheme{};
    bool complete{};
};
// Bounded synchronous slice. The owner may cancel between slices; the source
// must remain unchanged across slices (retain/check its revision externally).
[[nodiscard]] SearchProgress search_slice(const gui_forms::TextStore &, const SearchPattern &,
                                          std::size_t start_grapheme, std::size_t budget,
                                          bool match_case);
} // namespace swiftedit
