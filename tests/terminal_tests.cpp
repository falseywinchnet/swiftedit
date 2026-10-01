#include "terminal_buffer.hpp"
#include "terminal_row.hpp"
#include "terminal_page.hpp"
#include "terminal_search.hpp"
#include "terminal_query.hpp"
#include "date_time.hpp"
#include <iostream>
#include <fstream>
#include "platform.hpp"
#include <stdexcept>
namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        swiftedit::TerminalBuffer terminal{};
        std::tm calendar{};
        calendar.tm_year = 126;
        calendar.tm_mon = 9;
        calendar.tm_mday = 1;
        calendar.tm_hour = 12;
        calendar.tm_min = 34;
        calendar.tm_sec = 56;
        calendar.tm_isdst = -1;
        const std::time_t instant = std::mktime(&calendar);
        check(instant != static_cast<std::time_t>(-1), "Local timestamp fixture is representable");
        const std::string timestamp = notepad::local_date_time(instant);
        check(timestamp == "2026-10-01 12:34:56",
              "Shared local date/time uses plain stable formatting");
        terminal.insert("replace me");
        terminal.select_all();
        terminal.insert(timestamp);
        check(terminal.session().text() == timestamp, "Timestamp insertion replaces the selection");
        check(terminal.undo() && terminal.session().text() == "replace me",
              "Timestamp insertion is one undoable edit");
        terminal.reset(true);
        terminal.move(swiftedit::TerminalMotion::left);
        terminal.move(swiftedit::TerminalMotion::right);
        terminal.erase(true);
        check(terminal.selection().caret == 0 && terminal.session().text().empty(),
              "Empty boundary navigation is inert");
        terminal.insert("e\xcc\x81\r\n\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0"
                        "\x9f\x91\xa7");
        terminal.move(swiftedit::TerminalMotion::left);
        check(terminal.selection().caret == 5, "Family emoji is one movement");
        terminal.move(swiftedit::TerminalMotion::left);
        check(terminal.selection().caret == 3, "CRLF is one movement");
        terminal.erase(true);
        check(terminal.session().text().starts_with("\r\n"),
              "Backspace removes complete combining grapheme");
        check(terminal.undo(), "Terminal edit uses Session undo");
        terminal.move(swiftedit::TerminalMotion::document_start);
        terminal.move(swiftedit::TerminalMotion::right, true);
        check(terminal.selected_text() == "e\xcc\x81", "Selection preserves complete grapheme");
        terminal.insert("Q");
        check(terminal.session().text().starts_with("Q\r\n"),
              "Selection replacement is one source edit");
        check(terminal.undo() && terminal.session().text().starts_with("e\xcc\x81"),
              "Replacement undo restores source");
        terminal.reset(true);
        terminal.insert("abcd\nx\nabcdef");
        terminal.move(swiftedit::TerminalMotion::document_start);
        terminal.move(swiftedit::TerminalMotion::end);
        terminal.move(swiftedit::TerminalMotion::down);
        check(terminal.selection().caret == 6, "Vertical movement clamps to short line");
        terminal.move(swiftedit::TerminalMotion::down);
        check(terminal.selection().caret == 11,
              "Vertical movement preserves desired grapheme column");
        terminal.move(swiftedit::TerminalMotion::up, true, 2);
        check(terminal.selected_text() == "\nx\nabcd",
              "Page-style selection traverses logical rows");
        terminal.move(swiftedit::TerminalMotion::left);
        check(terminal.selection().caret == 4 && terminal.selected_range().length == 0,
              "Left collapses selection to start");
        const swiftedit::SourceClipboard cut = terminal.cut();
        check(cut.bytes() == "abcd\n" && terminal.session().text() == "x\nabcdef",
              "Cut without selection removes one full logical line");
        terminal.paste(cut);
        check(terminal.session().text() == "abcd\nx\nabcdef", "Internal cut text pastes exactly");
        terminal.reset(true);
        const std::string malformed("\xff\xcc\x81Z", 4);
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() /
            ("swiftedit-terminal-" + std::to_string(test_process_id()));
        check(std::filesystem::create_directory(dir), "Unique terminal fixture");
        struct Cleanup {
            std::filesystem::path path{};
            ~Cleanup() {
                std::error_code error{};
                std::filesystem::remove_all(path, error);
            }
        } cleanup{dir};
        const std::filesystem::path path = dir / "malformed.txt";
        {
            std::ofstream file(path, std::ios::binary);
            file.write(malformed.data(), static_cast<std::streamsize>(malformed.size()));
            check(static_cast<bool>(file), "Malformed fixture written");
        }
        terminal.open(path);
        terminal.move(swiftedit::TerminalMotion::document_start);
        terminal.erase(false);
        check(
            terminal.session().text() == std::string("\xcc\x81Z", 3),
            "Illegal byte is independently deletable without swallowing following combining text");
        check(terminal.undo() && terminal.session().text() == malformed,
              "Illegal bytes survive undo");
        terminal.select_all();
        check(terminal.selected_text() == malformed, "Copy returns exact source bytes");
        const swiftedit::SourceClipboard raw_copy = terminal.copy();
        const swiftedit::SourceClipboard raw_cut = terminal.cut();
        check(terminal.session().text().empty(), "Cut captures malformed source before deletion");
        terminal.paste(raw_cut);
        check(terminal.session().text() == malformed, "Malformed cut/paste preserves exact bytes");
        const bool paste_undone = terminal.undo();
        check(paste_undone && terminal.session().text().empty(), "Paste is one undo step");
        terminal.paste(raw_copy);
        bool external_refused = false;
        try {
            terminal.insert(malformed);
        } catch (const std::exception &) {
            external_refused = true;
        }
        check(external_refused && terminal.session().text() == malformed,
              "External malformed payload remains refused");
        terminal.move(swiftedit::TerminalMotion::document_end);
        terminal.insert("changed");
        const std::string dirty = terminal.session().text();
        const swiftedit::DocumentStamp before_copy = terminal.session().stamp();
        const swiftedit::TerminalSelection copy_selection = terminal.selection();
        const std::filesystem::path text_copy = dir / "malformed.1.txt";
        terminal.save_text_copy(text_copy);
        check(notepad::read_file(text_copy).bytes == std::string(" \xcc\x81Zchanged"),
              "Text Copy replaces each malformed byte while preserving Unicode");
        check(terminal.session().text() == dirty && terminal.session().dirty() &&
                  terminal.session().path() == path &&
                  terminal.session().stamp().identity == before_copy.identity &&
                  terminal.session().stamp().revision == before_copy.revision &&
                  terminal.selection().anchor == copy_selection.anchor &&
                  terminal.selection().caret == copy_selection.caret,
              "Text Copy does not alter the open document, dirty state or selection");
        bool copy_overwrite_refused = false;
        try {
            terminal.save_text_copy(text_copy);
        } catch (const std::exception &) {
            copy_overwrite_refused = true;
        }
        check(copy_overwrite_refused && notepad::read_file(path).bytes == malformed,
              "Text Copy refuses an existing destination and leaves the source file intact");
        check(terminal.undo() && terminal.session().text() == malformed,
              "Text Copy does not establish a new undo boundary");
        check(terminal.redo() && terminal.session().text() == dirty,
              "Text Copy preserves redo history");
        bool discard_refused = false;
        try {
            terminal.reset();
        } catch (const std::exception &) {
            discard_refused = true;
        }
        check(discard_refused && terminal.session().text() == dirty,
              "New document requires explicit dirty discard");
        terminal.reset(true);
        terminal.insert("\xf0\x9f\x98\x80");
        terminal.select_all();
        terminal.insert("ab");
        check(terminal.undo() && terminal.selection().caret == 0,
              "Undo clamps a caret inside a restored multibyte grapheme to its start");
        terminal.move(swiftedit::TerminalMotion::document_end);
        terminal.enter();
        check(terminal.session().text().ends_with(notepad::native_newline()),
              "New terminal documents use native Enter endings");
        const std::filesystem::path saved = dir / "saved.txt";
        terminal.save_as(saved);
        check(!terminal.session().dirty() && !terminal.undo(),
              "Terminal save establishes shared undo boundary");
        check(swiftedit::terminal_glyph("\xe4\xb8\xad", 0).cells == 2,
              "CJK uses two terminal cells");
        check(swiftedit::terminal_glyph("e\xcc\x81", 0).cells == 1,
              "Combining mark does not add a cell");
        check(swiftedit::terminal_glyph("\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9", 0).cells ==
                  2,
              "Joined emoji occupies two cells");
        check(swiftedit::terminal_glyph("\t", 3).text == " ", "Tabs use four-cell stops");
        const swiftedit::TerminalGlyph escape = swiftedit::terminal_glyph("\x1b", 0);
        check(escape.label && escape.text == "[U+001B]", "Escape control is visible inert text");
        check(swiftedit::terminal_glyph("\xe2\x80\xae", 0).text == "[U+202E]",
              "Bidi override is visibly labeled");
        check(swiftedit::terminal_glyph(std::string("\xff", 1), 0).text == "[BYTE FF]",
              "Malformed byte is visibly labeled");
        terminal.reset();
        terminal.insert("A\xe4\xb8\xad\tZ");
        terminal.move(swiftedit::TerminalMotion::document_start);
        terminal.move(swiftedit::TerminalMotion::right);
        terminal.move(swiftedit::TerminalMotion::right, true);
        const swiftedit::TerminalRow row = swiftedit::terminal_row(terminal, 0, 0, 8);
        check(row.total_cells == 5 && row.caret_column == 3 && row.runs.size() == 4 &&
                  row.runs[1].selected && row.runs[1].cells == 2 && row.runs[2].cells == 1,
              "Source selection and caret map to terminal cell columns");
        const swiftedit::TerminalRow clipped = swiftedit::terminal_row(terminal, 0, 2, 2);
        check(clipped.runs[0].text == " " && clipped.runs[0].source.length == 3 &&
                  clipped.clipped_left && clipped.clipped_right && clipped.caret_column == 1,
              "Partial wide glyph never emits a broken character or changes its source range");
        swiftedit::Session paged_source{};
        paged_source.replace_ranges({{0, 0}}, "ABCD\r\nEF\nG", paged_source.stamp());
        swiftedit::TerminalPager pager{};
        pager.reset(paged_source);
        const swiftedit::TerminalPageFrame first_page = pager.frame(paged_source, 4, 1);
        check(first_page.next.offset == 4 && first_page.next.after_wrap,
              "Page carries full-row wrap boundary");
        pager.next();
        const swiftedit::TerminalPageFrame second_page = pager.frame(paged_source, 4, 1);
        check(second_page.runs.size() == 2 && second_page.runs[0].text == "E" &&
                  second_page.next.offset == 9,
              "Newline after full row does not create an extra empty screen row");
        pager.previous();
        check(pager.frame(paged_source, 4, 1).runs[0].text == "A",
              "Previous restores exact page cursor");
        pager.first();
        const swiftedit::TerminalPageFrame row_frame = pager.frame(paged_source, 4, 3);
        check(row_frame.row_starts.size() == 3, "Pager records visual row source starts");
        pager.down();
        check(pager.frame(paged_source, 4, 3).runs[0].text == "E",
              "Down scrolls one row instead of a page, skipping no newline after exact wrap");
        pager.down();
        check(pager.frame(paged_source, 4, 3).runs[0].text == "G",
              "Down can reveal the last row even when the frame already reaches EOF");
        pager.down();
        check(pager.frame(paged_source, 4, 3).runs[0].text == "G",
              "Down at the last row does not scroll into empty space");
        pager.up();
        check(pager.frame(paged_source, 4, 3).runs[0].text == "E",
              "Up restores exactly one prior visual row");
        pager.first();
        static_cast<void>(pager.frame(paged_source, 4, 2));
        pager.next();
        check(pager.frame(paged_source, 4, 2).runs[0].text == "G",
              "Page Down continues to advance by a complete frame");
        pager.up();
        check(pager.frame(paged_source, 4, 2).runs[0].text == "E",
              "Up after Page Down returns one row, not the entire previous page");
        pager.previous();
        check(pager.frame(paged_source, 4, 2).runs[0].text == "A",
              "Page Up after row navigation restores the earlier page start");
        paged_source.replace_ranges({{0, paged_source.text().size()}}, "\x1bZ",
                                    paged_source.stamp());
        bool old_page_refused = false;
        try {
            static_cast<void>(pager.frame(paged_source, 4, 1));
        } catch (const std::exception &) {
            old_page_refused = true;
        }
        check(old_page_refused, "Pager cache rejects changed source");
        pager.reset(paged_source);
        const swiftedit::TerminalPageFrame label_first = pager.frame(paged_source, 4, 1);
        check(label_first.runs[0].text == "[U+0" && label_first.next.offset == 0 &&
                  label_first.next.label_cell == 4,
              "Long inert label can continue without consuming its source byte early");
        pager.next();
        check(pager.frame(paged_source, 4, 1).runs[0].text == "01B]",
              "Label continuation is exact");
        pager.next();
        check(pager.frame(paged_source, 4, 1).runs[0].text == "Z",
              "Following content is not skipped");
        pager.first();
        static_cast<void>(pager.frame(paged_source, 4, 3));
        pager.down();
        check(pager.frame(paged_source, 4, 3).runs[0].text == "01B]",
              "Row scrolling retains the exact inert label continuation");
        pager.up();
        check(pager.frame(paged_source, 4, 3).runs[0].text == "[U+0",
              "Reverse row scrolling restores the first label fragment");
        const std::filesystem::path large_path = dir / "large.txt";
        {
            std::ofstream file(large_path, std::ios::binary);
            file << std::string(swiftedit::maximum_page - 2, 'a') << "e\xcc\x81";
            file.seekp(swiftedit::editable_limit - 1);
            file.put('z');
            check(static_cast<bool>(file), "Large terminal fixture written");
        }
        swiftedit::Session large{};
        large.open(large_path);
        check(large.read_only(), "Actual large terminal fixture uses bounded file handle");
        swiftedit::TerminalPager large_pager{};
        large_pager.reset(large);
        const swiftedit::TerminalPageFrame large_first = large_pager.frame(large, 1000, 300);
        check(large_first.next.offset == swiftedit::maximum_page - 2,
              "Incomplete trailing scalar and its preceding grapheme are deferred together");
        const swiftedit::DocumentStamp large_stamp = large.stamp();
        large_pager.down();
        check(large_pager.frame(large, 1000, 300).row_starts[0].offset == 1000,
              "Actual read-only file scrolls one visual row with bounded reads");
        large_pager.up();
        check(large_pager.frame(large, 1000, 300).row_starts[0].offset == 0 &&
                  large.stamp().identity == large_stamp.identity &&
                  large.stamp().revision == large_stamp.revision && !large.dirty(),
              "Read-only row navigation preserves document identity, revision and cleanliness");
        large_pager.next();
        const swiftedit::TerminalPageFrame &large_second = large_pager.frame(large, 1000, 300);
        check(large_second.runs[0].text == "e\xcc\x81",
              "Next bounded read reconstructs full combining grapheme");
        const std::filesystem::path huge_cluster_path = dir / "huge-cluster.txt";
        {
            std::ofstream file(huge_cluster_path, std::ios::binary);
            file << 'e';
            for (std::size_t i = 0; i < 35000; ++i)
                file << "\xcc\x81";
            file.seekp(swiftedit::editable_limit - 1);
            file.put('z');
        }
        swiftedit::Session huge_cluster{};
        huge_cluster.open(huge_cluster_path);
        bool incomplete_refused = false;
        try {
            static_cast<void>(swiftedit::terminal_page(huge_cluster, {}, 80, 20));
        } catch (const std::exception &) {
            incomplete_refused = true;
        }
        check(incomplete_refused && huge_cluster.read_only(),
              "Oversized unknown grapheme is explicit unavailable, never sliced");
        terminal.reset(true);
        terminal.insert("one axc two abc");
        terminal.move(swiftedit::TerminalMotion::document_start);
        swiftedit::SearchPattern flagged("a?c");
        flagged.toggle(1);
        swiftedit::TerminalSearch search{};
        search.begin(terminal, flagged, true);
        std::size_t steps = 0;
        while (search.state() == swiftedit::TerminalSearchState::pending && steps < 1000) {
            static_cast<void>(search.step(terminal, 1));
            ++steps;
        }
        check(steps > 1 && search.state() == swiftedit::TerminalSearchState::found &&
                  terminal.selected_text() == "axc",
              "Incremental terminal search selects flagged grapheme match");
        search.begin(terminal, swiftedit::SearchPattern("one"));
        for (std::size_t i = 0;
             i < 100 && search.state() == swiftedit::TerminalSearchState::pending; ++i)
            static_cast<void>(search.step(terminal, 4));
        check(search.state() == swiftedit::TerminalSearchState::found &&
                  terminal.selected_text() == "one",
              "Terminal Find wraps at end of document");
        search.begin(terminal, swiftedit::SearchPattern("missing"));
        terminal.move(swiftedit::TerminalMotion::right);
        check(search.step(terminal) == swiftedit::TerminalSearchState::cancelled,
              "Moving selection revokes deferred search publication");
        search.begin(terminal, swiftedit::SearchPattern("abc"));
        terminal.insert("changed");
        check(search.step(terminal) == swiftedit::TerminalSearchState::cancelled,
              "Editing source revokes deferred search publication");
        search.begin(terminal, swiftedit::SearchPattern("missing"));
        search.cancel();
        check(search.step(terminal) == swiftedit::TerminalSearchState::cancelled,
              "Explicit cancellation cannot publish a later match");
        swiftedit::TerminalQuery query{};
        query.insert("a?c");
        query.move(false);
        query.move(false);
        query.toggle();
        check(query.text() == "a?c" && query.pattern().slots()[1].wildcard,
              "Terminal wildcard flag leaves punctuation source intact");
        query.home();
        query.insert("X");
        check(query.pattern().slots()[2].wildcard,
              "Unchanged suffix retains wildcard flag after query insertion");
        query.end();
        query.erase(true);
        check(query.text() == "Xa?" && query.pattern().slots()[2].wildcard,
              "Query grapheme deletion preserves other flags");
        query.toggle();
        check(!query.pattern().slots()[2].wildcard, "Toggle at query end addresses last slot");
        terminal.reset(true);
        terminal.insert("cat cat cat");
        swiftedit::TerminalReplace replacement{};
        replacement.begin(terminal, swiftedit::SearchPattern("cat"), "dog", true);
        check(replacement.step(terminal, 1) == swiftedit::TerminalReplaceState::pending &&
                  terminal.session().text() == "cat cat cat",
              "Partial replacement remains private");
        replacement.cancel();
        check(terminal.session().text() == "cat cat cat", "Cancel preserves source");
        replacement.begin(terminal, swiftedit::SearchPattern("cat"), "dog", true);
        for (std::size_t i = 0;
             i < 100 && replacement.state() == swiftedit::TerminalReplaceState::pending; ++i)
            static_cast<void>(replacement.step(terminal, 2));
        check(replacement.state() == swiftedit::TerminalReplaceState::complete &&
                  replacement.count() == 3 && terminal.session().text() == "dog dog dog",
              "Prepared replacements publish together");
        check(terminal.undo() && terminal.session().text() == "cat cat cat",
              "One undo restores entire Replace All");
        replacement.begin(terminal, swiftedit::SearchPattern("cat"), "dog", true);
        terminal.move(swiftedit::TerminalMotion::document_end);
        terminal.insert("!");
        check(replacement.step(terminal) == swiftedit::TerminalReplaceState::cancelled &&
                  terminal.session().text() == "cat cat cat!",
              "Source edit revokes replacement authority");
        const std::filesystem::path crcr_path = dir / "existing-crcr.txt";
        {
            std::ofstream file(crcr_path, std::ios::binary);
            file << "cat\r\rcat";
        }
        terminal.reset(true);
        terminal.open(crcr_path);
        terminal.select_all();
        const swiftedit::SourceClipboard crcr_copy = terminal.copy();
        terminal.paste(crcr_copy);
        check(terminal.session().text() == "cat\r\rcat",
              "CRCR source clipboard remains byte-faithful");
        swiftedit::Session destination{};
        destination.paste_range({0, 0}, raw_copy, destination.stamp());
        check(destination.text() == malformed, "Clipboard survives source document replacement");
        const swiftedit::DocumentStamp old_destination = destination.stamp();
        destination.paste_range({0, malformed.size()}, crcr_copy, destination.stamp());
        bool stale_paste_refused = false;
        try {
            destination.paste_range({0, 0}, raw_copy, old_destination);
        } catch (const std::exception &) {
            stale_paste_refused = true;
        }
        check(stale_paste_refused && destination.text() == "cat\r\rcat",
              "Stale destination refuses clipboard publication");
        bool range_paste_refused = false;
        try {
            destination.paste_range({destination.text().size(), 1}, raw_copy, destination.stamp());
        } catch (const std::exception &) {
            range_paste_refused = true;
        }
        check(range_paste_refused && destination.text() == "cat\r\rcat",
              "Out-of-bounds clipboard publication preserves destination");
        replacement.begin(terminal, swiftedit::SearchPattern("cat"), "dog", true);
        while (replacement.state() == swiftedit::TerminalReplaceState::pending)
            static_cast<void>(replacement.step(terminal, 4));
        check(replacement.count() == 2 && terminal.session().text() == "dog\r\rdog",
              "Session-issued replacement preserves pre-existing CRCR source bytes");
        check(terminal.undo() && terminal.session().text() == "cat\r\rcat",
              "CRCR replacement remains one undoable edit");
        bool metadata_refused = false;
        try {
            replacement.begin(terminal, swiftedit::SearchPattern("cat"), "\r\rL9\r\r", true);
        } catch (const std::exception &) {
            metadata_refused = true;
        }
        check(metadata_refused && terminal.session().text() == "cat\r\rcat",
              "Caller-inserted metadata stays forbidden despite source-preserving operation");
        swiftedit::Session near_limit{};
        near_limit.replace_ranges({{0, 0}}, std::string(swiftedit::editable_limit - 2, 'x'),
                                  near_limit.stamp());
        const swiftedit::DocumentStamp before_capacity = near_limit.stamp();
        bool capacity_refused = false;
        try {
            near_limit.paste_range({0, 0}, raw_copy, before_capacity);
        } catch (const std::exception &) {
            capacity_refused = true;
        }
        check(capacity_refused && near_limit.revision() == before_capacity.revision &&
                  near_limit.size() == swiftedit::editable_limit - 2,
              "Oversized paste preserves source and revision");
        const bool original_insert_undone = near_limit.undo();
        check(original_insert_undone && near_limit.text().empty(),
              "Refused paste does not add an undo step");
        bool stale_copy_refused = false;
        try {
            static_cast<void>(near_limit.copy_range({0, 0}, before_capacity));
        } catch (const std::exception &) {
            stale_copy_refused = true;
        }
        check(stale_copy_refused, "Clipboard capture requires the observed source revision");
        swiftedit::Session issued{};
        issued.replace_ranges({{0, 0}}, "cat", issued.stamp());
        std::unique_ptr<swiftedit::SessionReplacement> plan =
            issued.prepare_replacement(swiftedit::SearchPattern("cat"), "dog");
        bool early_refused = false;
        try {
            static_cast<void>(issued.commit_replacement(*plan));
        } catch (const std::exception &) {
            early_refused = true;
        }
        check(early_refused && issued.text() == "cat",
              "Incomplete plan grants no commit authority");
        while (!(*plan).step(1)) {
        }
        swiftedit::Session other{};
        bool foreign_plan_refused = false;
        try {
            static_cast<void>(other.commit_replacement(*plan));
        } catch (const std::exception &) {
            foreign_plan_refused = true;
        }
        check(foreign_plan_refused && other.text().empty(),
              "Plan is bound to issuing Session identity");
        check(issued.commit_replacement(*plan) == 1 && issued.text() == "dog",
              "Issuing Session can commit complete plan");
        bool reuse_refused = false;
        try {
            static_cast<void>(issued.commit_replacement(*plan));
        } catch (const std::exception &) {
            reuse_refused = true;
        }
        check(reuse_refused, "Plan cannot publish twice");
        std::unique_ptr<swiftedit::SessionReplacement> same =
            issued.prepare_replacement(swiftedit::SearchPattern("dog"), "dog");
        while (!(*same).step(16)) {
        }
        const swiftedit::DocumentStamp before_same = issued.stamp();
        check(issued.commit_replacement(*same) == 1 &&
                  issued.stamp().revision == before_same.revision,
              "Identical result preserves revision and undo boundary");
        bool same_reuse_refused = false;
        try {
            static_cast<void>(issued.commit_replacement(*same));
        } catch (const std::exception &) {
            same_reuse_refused = true;
        }
        check(same_reuse_refused, "One-use consent holds even when revision did not change");
        std::unique_ptr<swiftedit::SessionReplacement> stale_plan =
            issued.prepare_replacement(swiftedit::SearchPattern("dog"), "cat");
        while (!(*stale_plan).step(16)) {
        }
        issued.replace_ranges({{0, 0}}, "new ", issued.stamp());
        bool stale_plan_refused = false;
        try {
            static_cast<void>(issued.commit_replacement(*stale_plan));
        } catch (const std::exception &) {
            stale_plan_refused = true;
        }
        check(stale_plan_refused && issued.text() == "new dog",
              "Completed plan cannot overwrite a later revision");
        std::cout << "Terminal navigation and shared edit tests passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
