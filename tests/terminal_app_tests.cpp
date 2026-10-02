#include "terminal_app.hpp"
#include "platform.hpp"
#include <fstream>
#include <iostream>

class ScriptConsole final : public swiftedit::TerminalConsole {
public:
    std::vector<swiftedit::TerminalInput> inputs{};
    mutable std::size_t cursor{}, writes{};
    mutable bool end_seen{}, copy_seen{};
    bool wait_for_end{}, wait_for_copy{}, wait_for_previous{};
    mutable bool previous_pending{}, previous_seen{}, search_pending{}, single_copy_seen{};
    bool wait_for_search{};
    mutable std::size_t found_count{};
    mutable bool three_copy_seen{}, two_copy_seen{}, caret_completed{};
    mutable bool line_pending{}, line_completed{};
    bool wait_for_line{}, wait_for_text_copy{}, wait_for_publication{};
    mutable bool publication_seen{};
    mutable bool text_copy_pending{}, text_copy_saved{};
    bool started{};
    std::size_t inspect_after{};
    std::string expected_cursor{};
    mutable bool cursor_checked{};
    std::string expected_copy{};
    mutable bool expected_copy_seen{};
    bool cancel_repeats{};
    mutable bool repeats_cancelled{};
    void start() override { started = true; }
    swiftedit::TerminalSize size() const override { return {80, 24}; }
    void write(std::string_view text) const override {
        if (!expected_copy.empty() && text.find(expected_copy) != std::string_view::npos)
            expected_copy_seen = true;
        if (inspect_after && cursor == inspect_after) {
            if (text.find(expected_cursor + "\x1b[?25h") == std::string_view::npos)
                throw std::runtime_error("Read-only vertical navigation displayed the wrong caret.");
            cursor_checked = true;
        }
        if (text.empty() || !started)
            throw std::runtime_error("Unexpected empty or premature terminal frame.");
        ++writes;
        if (text.find("Publishing text copy...") != std::string_view::npos)
            publication_seen = true;
        if (text.find("Saving text copy...") != std::string_view::npos)
            text_copy_pending = true;
        if (text.find("Text copy saved; open document unchanged") != std::string_view::npos) {
            text_copy_pending = false;
            text_copy_saved = true;
        }
        if (text.find("Finding line boundary...") != std::string_view::npos)
            line_pending = true;
        if (text.find("Read-only line boundary reached") != std::string_view::npos) {
            line_pending = false;
            line_completed = true;
        }
        if (text.find("Finding previous grapheme...") != std::string_view::npos)
            previous_pending = true;
        if (text.find("Read-only caret moved") != std::string_view::npos && previous_pending) {
            caret_completed = true;
            previous_pending = false;
        }
        if (text.find("Copied 3 bytes") != std::string_view::npos)
            three_copy_seen = true;
        if (text.find("Copied 2 bytes") != std::string_view::npos)
            two_copy_seen = true;
        if (text.find("Searching...") != std::string_view::npos)
            search_pending = true;
        if (text.find("Found") != std::string_view::npos) {
            ++found_count;
            search_pending = false;
        }
        if (text.find("Copied 1 bytes") != std::string_view::npos)
            single_copy_seen = true;
        if (text.find("Finding earlier page...") != std::string_view::npos)
            previous_pending = true;
        if (text.find("Earlier read-only page") != std::string_view::npos) {
            previous_seen = true;
            previous_pending = false;
        }
        if (text.find("End of read-only document") != std::string_view::npos)
            end_seen = true;
        if (text.find("Copied 16777216 bytes") != std::string_view::npos)
            copy_seen = true;
    }
    bool input_ready() const override {
        const bool ready = cursor < inputs.size() && (!wait_for_end || end_seen) &&
                           (!wait_for_publication || publication_seen) && (!wait_for_text_copy || !text_copy_pending) && (!wait_for_line || !line_pending) && (!wait_for_copy || copy_seen) && (!wait_for_previous || !previous_pending) &&
                           (!wait_for_search || !search_pending);
        return ready;
    }
    swiftedit::TerminalInput read() const override {
        if (cursor == inputs.size())
            throw std::runtime_error("Terminal requested unexpected input.");
        const swiftedit::TerminalInput result = inputs[cursor];
        ++cursor;
        return result;
    }
    bool cancel_requested() override {
        if (cancel_repeats && cursor == 1 && !repeats_cancelled) {
            repeats_cancelled = true;
            return true;
        }
        return false;
    }
    void allow_wrap_cancel(bool) override {}
    void press(std::uint32_t key, char16_t text = 0, bool control = false, bool shift = false) {
        swiftedit::TerminalInput input{};
        input.key = key;
        input.text_unit = text;
        input.control = control;
        input.shift = shift;
        input.pressed = true;
        inputs.push_back(input);
    }
};
int main() {
    try {
        const std::filesystem::path directory = std::filesystem::temp_directory_path() /
            ("swiftedit-terminal-app-" + std::to_string(test_process_id()));
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Terminal fixture directory already exists.");
        struct Cleanup {
            std::filesystem::path directory{};
            ~Cleanup() {
                std::error_code ignored{};
                std::filesystem::remove_all(directory, ignored);
            }
        } cleanup{directory};
        const std::filesystem::path path = directory / swiftedit::terminal_path("caf\xc3\xa9.txt");
        {
            std::ofstream output(path, std::ios::binary);
            output << "base\r\n";
        }
        ScriptConsole console{};
        console.inputs.push_back({}); // Non-key event must not redraw or mutate.
        swiftedit::TerminalInput resized{};
        resized.resized = true;
        console.inputs.push_back(resized);
        console.press('A', u'A');
        console.press(0, 0xd83d);
        console.press(0, 0xde00);
        console.press('Z', 0, true); // Undo supplementary scalar as one insertion.
        console.press('Y', 0, true);
        console.press(swiftedit::terminal_key::left);
        console.press(swiftedit::terminal_key::erase);
        console.press('S', 0, true);
        console.press('X', 0, true);
        swiftedit::Terminal terminal(console);
        const int result = terminal.run(path);
        if (result != 0 || console.cursor != console.inputs.size() || console.writes == 0 ||
            notepad::read_file(path).bytes != "Abase\r\n")
            throw std::runtime_error("Shared terminal Unicode/navigation/save behavior failed.");
        ScriptConsole invalid{};
        invalid.press(0, 0xdc00); // A lone low surrogate must not become source text.
        invalid.press('Q', u'Q');
        invalid.press('S', 0, true);
        invalid.press('X', 0, true);
        swiftedit::Terminal recovered(invalid);
        if (recovered.run(path) != 0 || notepad::read_file(path).bytes != "QAbase\r\n")
            throw std::runtime_error("Invalid input recovery changed source incorrectly.");
        ScriptConsole pasted{};
        swiftedit::TerminalInput paste{};
        paste.pasted = true;
        paste.paste = "\x13\x18line\r\n";
        pasted.inputs.push_back(paste);
        pasted.press('Z', 0, true);
        pasted.press('Y', 0, true);
        pasted.press('S', 0, true);
        pasted.press('X', 0, true);
        swiftedit::Terminal paste_terminal(pasted);
        if (paste_terminal.run(path) != 0 || pasted.cursor != pasted.inputs.size() ||
            notepad::read_file(path).bytes != paste.paste + "QAbase\r\n")
            throw std::runtime_error("Paste executed commands or failed atomic undo/redo.");
        const std::string original = notepad::read_file(path).bytes;
        ScriptConsole cancelled{};
        swiftedit::TerminalInput large{};
        large.pasted = true;
        large.paste.assign(500001, 'x');
        cancelled.inputs.push_back(large);
        swiftedit::TerminalInput false_confirmation{};
        false_confirmation.pasted = true;
        false_confirmation.paste = "Y\r\n";
        cancelled.inputs.push_back(false_confirmation);
        cancelled.press('Y', 0, true); // Ctrl+Y must not approve a paste.
        cancelled.press('S', 0, true); // Other commands must not bypass the choice.
        cancelled.press(swiftedit::terminal_key::escape);
        cancelled.press('X', 0, true);
        swiftedit::Terminal cancel_terminal(cancelled);
        if (cancel_terminal.run(path) != 0 || cancelled.cursor != cancelled.inputs.size() ||
            notepad::read_file(path).bytes != original)
            throw std::runtime_error("Large paste cancellation or explicit confirmation failed.");
        ScriptConsole undone{};
        undone.inputs.push_back(large);
        undone.press('Y', u'y');
        undone.press('Z', 0, true);
        undone.press('X', 0, true); // Must exit cleanly without an unsaved-changes choice.
        swiftedit::Terminal undone_terminal(undone);
        if (undone_terminal.run(path) != 0 || undone.cursor != undone.inputs.size() ||
            notepad::read_file(path).bytes != original)
            throw std::runtime_error("One undo did not remove the entire confirmed paste.");
        ScriptConsole approved{};
        approved.inputs.push_back(large);
        approved.press('Y', u'y');
        approved.press('Z', 0, true);
        approved.press('Y', 0, true);
        approved.press('S', 0, true);
        approved.press('X', 0, true);
        swiftedit::Terminal approved_terminal(approved);
        if (approved_terminal.run(path) != 0 || approved.cursor != approved.inputs.size() ||
            notepad::read_file(path).bytes != large.paste + original)
            throw std::runtime_error("Confirmed large paste did not remain one undoable edit.");
        ScriptConsole private_clipboard{};
        private_clipboard.press('A', 0, true);
        private_clipboard.press('C', 0, true);
        private_clipboard.press(swiftedit::terminal_key::home, 0, true);
        private_clipboard.press('U', 0, true);
        private_clipboard.press('N', u'n');
        private_clipboard.press('S', 0, true);
        private_clipboard.press('X', 0, true);
        swiftedit::Terminal private_terminal(private_clipboard);
        if (private_terminal.run(path) != 0 || private_clipboard.cursor != private_clipboard.inputs.size() ||
            notepad::read_file(path).bytes != large.paste + original)
            throw std::runtime_error("Private clipboard bypassed large paste confirmation.");
        ScriptConsole threshold{};
        large.paste.resize(500000);
        threshold.inputs.push_back(large);
        threshold.press('S', 0, true);
        threshold.press('X', 0, true);
        swiftedit::Terminal threshold_terminal(threshold);
        if (threshold_terminal.run(path) != 0 || threshold.cursor != threshold.inputs.size() ||
            notepad::read_file(path).bytes != large.paste + std::string(500001, 'x') + original)
            throw std::runtime_error("Exactly 500000 paste bytes incorrectly required confirmation.");
        const std::filesystem::path large_path = directory / "large-read-only.txt";
        {
            std::ofstream output(large_path, std::ios::binary);
            output.seekp(swiftedit::editable_limit - 1);
            output.put('z');
        }
        const std::filesystem::path saved_copy = swiftedit::versioned_name(large_path);
        const std::filesystem::path vertical_path = directory / "vertical-read-only.txt";
        {
            std::ofstream output(vertical_path, std::ios::binary);
            output << "abcdef\nx\nabcdef\n";
            for (std::size_t row = 0; row < 30; ++row)
                output << "abcdef\n";
            output.seekp(swiftedit::editable_limit - 1);
            output.put('z');
        }
        ScriptConsole repeated_rows{};
        repeated_rows.press(swiftedit::terminal_key::down, 0, false, true);
        repeated_rows.inputs.back().repeats = 23;
        repeated_rows.expected_copy = "Copied 156 bytes";
        repeated_rows.press('C', 0, true);
        repeated_rows.press('X', 0, true);
        swiftedit::Terminal repeated_terminal(repeated_rows);
        if (repeated_terminal.run(vertical_path) != 0 || !repeated_rows.expected_copy_seen)
            throw std::runtime_error("Navigation slicing lost repetitions or changed the selection anchor.");
        ScriptConsole cancelled_repeats{};
        cancelled_repeats.cancel_repeats = true;
        cancelled_repeats.press(swiftedit::terminal_key::down);
        cancelled_repeats.inputs.back().repeats = 1000;
        cancelled_repeats.inspect_after = 1;
        cancelled_repeats.expected_cursor = "\x1b[18;1H";
        cancelled_repeats.press('X', 0, true);
        swiftedit::Terminal cancelled_repeat_terminal(cancelled_repeats);
        if (cancelled_repeat_terminal.run(vertical_path) != 0 || !cancelled_repeats.repeats_cancelled || !cancelled_repeats.cursor_checked)
            throw std::runtime_error("Escape did not cancel repeated navigation after a bounded slice.");
        for (const std::size_t moves : {std::size_t{2}, std::size_t{23}}) {
            ScriptConsole vertical{};
            for (std::size_t column = 0; column < 3; ++column)
                vertical.press(swiftedit::terminal_key::right);
            for (std::size_t row = 0; row < moves; ++row)
                vertical.press(swiftedit::terminal_key::down);
            vertical.inspect_after = 3 + moves;
            vertical.expected_cursor = moves == 2 ? "\x1b[4;4H" : "\x1b[21;4H";
            vertical.press('X', 0, true);
            swiftedit::Terminal vertical_terminal(vertical);
            if (vertical_terminal.run(vertical_path) != 0 || !vertical.cursor_checked)
                throw std::runtime_error("Read-only desired column was not restored after short lines or scrolling.");
        }
        ScriptConsole vertical_return{};
        for (std::size_t column = 0; column < 3; ++column)
            vertical_return.press(swiftedit::terminal_key::right);
        for (std::size_t row = 0; row < 23; ++row)
            vertical_return.press(swiftedit::terminal_key::down);
        for (std::size_t row = 0; row < 23; ++row)
            vertical_return.press(swiftedit::terminal_key::up, 0, false, true);
        vertical_return.inspect_after = 49;
        vertical_return.expected_cursor = "\x1b[2;4H";
        vertical_return.expected_copy = "Copied 156 bytes";
        vertical_return.press('C', 0, true);
        vertical_return.press('X', 0, true);
        swiftedit::Terminal return_terminal(vertical_return);
        if (return_terminal.run(vertical_path) != 0 || !vertical_return.cursor_checked || !vertical_return.expected_copy_seen)
            throw std::runtime_error("Upward scrolling lost the desired column or selection caret.");
        ScriptConsole page_return{};
        for (std::size_t column = 0; column < 3; ++column)
            page_return.press(swiftedit::terminal_key::right);
        page_return.press(swiftedit::terminal_key::down);
        page_return.press(swiftedit::terminal_key::down);
        page_return.press(swiftedit::terminal_key::page_down);
        page_return.press(swiftedit::terminal_key::page_up, 0, false, true);
        page_return.inspect_after = 7;
        page_return.expected_cursor = "\x1b[4;4H";
        page_return.expected_copy = "Copied 140 bytes";
        page_return.press('C', 0, true);
        page_return.press('X', 0, true);
        swiftedit::Terminal page_return_terminal(page_return);
        if (page_return_terminal.run(vertical_path) != 0 || !page_return.cursor_checked || !page_return.expected_copy_seen)
            throw std::runtime_error("Page navigation lost its screen row, desired column or selection anchor.");
        ScriptConsole copy_navigation{};
        copy_navigation.wait_for_text_copy = true;
        copy_navigation.press('T', 0, true);
        copy_navigation.press(swiftedit::terminal_key::enter);
        copy_navigation.press('X', 0, true);
        swiftedit::Terminal copy_terminal(copy_navigation);
        if (copy_terminal.run(large_path) != 0 || !copy_navigation.text_copy_saved)
            throw std::runtime_error("Read-only Save Text Copy did not complete cooperatively.");
        {
            swiftedit::PagedFile copied(saved_copy);
            if (copied.size() != swiftedit::editable_limit || copied.page(copied.size() - 1, 1).bytes != "z")
                throw std::runtime_error("Read-only text copy lost source bytes.");
        }
        if (!std::filesystem::remove(saved_copy))
            throw std::runtime_error("Could not remove the completed owned copy fixture.");
        const std::filesystem::path cancelled_copy_path = swiftedit::versioned_name(large_path);
        ScriptConsole cancelled_copy{};
        cancelled_copy.press('T', 0, true);
        cancelled_copy.press(swiftedit::terminal_key::enter);
        cancelled_copy.press(swiftedit::terminal_key::escape);
        cancelled_copy.press('X', 0, true);
        swiftedit::Terminal cancelled_copy_terminal(cancelled_copy);
        if (cancelled_copy_terminal.run(large_path) != 0 || cancelled_copy.text_copy_saved ||
            std::filesystem::exists(cancelled_copy_path))
            throw std::runtime_error("Read-only text copy cancellation published output.");
        ScriptConsole exit_during_copy{};
        exit_during_copy.wait_for_publication = true;
        exit_during_copy.press('T', 0, true);
        exit_during_copy.press(swiftedit::terminal_key::enter);
        exit_during_copy.press('X', 0, true);
        swiftedit::Terminal exit_copy_terminal(exit_during_copy);
        if (exit_copy_terminal.run(large_path) != 0 || !exit_during_copy.publication_seen ||
            !std::filesystem::exists(cancelled_copy_path) ||
            exit_during_copy.cursor != exit_during_copy.inputs.size())
            throw std::runtime_error("Exit during publication did not retain the worker to completion.");
        ScriptConsole end_navigation{};
        end_navigation.wait_for_end = true;
        end_navigation.wait_for_copy = true;
        end_navigation.press(swiftedit::terminal_key::end, 0, true, true);
        end_navigation.press('C', 0, true);
        end_navigation.press('X', 0, true);
        swiftedit::Terminal end_terminal(end_navigation);
        if (end_terminal.run(large_path) != 0 || !end_navigation.end_seen || !end_navigation.copy_seen ||
            end_navigation.cursor != end_navigation.inputs.size())
            throw std::runtime_error("Read-only Ctrl+End failed to finish cooperatively.");
        ScriptConsole rewind_navigation{};
        rewind_navigation.wait_for_end = true;
        rewind_navigation.wait_for_previous = true;
        rewind_navigation.press(swiftedit::terminal_key::end, 0, true);
        for (std::size_t batch = 0; batch < 32; ++batch) {
            rewind_navigation.press(swiftedit::terminal_key::up);
            rewind_navigation.inputs.back().repeats = 1000;
        }
        rewind_navigation.press(swiftedit::terminal_key::up);
        rewind_navigation.inputs.back().repeats = 768;
        rewind_navigation.press(swiftedit::terminal_key::page_up, 0, false, true);
        rewind_navigation.press('X', 0, true);
        swiftedit::Terminal rewind_terminal(rewind_navigation);
        if (rewind_terminal.run(large_path) != 0 || !rewind_navigation.previous_seen ||
            rewind_navigation.cursor != rewind_navigation.inputs.size())
            throw std::runtime_error("Read-only Page Up did not rebuild exhausted history cooperatively.");
        ScriptConsole repeated_rewind{};
        repeated_rewind.wait_for_end = true;
        repeated_rewind.wait_for_previous = true;
        repeated_rewind.inputs = rewind_navigation.inputs;
        // Cross history exhaustion within the last repeated Up event. Its
        // reconstruction replenishes history, so the following Page Up need
        // not reconstruct again; observe the Up completion instead.
        repeated_rewind.inputs[repeated_rewind.inputs.size() - 3].repeats = 900;
        swiftedit::Terminal repeated_rewind_terminal(repeated_rewind);
        if (repeated_rewind_terminal.run(large_path) != 0 || !repeated_rewind.caret_completed ||
            repeated_rewind.cursor != repeated_rewind.inputs.size())
            throw std::runtime_error("Repeated Up cancelled its own pending reconstruction.");
        ScriptConsole paged_find{};
        paged_find.wait_for_search = true;
        paged_find.press('W', 0, true);
        paged_find.press('Z', u'z');
        paged_find.press(swiftedit::terminal_key::enter);
        paged_find.press(swiftedit::terminal_key::f3);
        paged_find.press('C', 0, true);
        paged_find.press('X', 0, true);
        swiftedit::Terminal find_terminal(paged_find);
        if (find_terminal.run(large_path) != 0 || paged_find.found_count != 2 || !paged_find.single_copy_seen)
            throw std::runtime_error("Paged Find/F3 did not select, wrap and copy the exact final match.");
        ScriptConsole cancelled_find{};
        cancelled_find.press('W', 0, true);
        cancelled_find.press('Z', u'z');
        cancelled_find.press(swiftedit::terminal_key::enter);
        cancelled_find.press(swiftedit::terminal_key::escape);
        cancelled_find.press('X', 0, true);
        swiftedit::Terminal cancelled_find_terminal(cancelled_find);
        if (cancelled_find_terminal.run(large_path) != 0 || cancelled_find.found_count)
            throw std::runtime_error("Paged Find did not cancel before publishing a match.");
        const std::filesystem::path horizontal_path = directory / "horizontal-read-only.txt";
        {
            std::ofstream output(horizontal_path, std::ios::binary);
            output << "e\xcc\x81\r\n\x1bZ";
            output.seekp(swiftedit::editable_limit - 1);
            output.put('z');
        }
        ScriptConsole horizontal{};
        horizontal.press(swiftedit::terminal_key::right, 0, false, true);
        horizontal.press('C', 0, true);
        horizontal.press(swiftedit::terminal_key::right);
        horizontal.press(swiftedit::terminal_key::right, 0, false, true);
        horizontal.press('C', 0, true);
        horizontal.press(swiftedit::terminal_key::right);
        horizontal.press(swiftedit::terminal_key::right, 0, false, true);
        horizontal.press('C', 0, true);
        horizontal.press('X', 0, true);
        swiftedit::Terminal horizontal_terminal(horizontal);
        if (horizontal_terminal.run(horizontal_path) != 0 || !horizontal.three_copy_seen ||
            !horizontal.two_copy_seen || !horizontal.single_copy_seen)
            throw std::runtime_error("Shift+Right did not select atomic Unicode, CRLF and control source bytes.");
        ScriptConsole line_selection{};
        line_selection.wait_for_line = true;
        line_selection.press(swiftedit::terminal_key::end, 0, false, true);
        line_selection.press('C', 0, true);
        line_selection.press(swiftedit::terminal_key::home);
        line_selection.press(swiftedit::terminal_key::right, 0, false, true);
        line_selection.press('C', 0, true);
        line_selection.press('X', 0, true);
        swiftedit::Terminal line_terminal(line_selection);
        if (line_terminal.run(horizontal_path) != 0 || !line_selection.line_completed ||
            !line_selection.three_copy_seen)
            throw std::runtime_error("Read-only Home/End failed to select the logical line.");
        ScriptConsole cancelled_line{};
        cancelled_line.press('A', 0, true);
        cancelled_line.press(swiftedit::terminal_key::right);
        cancelled_line.press(swiftedit::terminal_key::home);
        cancelled_line.press(swiftedit::terminal_key::escape);
        cancelled_line.press('X', 0, true);
        swiftedit::Terminal cancelled_line_terminal(cancelled_line);
        if (cancelled_line_terminal.run(horizontal_path) != 0 || cancelled_line.line_completed)
            throw std::runtime_error("Read-only Home did not cancel before publishing its result.");
        ScriptConsole distant_left{};
        distant_left.wait_for_previous = true;
        distant_left.press('A', 0, true);
        distant_left.press(swiftedit::terminal_key::right);
        distant_left.press(swiftedit::terminal_key::left, 0, false, true);
        distant_left.press('C', 0, true);
        distant_left.press('X', 0, true);
        swiftedit::Terminal left_terminal(distant_left);
        if (left_terminal.run(horizontal_path) != 0 || !distant_left.caret_completed ||
            !distant_left.single_copy_seen)
            throw std::runtime_error("Shift+Left from an off-screen EOF did not reconstruct and copy the final grapheme.");
        ScriptConsole cancelled_left{};
        cancelled_left.press('A', 0, true);
        cancelled_left.press(swiftedit::terminal_key::right);
        cancelled_left.press(swiftedit::terminal_key::left, 0, false, true);
        cancelled_left.press(swiftedit::terminal_key::escape);
        cancelled_left.press('X', 0, true);
        swiftedit::Terminal cancelled_left_terminal(cancelled_left);
        if (cancelled_left_terminal.run(horizontal_path) != 0 || cancelled_left.caret_completed)
            throw std::runtime_error("Previous-grapheme scan did not cancel before publication.");
        ScriptConsole cancelled_end{};
        cancelled_end.press(swiftedit::terminal_key::end, 0, true);
        cancelled_end.press(swiftedit::terminal_key::escape);
        cancelled_end.press('X', 0, true);
        swiftedit::Terminal cancelled_end_terminal(cancelled_end);
        if (cancelled_end_terminal.run(large_path) != 0 || cancelled_end.end_seen ||
            cancelled_end.cursor != cancelled_end.inputs.size())
            throw std::runtime_error("Read-only Ctrl+End did not cancel on the next key.");
        std::cout << "Shared terminal loop: Unicode, undo/redo, navigation, save, recovery and large paste passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
