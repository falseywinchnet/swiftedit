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
    mutable bool previous_pending{}, previous_seen{};
    bool started{};
    void start() override { started = true; }
    swiftedit::TerminalSize size() const override { return {80, 24}; }
    void write(std::string_view text) const override {
        if (text.empty() || !started)
            throw std::runtime_error("Unexpected empty or premature terminal frame.");
        ++writes;
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
                           (!wait_for_copy || copy_seen) && (!wait_for_previous || !previous_pending);
        return ready;
    }
    swiftedit::TerminalInput read() const override {
        if (cursor == inputs.size())
            throw std::runtime_error("Terminal requested unexpected input.");
        const swiftedit::TerminalInput result = inputs[cursor];
        ++cursor;
        return result;
    }
    bool cancel_requested() override { return false; }
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
