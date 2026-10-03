#include "selection_set.hpp"
#include <gui_forms/text.hpp>
#include <stdexcept>

namespace swiftedit {
namespace gf = gui_forms;
namespace {
std::vector<SourceRange> map_selections(const Session &session, const gf::DocumentViewState &view,
                                      const gf::DocumentPageRequest &token,
                                      const std::vector<DisplaySelection> &selections) {
    const DocumentStamp current = session.stamp();
    if (token.revision.document != current.identity.value || token.revision.revision != current.revision.value)
        throw std::runtime_error("Display selections belong to an older or different document.");
    if (session.read_only())
        throw std::runtime_error("Interactive multi-selection metadata requires an editable document.");
    if (selections.empty() || selections.size() > 1000)
        throw std::runtime_error("Select between 1 and 1000 display ranges.");
    std::vector<SourceRange> ranges{};
    ranges.reserve(selections.size());
    for (const DisplaySelection &selection : selections) {
        if (selection.begin.value > selection.end.value)
            throw std::runtime_error("Display selection endpoints must be ordered.");
        const gf::SourceMappingResult begin = view.source_position(token, selection.begin);
        const gf::SourceMappingResult end = view.source_position(token, selection.end);
        if (begin.status != gf::DocumentViewStatus::success || end.status != gf::DocumentViewStatus::success ||
            !begin.position || !end.position)
            throw std::runtime_error("Display selection is stale or splits a character or control label.");
        const std::uint64_t first = (*begin.position).value;
        const std::uint64_t last = (*end.position).value;
        if (first > last || last > session.size())
            throw std::runtime_error("Mapped selection exceeds document.");
        // Editable Session size is below 16 MiB, so checked endpoints fit size_t.
        ranges.push_back({static_cast<std::size_t>(first), static_cast<std::size_t>(last - first)});
    }
    return ranges;
}
}
SelectionSet::SelectionSet(const Session &session, const gf::DocumentViewState &view,
                           const gf::DocumentPageRequest &token,
                           const std::vector<DisplaySelection> &selections)
    : SelectionSet(session, map_selections(session, view, token, selections)) {}
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
    // LF ends all grapheme context, including CRLF and regional-indicator
    // parity. Keep complete LF-delimited context around the selected interval;
    // never cut at a guessed scalar or a fixed look-behind distance.
    std::size_t context_begin = 0;
    if (ranges.front().offset) {
        const std::size_t previous_lf = source.rfind('\n', ranges.front().offset - 1);
        if (previous_lf != std::string::npos)
            context_begin = previous_lf + 1;
    }
    const std::size_t next_lf = source.find('\n', previous_end);
    const std::size_t context_end = next_lf == std::string::npos ? source.size() : next_lf + 1;
    // Metadata placeholders force a grapheme break around each illegal byte.
    // Their byte lengths match source; they are never copied or published.
    std::string metadata = source.substr(context_begin, context_end - context_begin);
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
        const gf::Utf8Offset start(range.offset - context_begin);
        const gf::Utf8Offset end(range.offset + range.length - context_begin);
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
