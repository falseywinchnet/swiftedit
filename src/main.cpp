#include "editor.hpp"
#include <iostream>
#include <windows.h>
#include <shellapi.h>

namespace {
// CommandLineToArgvW owns one LocalAlloc block, including every string.
struct NativeArguments {
    int count{};
    LPWSTR *values{};
    NativeArguments() : values(CommandLineToArgvW(GetCommandLineW(), &count)) {
        if (!values)
            throw std::runtime_error("Cannot read command line.");
    }
    ~NativeArguments() {
        if (values)
            LocalFree(values);
    }
    NativeArguments(const NativeArguments &) = delete;
    NativeArguments &operator=(const NativeArguments &) = delete;
};
} // namespace

int main() {
    try {
        const NativeArguments arguments{};
        std::filesystem::path initial{};
        if (arguments.count > 1)
            initial = std::filesystem::absolute(arguments.values[1]);
        std::shared_ptr<notepad::Editor> editor =
            gui_forms::make_control<notepad::Editor>(gui_forms::StableId("notepad.editor"));
        std::vector<gui_forms::ApplicationWindow> windows = (*editor).application_windows(initial);
        const gui_forms::ApplicationResult result = gui_forms::Application::run(std::move(windows));
        if (result.callback_exception)
            std::rethrow_exception(result.callback_exception);
        const int exit_code = result.accepted() ? 0 : 1;
        return exit_code;
    } catch (const std::exception &e) {
        std::cerr << "SwiftEdit: " << e.what() << '\n';
        return 1;
    }
}
