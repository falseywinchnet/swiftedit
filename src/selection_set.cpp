#include "selection_set.hpp"
#include <gui_forms/text.hpp>
#include <stdexcept>

namespace swiftedit {
namespace gf = gui_forms;
SelectionSet::SelectionSet(const Session &session, const std::vector<SourceRange> &ranges)
    : stamp_(session.stamp()) {
    if (session.read_only())
        throw std::runtime_error("Interactive multi-selection metadata requires an editable document.");
    if (ranges.empty() || ranges.size() > 1000)
        throw std::runtime_error("Select between 1 and 1000 source ranges.");
    const std::string &source = session.text();
    std::size_t previous_offset = 0, previous_end = 0, previous_length = 0;
    for (std::size_t index = 0; index < ranges.size(); ++index) {
        const SourceRange &range = ranges[index];
        if (range.offset > source.size() || range.length > source.size() - range.offset)
            throw std::runtime_error("Selection range exceeds document.");
        if (index && (range.offset < previous_end || range.offset == previous_offset ||
                      (range.offset == previous_end && (!range.length || !previous_length))))
            throw std::runtime_error("Selections must be ordered and disjoint.");
        previous_offset = range.offset;
        previous_end = range.offset + range.length;
        previous_length = range.length;
    }
    // Metadata placeholders force a grapheme break around each illegal byte.
    // Their byte lengths match source; they are never copied or published.
    std::string metadata = source;
    for (std::size_t offset = 0; offset < metadata.size();) {
        const std::size_t length = utf8_sequence_length(metadata, offset);
        if (length)
            offset += length;
        else {
            metadata[offset] = '\x01';
            ++offset;
        }
    }
    const gf::TextStore text(metadata);
    std::size_t first_count = 0;
    for (std::size_t index = 0; index < ranges.size(); ++index) {
        const SourceRange &range = ranges[index];
        const gf::Utf8Offset start(range.offset);
        const gf::Utf8Offset end(range.offset + range.length);
        if (!text.is_grapheme_boundary(start) || !text.is_grapheme_boundary(end))
            throw std::runtime_error("Selection splits a source grapheme or line ending.");
        const std::size_t count = text.grapheme_index(end).value() - text.grapheme_index(start).value();
        if (index == 0)
            first_count = count;
        else if (count != first_count)
            equal_lengths_ = false;
    }
    ranges_ = ranges;
}
void SelectionSet::require_current(const Session &session) const {
    const DocumentStamp current = session.stamp();
    if (current.identity != stamp_.identity || current.revision != stamp_.revision)
        throw std::runtime_error("Selections belong to an older or different document.");
}
std::vector<SourceClipboard> SelectionSet::copy(const Session &session) const {
    require_current(session);
    std::vector<SourceClipboard> result{};
    result.reserve(ranges_.size());
    for (const SourceRange &range : ranges_)
        result.push_back(session.copy_range(range, stamp_));
    return result;
}
void SelectionSet::rewrite(Session &session, const std::string_view replacement) const {
    require_current(session);
    if (!equal_lengths_)
        throw std::runtime_error("Unequal-length selections are copy-only.");
    session.replace_ranges(ranges_, replacement, stamp_);
}
} // namespace swiftedit
