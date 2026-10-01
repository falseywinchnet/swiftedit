#include "display.hpp"
#include "search.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        const std::string source =
            std::string("A\0", 2) + "\xff\r\n" + "\xe2\x80\xae" + "e\xcc\x81";
        const swiftedit::DisplayPage page(source);
        check(page.text() == "A[U+0000][BYTE FF]\r\n[U+202E]e\xcc\x81",
              "Controls and invalid bytes have inert visible labels");
        for (const swiftedit::DisplayUnit &unit : page.units()) {
            const swiftedit::SourceRange mapped = page.source_range(unit.display);
            check(mapped.offset == unit.source.offset && mapped.length == unit.source.length,
                  "Every display unit maps exactly to source");
            const std::size_t offset = page.display_offset(unit.source.offset);
            check(offset == unit.display.offset, "Source mapping round trip");
        }
        bool partial_refused = false;
        try {
            static_cast<void>(page.source_range({2, 1}));
        } catch (const std::runtime_error &) {
            partial_refused = true;
        }
        check(partial_refused, "Partial control label cannot corrupt source");
        const swiftedit::DisplayPage empty("");
        const swiftedit::SourceRange end = empty.source_range({0, 0});
        check(end.offset == 0 && end.length == 0, "Empty insertion maps correctly");
        const swiftedit::DisplayPage literal("[BYTE FF]");
        check(literal.units().size() == 9, "Literal label text remains ordinary source text");
        std::string maximum_source(swiftedit::maximum_page, 'a');
        for (std::size_t index = 1; index < maximum_source.size(); index += 2)
            maximum_source[index] = static_cast<char>(0xff);
        const swiftedit::DisplayPage maximum(maximum_source);
        for (const swiftedit::DisplayUnit &unit : maximum.units()) {
            const swiftedit::SourceRange mapped = maximum.source_range(unit.display);
            check(mapped.offset == unit.source.offset && mapped.length == unit.source.length,
                  "Maximum-page expanded-label forward mapping");
            const std::size_t inverse = maximum.display_offset(unit.source.offset);
            check(inverse == unit.display.offset, "Maximum-page inverse mapping");
        }
        const std::size_t maximum_end = maximum.display_offset(maximum_source.size());
        const swiftedit::SourceRange maximum_eof = maximum.source_range({maximum_end, 0});
        check(maximum_eof.offset == maximum_source.size() && maximum_eof.length == 0,
              "Expanded maximum-page EOF maps exactly");
        for (const swiftedit::DisplayUnit &unit : page.units()) {
            for (std::size_t interior = 1; interior < unit.source.length; ++interior) {
                bool refused = false;
                try {
                    static_cast<void>(page.display_offset(unit.source.offset + interior));
                } catch (const std::runtime_error &) {
                    refused = true;
                }
                check(refused, "Inverse mapping rejects scalar and CRLF interiors");
            }
        }

        swiftedit::Session session{};
        session.replace_ranges({{0, 0}}, "abc abc", session.revision());
        const swiftedit::DocumentRevision initial = session.revision();
        session.replace_ranges({{0, 3}, {4, 3}}, "X", initial);
        check(session.text() == "X X", "Parallel replacement uses original source ranges");
        const bool undone = session.undo();
        check(undone && session.text() == "abc abc", "Parallel replacement is one undo");
        bool stale_refused = false;
        try {
            session.replace_ranges({{0, 1}}, "Y", initial);
        } catch (const std::runtime_error &) {
            stale_refused = true;
        }
        check(stale_refused && session.text() == "abc abc", "Stale range edit refused atomically");
        bool overlap_refused = false;
        try {
            session.replace_ranges({{0, 3}, {2, 3}}, "Y", session.revision());
        } catch (const std::runtime_error &) {
            overlap_refused = true;
        }
        check(overlap_refused && session.text() == "abc abc",
              "Overlapping ranges refused atomically");
        session.replace_ranges({{0, 3}, {4, 3}}, "", session.revision());
        check(session.text() == " ", "Discontiguous deletion preserves intervening source");
        const gui_forms::TextStore searchable("a?c a*c a\xc3\xa9"
                                              "c ae\xcc\x81"
                                              "c aZZc");
        swiftedit::SearchPattern pattern("a?c");
        const swiftedit::SearchProgress literal_hit =
            swiftedit::search_slice(searchable, pattern, 0, 1, true);
        check(literal_hit.match && (*literal_hit.match).length == 3,
              "Question mark is literal until explicitly flagged");
        pattern.toggle(1);
        const swiftedit::SearchProgress unicode_hit =
            swiftedit::search_slice(searchable, pattern, 8, 1, true);
        check(unicode_hit.match && (*unicode_hit.match).length == 4,
              "Wildcard consumes one Unicode grapheme");
        const swiftedit::SearchProgress combining_hit =
            swiftedit::search_slice(searchable, pattern, 12, 1, true);
        check(combining_hit.match && (*combining_hit.match).length == 5,
              "Combining sequence is one wildcard character");
        const swiftedit::SearchProgress slice =
            swiftedit::search_slice(searchable, pattern, 16, 1, true);
        check(!slice.match && slice.next_grapheme == 17 && !slice.complete,
              "Search slice yields without unbounded scanning");
        std::cout << "Display mapping and atomic source-range tests passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
