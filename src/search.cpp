#include "search.hpp"
#include <stdexcept>

namespace swiftedit {
SearchPattern::SearchPattern(std::string_view query) {
    if (query.empty() || query.size() > 4096)
        throw std::runtime_error("Search text must contain 1..4096 UTF-8 bytes.");
    const gui_forms::TextStore parsed(query);
    const std::size_t count = parsed.grapheme_count().value();
    slots_.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const gui_forms::Utf8Range range = parsed.grapheme_range(gui_forms::GraphemeIndex(index));
        const std::size_t length = range.end.value() - range.start.value();
        const std::string_view bytes = query.substr(range.start.value(), length);
        SearchSlot slot{std::string(bytes), false};
        slots_.push_back(std::move(slot));
    }
}
void SearchPattern::toggle(std::size_t slot) {
    if (slot >= slots_.size())
        throw std::runtime_error("Wildcard slot exceeds search pattern.");
    slots_[slot].wildcard = !slots_[slot].wildcard;
}
namespace {
unsigned char fold(unsigned char value) {
    if (value >= 'A' && value <= 'Z') {
        const unsigned char folded = static_cast<unsigned char>(value + ('a' - 'A'));
        return folded;
    }
    return value;
}
bool equal_text(std::string_view left, std::string_view right, bool match_case) {
    if (match_case) {
        const bool equal = left == right;
        return equal;
    }
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const unsigned char a = fold(static_cast<unsigned char>(left[index]));
        const unsigned char b = fold(static_cast<unsigned char>(right[index]));
        if (a != b)
            return false;
    }
    return true;
}
} // namespace
SearchProgress search_slice(const gui_forms::TextStore &source, const SearchPattern &pattern,
                            std::size_t start_grapheme, std::size_t budget, bool match_case) {
    if (!budget || budget > 65536)
        throw std::runtime_error("Search slice budget must be 1..65536 candidates.");
    const std::size_t count = source.grapheme_count().value();
    if (start_grapheme > count)
        throw std::runtime_error("Search cursor exceeds document.");
    const std::vector<SearchSlot> &slots = pattern.slots();
    SearchProgress result{{}, start_grapheme, false};
    if (slots.size() > count || start_grapheme > count - slots.size()) {
        result.complete = true;
        return result;
    }
    const std::size_t last_candidate = count - slots.size();
    const std::string_view text = source.utf8();
    std::size_t checked = 0;
    while (result.next_grapheme <= last_candidate && checked < budget) {
        const std::size_t candidate = result.next_grapheme;
        ++result.next_grapheme;
        ++checked;
        bool matches = true;
        for (std::size_t index = 0; index < slots.size(); ++index) {
            if (slots[index].wildcard)
                continue;
            const gui_forms::Utf8Range range =
                source.grapheme_range(gui_forms::GraphemeIndex(candidate + index));
            const std::size_t length = range.end.value() - range.start.value();
            const std::string_view value = text.substr(range.start.value(), length);
            const bool equal = equal_text(value, slots[index].literal, match_case);
            if (!equal) {
                matches = false;
                break;
            }
        }
        if (matches) {
            const gui_forms::Utf8Offset first =
                source.utf8_offset(gui_forms::GraphemeIndex(candidate));
            const gui_forms::Utf8Offset end =
                source.utf8_offset(gui_forms::GraphemeIndex(candidate + slots.size()));
            result.match = SourceRange{first.value(), end.value() - first.value()};
            return result;
        }
    }
    result.complete = result.next_grapheme > last_candidate;
    return result;
}
} // namespace swiftedit
