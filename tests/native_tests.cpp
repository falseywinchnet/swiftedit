#include "editor.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <windows.h>
namespace gf = gui_forms;
void require(bool good, const char *message) {
    if (!good)
        throw std::runtime_error(message);
}
gf::Control::Ptr find_control(const gf::Control::Ptr &root, std::string_view id) {
    if ((*root).stable_id().value() == id)
        return root;
    for (const gf::Control::Ptr &child : (*root).children()) {
        const gf::Control::Ptr found = find_control(child, id);
        if (found)
            return found;
    }
    return {};
}
// Application::run is synchronous. Every callback borrows this stack owner only
// until run returns. The timer subscription is revoked before timer destruction;
// original window hooks retain only the production editor's weak observer.
class NativeSmoke final {
public:
    struct ReadyListener {
        NativeSmoke *owner{};
        std::size_t index{};
        void operator()(gf::Window &window, gf::ApplicationWindowHandle handle) const {
            (*owner).ready(index, window, handle);
        }
    };
    struct ClosingListener {
        NativeSmoke *owner{};
        std::size_t index{};
        void operator()(gf::HostCloseRequest &request) const { (*owner).closing(index, request); }
    };
    struct TickListener {
        NativeSmoke *owner{};
        void operator()() const { (*owner).on_tick(); }
    };
    struct WindowHooks {
        std::function<void(gf::Window &, gf::ApplicationWindowHandle)> ready{};
        std::function<void(gf::HostCloseRequest &)> closing{};
    };
    std::filesystem::path path{};
    std::shared_ptr<notepad::Editor> editor{};
    gf::Window *main{};
    gf::Window *find_window{};
    std::array<gf::ApplicationWindowHandle, 9> handles{};
    std::array<unsigned, 9> close_count{};
    std::array<WindowHooks, 9> hooks{};
    gf::ApplicationWindowHandle main_handle{};
    std::size_t ready_count{};
    bool passed{};
    int stage{};
    std::exception_ptr test_failure{};
    std::unique_ptr<gf::Timer> timer{};
    gf::SubscriptionToken tick{};
    ~NativeSmoke() {
        // Application::run has already shut down its windows. Stop is performed
        // by the completion/failure callback before close, never after teardown.
        tick = {};
        timer.reset();
    }
    void ready(std::size_t index, gf::Window &window, gf::ApplicationWindowHandle handle) {
        hooks[index].ready(window, handle);
        handles[index] = handle;
        ++ready_count;
        if (index == 0)
            main_handle = handle;
        if (ready_count == handles.size()) {
            timer = std::make_unique<gf::Timer>(*main, std::chrono::milliseconds(60));
            tick = (*timer).tick().subscribe(TickListener{this});
            (*timer).start();
        }
    }
    void closing(std::size_t index, gf::HostCloseRequest &request) {
        if (index == 0 && test_failure)
            request.cancel = false;
        else if (hooks[index].closing)
            hooks[index].closing(request);
        ++close_count[index];
    }
    void on_tick() {
        try {
            const int current_stage = stage;
            ++stage;
            switch (current_stage) {
            case 0: {
                require((*(*editor).text_control()).text() == "native\r\nfixture",
                        "Native initial file");
                (*(*editor).text_control()).select_all();
                (*(*editor).text_control()).replace_selection("native saved\r\n");
                (*editor).execute("save");
                const notepad::FileSnapshot observed_1 = notepad::read_file(path);
                require(observed_1.bytes == "native saved\r\n", "Native save");
                // Public lifecycle only: no global input, cursor or desktop automation.
                (*editor).execute("find");
                require(handles[3].active(), "Owned find window active");
                const std::shared_ptr<notepad::QueryField> query =
                    std::dynamic_pointer_cast<notepad::QueryField>(
                        find_control((*find_window).root(), "find.query"));
                const std::shared_ptr<gf::TextBox> replacement =
                    std::dynamic_pointer_cast<gf::TextBox>(
                        find_control((*find_window).root(), "find.replacement"));
                require(query && replacement, "Find dialog public controls");
                (*query).set_text("native");
                (*replacement).set_text("replaced");
                (*std::dynamic_pointer_cast<gf::Button>(
                     find_control((*find_window).root(), "find.next")))
                    .perform_click();
                const std::string selected = (*(*editor).text_control()).selected_text();
                require(selected == "native", "Literal Find selects matching text");
                (*std::dynamic_pointer_cast<gf::Button>(
                     find_control((*find_window).root(), "find.all")))
                    .perform_click();
                require((*(*editor).text_control()).text() == "replaced saved\r\n",
                        "Replace All changes literal text");
                (*editor).execute("undo");
                require((*(*editor).text_control()).text() == "native saved\r\n",
                        "Replace All single undo");
                static_cast<void>(handles[3].request_close());
                break;
            }
            case 1:
                (*editor).execute("font");
                require(handles[4].active(), "Owned font window active");
                static_cast<void>(handles[4].request_close());
                break;
            case 2:
                (*editor).execute("open");
                require(!(*editor).enabled(), "Picker suppresses owner controls");
                static_cast<void>(handles[1].request_close());
                break;
            case 3:
                if (close_count[1] < 1) {
                    --stage;
                    return;
                }
                std::cout << "open picker closed; owner enabled=" << (*editor).enabled()
                          << std::endl;
                require((*editor).enabled(), "Picker cancellation restores owner controls");
                std::cout << "show save as" << std::endl;
                (*editor).execute("save-as");
                require(!(*editor).enabled(), "Save As suppresses owner controls");
                static_cast<void>(handles[2].request_close());
                break;
            case 4:
                if (close_count[2] < 1) {
                    --stage;
                    return;
                }
                require((*editor).enabled(), "Save As cancellation restores owner controls");
                (*editor).execute("open");
                require(!(*editor).enabled(), "Picker reusable presentation");
                static_cast<void>(handles[1].request_close());
                break;
            case 5:
                if (close_count[1] < 2) {
                    --stage;
                    return;
                }
                require((*editor).enabled(), "Reopened picker cancellation");
                (*editor).execute("characters");
                require(handles[5].active(), "Owned Unicode dialog active");
                (*(*editor).character_picker(false)).select_codepoint(0x1f600);
                (*(*editor).character_picker(false)).insert_selected();
                require((*(*editor).text_control()).text().find("\xf0\x9f\x98\x80") !=
                            std::string::npos,
                        "Native Unicode dialog inserts a supplementary scalar");
                (*editor).execute("undo");
                require((*(*editor).text_control()).text() == "native saved\r\n",
                        "Unicode insertion undo");
                static_cast<void>(handles[5].request_close());
                break;
            case 6:
                (*editor).execute("controls");
                require(handles[6].active(), "Owned control dialog active");
                (*(*editor).character_picker(true)).select_codepoint(0x200b);
                (*(*editor).character_picker(true)).insert_selected();
                require((*(*editor).text_control()).text().find("\xe2\x80\x8b") !=
                            std::string::npos,
                        "Native control dialog inserts literal source bytes");
                (*editor).execute("undo");
                require((*(*editor).text_control()).text() == "native saved\r\n",
                        "Control insertion undo");
                static_cast<void>(handles[6].request_close());
                break;
            case 7:
                if (close_count[5] < 1 || close_count[6] < 1) {
                    --stage;
                    return;
                }
                (*(*editor).text_control()).set_text("mixed\nline\r\n");
                (*editor).execute("save");
                require((*editor).save_choice_pending() && handles[7].active(),
                        "Native mixed-ending choice is active before publication");
                (*editor).execute("cancel-save");
                require(!(*editor).save_choice_pending() && (*editor).enabled(),
                        "Native mixed-ending cancellation restores owner");
                (*editor).execute("save");
                (*editor).execute("save-as-is");
                require(notepad::read_file(path).bytes == "mixed\nline\r\n",
                        "Native mixed-ending Save As-Is preserves bytes");
                break;
            case 8:
                (*editor).execute("save");
                require((*editor).save_choice_pending(), "Native normalization choice reopened");
                (*editor).execute("save-normalized");
                require(
                    notepad::read_file(path).bytes == "mixed\r\nline\r\n" &&
                        (*(*editor).text_control()).text() == "mixed\r\nline\r\n",
                    "Native conversion uses the document CRLF default and displays saved bytes");
                break;
            case 9: {
                std::ofstream external(path, std::ios::binary);
                external << "external native change";
                external.close();
                (*editor).execute("save");
                require((*editor).conflict_pending() && handles[8].active(),
                        "Native conflict choice owns an active dialog");
                (*editor).execute("conflict-over");
                (*editor).execute("conflict-review");
                break;
            }
            case 10:
                (*editor).execute("conflict-save");
                require(!(*editor).conflict_pending() && (*editor).enabled() &&
                            notepad::read_file(path).bytes == "mixed\r\nline\r\n",
                        "Native reviewed overwrite publishes and restores owner");
                break;
            case 11:
                passed = true;
                (*timer).stop();
                static_cast<void>(main_handle.request_close());
                break;
            }

        } catch (const std::exception &failure) {
            std::cerr << "Native stage failed: " << failure.what() << std::endl;
            test_failure = std::current_exception();
            (*timer).stop();
            static_cast<void>(main_handle.request_close());
        }
    }
    void run() {
        editor = gf::make_control<notepad::Editor>(gf::StableId("native.editor"));
        std::vector<gf::ApplicationWindow> windows = (*editor).application_windows(path);
        require(windows.size() == handles.size(), "Expected main and eight owned dialogs");
        main = windows.front().model.get();
        find_window = windows[3].model.get();
        for (std::size_t i = 0; i < windows.size(); ++i) {
            hooks[i].ready = std::move(windows[i].options.ready);
            hooks[i].closing = std::move(windows[i].options.closing);
            windows[i].options.ready = ReadyListener{this, i};
            windows[i].options.closing = ClosingListener{this, i};
            windows[i].options.initially_visible = i == 0;
        }
        const gf::ApplicationResult result = gf::Application::run(std::move(windows));
        tick = {};
        timer.reset();
        if (result.callback_exception)
            std::rethrow_exception(result.callback_exception);
        if (test_failure)
            std::rethrow_exception(test_failure);
        require(result.accepted() && passed, "Native lifecycle smoke completion");
    }
};
int main() {
    try {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("notepad-native-" + std::to_string(GetCurrentProcessId()) + ".txt");
        const bool observed_2 = std::filesystem::exists(path);
        require(!observed_2, "Unique native fixture");
        struct Cleanup {
            std::filesystem::path path{};
            ~Cleanup() {
                std::error_code error{};
                std::filesystem::remove(path, error);
            }
        } cleanup{path};
        {
            std::ofstream output(path, std::ios::binary);
            output << "native\r\nfixture";
        }
        NativeSmoke smoke{};
        smoke.path = path;
        smoke.run();
        std::cout << "Native Windows smoke passed: save, Find/Replace/undo, owned dialogs, picker "
                     "cancel/reopen and clean shutdown.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
