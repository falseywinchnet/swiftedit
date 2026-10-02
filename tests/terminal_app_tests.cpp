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
        std::cout << "Shared terminal loop: Unicode, undo/redo, navigation, save and recovery passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
