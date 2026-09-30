#include "editor.hpp"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace gf=gui_forms;
void require(bool good,const char* message){if(!good)throw std::runtime_error(message);}
gf::Control::Ptr find_control(const gf::Control::Ptr& root,std::string_view id) {
    if(root->stable_id().value()==id)return root;
    for(auto& child:root->children())if(auto found=find_control(child,id))return found;
    return {};
}
int main() {
    try {
        const auto path=std::filesystem::temp_directory_path()/("notepad-native-"+std::to_string(GetCurrentProcessId())+".txt");
        require(!std::filesystem::exists(path),"Unique native fixture");
        struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
        {std::ofstream out(path,std::ios::binary);out<<"native\r\nfixture";}
        auto editor=gf::make_control<notepad::Editor>(gf::StableId("native.editor"));
        auto windows=editor->application_windows(path);
        auto* main=windows.front().model.get();
        auto* find_window=windows[3].model.get();
        gf::ApplicationWindowHandle main_handle;
        std::vector<gf::ApplicationWindowHandle> handles(5);
        std::vector<unsigned> close_count(5);
        std::size_t ready_count{};bool passed{};
        std::unique_ptr<gf::Timer> timer;
        gf::SubscriptionToken tick;
        int stage{};
        std::exception_ptr test_failure;
        auto normal_close=windows[0].options.closing;
        windows[0].options.closing=[&](auto& request){if(test_failure)request.cancel=false;else normal_close(request);};
        for(std::size_t i=0;i<windows.size();++i) {
            auto closing=windows[i].options.closing;
            windows[i].options.closing=[&,i,closing](auto& request){std::cout<<"closing "<<i<<std::endl;if(closing)closing(request);++close_count[i];};
            windows[i].options.initially_visible=(i==0);
            auto previous=windows[i].options.ready;
            windows[i].options.ready=[&,i,previous](auto& w,auto handle){
                std::cout<<"ready "<<i<<std::endl;
                previous(w,handle);handles[i]=handle;++ready_count;
                if(i==0)main_handle=handle;
                if(ready_count==5) {
                  timer=std::make_unique<gf::Timer>(*main,std::chrono::milliseconds(60));
                  tick=timer->tick().subscribe([&]{
                   std::cout<<"stage "<<stage<<std::endl;
                   try {
                   switch(stage++) {
                    case 0: {
                    require(editor->text_control()->text()=="native\r\nfixture","Native initial file");
                    editor->text_control()->select_all();
                    editor->text_control()->replace_selection("native saved\r\n");
                    editor->execute("save");
                    require(notepad::read_file(path).bytes=="native saved\r\n","Native save");
                    // Public lifecycle only: no global input, cursor or desktop automation.
                    editor->execute("find");require(handles[3].active(),"Owned find window active");
                    auto query=std::dynamic_pointer_cast<gf::TextBox>(find_control(find_window->root(),"find.query"));
                    auto replacement=std::dynamic_pointer_cast<gf::TextBox>(find_control(find_window->root(),"find.replacement"));
                    require(query && replacement,"Find dialog public controls");
                    query->set_text("native");replacement->set_text("replaced");
                    std::dynamic_pointer_cast<gf::Button>(find_control(find_window->root(),"find.next"))->perform_click();
                    require(editor->text_control()->selected_text()=="native","Literal Find selects matching text");
                    std::dynamic_pointer_cast<gf::Button>(find_control(find_window->root(),"find.all"))->perform_click();
                    require(editor->text_control()->text()=="replaced saved\r\n","Replace All changes literal text");
                    editor->execute("undo");
                    require(editor->text_control()->text()=="native saved\r\n","Replace All single undo");
                    static_cast<void>(handles[3].request_close());
                    break;
                    }
                    case 1:
                    editor->execute("font");require(handles[4].active(),"Owned font window active");
                    static_cast<void>(handles[4].request_close());
                    break;
                    case 2:
                    editor->execute("open");require(!editor->enabled(),"Picker suppresses owner controls");
                    static_cast<void>(handles[1].request_close());
                    break;
                    case 3:
                    if(close_count[1]<1){--stage;return;}
                    std::cout<<"open picker closed; owner enabled="<<editor->enabled()<<std::endl;
                    require(editor->enabled(),"Picker cancellation restores owner controls");
                    std::cout<<"show save as"<<std::endl;
                    editor->execute("save-as");require(!editor->enabled(),"Save As suppresses owner controls");
                    static_cast<void>(handles[2].request_close());
                    break;
                    case 4:
                    if(close_count[2]<1){--stage;return;}
                    require(editor->enabled(),"Save As cancellation restores owner controls");
                    editor->execute("open");require(!editor->enabled(),"Picker reusable presentation");
                    static_cast<void>(handles[1].request_close());
                    break;
                    case 5:
                    if(close_count[1]<2){--stage;return;}
                    require(editor->enabled(),"Reopened picker cancellation");
                    passed=true;
                    timer->stop();
                    static_cast<void>(main_handle.request_close());
                    break;
                   }
                   } catch(const std::exception& e) {
                    std::cerr<<"Native stage failed: "<<e.what()<<std::endl;
                    test_failure=std::current_exception();timer->stop();static_cast<void>(main_handle.request_close());
                   }
                  });
                  timer->start();
                }
            };
        }
        const auto result=gf::Application::run(std::move(windows));
        if(result.callback_exception)std::rethrow_exception(result.callback_exception);
        if(test_failure)std::rethrow_exception(test_failure);
        if(!result.accepted() || !passed) std::cerr<<"Native result="<<int(result.error)<<" ready="<<ready_count<<" stage="<<stage<<" exit="<<result.native_exit_code<<'\n';
        require(result.accepted() && passed,"Native lifecycle smoke completion");
        std::cout<<"Native Windows smoke passed: document save, literal Find/Replace/undo, owned Find/Font, Open/Save As cancel/reopen, owner restoration and clean shutdown.\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
