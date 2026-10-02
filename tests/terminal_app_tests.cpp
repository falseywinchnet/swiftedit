#include "terminal_app.hpp"
#include "platform.hpp"
#include <fstream>
#include <iostream>

class ScriptConsole final : public swiftedit::TerminalConsole {
public:
    std::vector<swiftedit::TerminalInput> inputs{};
    mutable std::size_t cursor{}, writes{};
    bool started{};
    void start() override { started = true; }
    swiftedit::TerminalSize size() const override { return {80, 24}; }
    void write(std::string_view text) const override {
        if (text.empty() || !started)
            throw std::runtime_error("Unexpected empty or premature terminal frame.");
        ++writes;
    }
    bool input_ready() const override {
        const bool ready = cursor < inputs.size();
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
    void press(std::uint32_t key, char16_t text = 0, bool control = false) {
        swiftedit::TerminalInput input{};
        input.key = key;
        input.text_unit = text;
        input.control = control;
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
        std::cout << "Shared terminal loop: Unicode, undo/redo, navigation, save, recovery and large paste passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
