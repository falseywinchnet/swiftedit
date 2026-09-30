#include "editor.hpp"
#include <windows.h>
#include <shellapi.h>
#include <iostream>

int main() {
    try {
        int argc{};auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
        if(!argv) throw std::runtime_error("Cannot read command line.");
        std::filesystem::path initial;
        if(argc>1) initial=std::filesystem::absolute(argv[1]);
        LocalFree(argv);
        auto editor=gui_forms::make_control<notepad::Editor>(gui_forms::StableId("notepad.editor"));
        auto result=gui_forms::Application::run(editor->application_windows(initial));
        if(result.callback_exception) std::rethrow_exception(result.callback_exception);
        return result.accepted()?0:1;
    } catch(const std::exception& e){std::cerr<<"SwiftEdit: "<<e.what()<<'\n';return 1;}
}
