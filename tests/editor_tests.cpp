#include "editor.hpp"
#include <deque>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <windows.h>

namespace gf = gui_forms;
gf::HostCapabilities capabilities() {
    gf::HostCapabilities caps{};
    caps.platform = "notepad-test";
    caps.available = gf::HostCapability::lifecycle | gf::HostCapability::dialogs |
                     gf::HostCapability::clipboard | gf::HostCapability::keyboard_input;
    return caps;
}
class TestServices final : public gf::HostServices {
public:
    TestServices() : HostServices(capabilities()) {}
    std::deque<gf::HostDialogChoice> choices{};
    std::string clipboard{};
    int dialogs{};
    std::string last_message{};

protected:
    gf::HostMonitorResult query_monitors_impl() override { return {}; }
    gf::HostServiceStatus set_cursor_impl(gf::CursorKind) override { return {}; }
    gf::HostServiceStatus set_pointer_capture_impl(bool, std::uint64_t) override { return {}; }
    gf::HostClipboardTextResult read_clipboard_text_impl() override {
        const gf::HostClipboardTextResult result{{}, clipboard, 1, true};
        return result;
    }
    gf::HostServiceStatus write_clipboard_text_impl(std::string_view text) override {
        clipboard = text;
        return {};
    }
    gf::HostServiceStatus play_sound_cue_impl(const gf::HostSoundCueRequest &) override {
        return {};
    }
    gf::HostDialogResult show_dialog_impl(const gf::HostDialogRequest &request) override {
        ++dialogs;
        const gf::HostMessageDialogRequest &message =
            std::get<gf::HostMessageDialogRequest>(request.payload);
        last_message = message.message;
        const gf::HostDialogChoice choice =
            choices.empty() ? gf::HostDialogChoice::ok : choices.front();
        if (!choices.empty())
            choices.pop_front();
        const gf::HostDialogResult result{
            {},
            request.request_id,
            gf::HostMessageDialogResult{gf::HostDialogOutcome::accepted, choice}};
        return result;
    }
};

void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void verify_callback_revocation() {
    std::shared_ptr<notepad::Editor> editor =
        gf::make_control<notepad::Editor>(gf::StableId("lifetime.editor"));
    const std::weak_ptr<notepad::Editor> observer = editor;
    std::vector<gf::ApplicationWindow> windows = (*editor).application_windows({});
    const std::function<void(gf::Window &, gf::ApplicationWindowHandle)> retained_ready =
        windows[0].options.ready;
    const std::function<void(gf::HostCloseRequest &)> retained_closing = windows[0].options.closing;
    {
        gf::Window &main = *windows[0].model;
        TestServices services{};
        gf::HostSession host(main, capabilities(), &services);
        const gf::HostDispatchResult attached =
            host.dispatch({1, 0, gf::HostAttachEvent{{800, 600}, 1}});
        check(attached.accepted(), "Callback test host attached");
        retained_ready(main, {});
        (*(*editor).text_control()).replace_selection("dirty");
        services.choices.push_back(gf::HostDialogChoice::no);
        gf::HostCloseRequest first_close{};
        retained_closing(first_close);
        check(first_close.cancel, "Close continuation initially defers native close");
        check(main.dispatcher_snapshot().pending > 0, "Close continuation was queued");
        (*editor).dispose();
        const gf::DispatchDrainResult drained = main.drain_posted_work();
        check(drained.cancelled > 0 && drained.invoked == 0,
              "Disposal revokes queued close continuation");
        const int dialogs_before = services.dialogs;
        retained_ready(main, {});
        gf::HostCloseRequest disposed_close{};
        retained_closing(disposed_close);
        check(!disposed_close.cancel && services.dialogs == dialogs_before,
              "Retained callbacks ignore disposed live editor");
        // HostSession must be detached before its borrowed window is destroyed.
        host.shutdown();
    }
    windows.clear();
    editor.reset();
    check(observer.expired(), "Retained callbacks do not own editor");
    const std::shared_ptr<notepad::DialogLayout> root =
        gf::make_control<notepad::DialogLayout>(gf::StableId("lifetime.probe"));
    gf::Window probe(root, {200, 100});
    retained_ready(probe, {});
    gf::HostCloseRequest expired_close{};
    retained_closing(expired_close);
    check(!expired_close.cancel, "Retained closing callback ignores destroyed editor");
}
int main() {
    try {
        verify_callback_revocation();
        namespace gf = gui_forms;
        const std::shared_ptr<notepad::Editor> editor =
            gf::make_control<notepad::Editor>(gf::StableId("test.editor"));
        gf::Window window(editor, {800, 600});
        window.perform_layout();
        TestServices services{};
        gf::HostSession host(window, capabilities(), &services);
        const gf::HostDispatchResult attached =
            host.dispatch({1, 0, gf::HostAttachEvent{{800, 600}, 1}});
        check(attached.accepted(), "Headless host attachment");
        const std::shared_ptr<gf::TextBox> text = (*editor).text_control();
        check((*text).multiline() && (*text).accepts_tab(), "Multiline editor setup");
        window.request_focus(text);
        const bool input_routed = window.dispatch_text({"first"});
        check(input_routed, "Text input routed");
        window.dispatch_key({gf::KeyAction::down, gf::PhysicalKey::enter});
        window.dispatch_text({"second"});
        check((*text).text() == "first\r\nsecond", "Configured Enter preserves CRLF");
        (*editor).execute("select-all");
        const std::string selected = (*text).selected_text();
        check(selected == "first\r\nsecond", "Select all");
        const gf::TextSelection count_selection = (*text).selection();
        const bool count_dirty = (*editor).document().dirty((*text).text());
        (*editor).execute("word-count");
        check(services.last_message.starts_with("Document: 2 words\nSelection: 2 words"),
              "Word Count reports document and selected text");
        check((*text).text() == selected && (*text).selection() == count_selection,
              "Word Count preserves text and selection");
        check((*editor).document().dirty((*text).text()) == count_dirty,
              "Word Count preserves save state");
        (*text).replace_selection("replacement");
        (*editor).execute("undo");
        check((*text).text() == "first\r\nsecond", "Menu undo");
        (*editor).execute("redo");
        check((*text).text() == "replacement", "Menu redo");
        (*editor).execute("select-all");
        (*editor).execute("copy");
        check(services.clipboard == "replacement", "Copy uses host clipboard");
        (*editor).execute("cut");
        check((*text).text().empty(), "Cut");
        (*editor).execute("paste");
        check((*text).text() == "replacement", "Paste");
        (*editor).execute("wrap");
        check((*text).word_wrap(), "Word wrap menu");
        const std::string before_scale = std::string((*text).text());
        const gf::TextSelection selection_before_scale = (*text).selection();
        window.set_scale(1.5);
        window.perform_layout();
        static_cast<void>((*text).visual_line_count());
        window.set_scale(2.0);
        window.perform_layout();
        static_cast<void>((*text).visual_line_count());
        window.set_scale(1.0);
        window.perform_layout();
        check((*text).text() == before_scale && (*text).selection() == selection_before_scale,
              "DPI transitions preserve document and selection");
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("notepad-ui-tests-" + std::to_string(GetCurrentProcessId()) + ".txt");
        const bool observed_2 = std::filesystem::exists(path);
        check(!observed_2, "Unique fixture");
        struct Cleanup {
            std::filesystem::path path{};
            ~Cleanup() {
                std::error_code ec{};
                std::filesystem::remove(path, ec);
            }
        } cleanup{path};
        {
            std::ofstream out(path, std::ios::binary);
            out << "one\ntwo\r\nthree\r";
        }
        (*editor).open_file(path);
        check((*text).text() == "one\ntwo\r\nthree\r", "Open mixed endings unchanged");
        check(!(*text).can_undo(), "Open resets undo history");
        (*text).select_all();
        (*text).replace_selection("changed\n");
        (*editor).execute("save");
        const notepad::FileSnapshot observed_1 = notepad::read_file(path);
        check(observed_1.bytes == "changed\n", "Menu save exact bytes");
        check(!(*editor).document().dirty((*text).text()), "Save updates clean boundary");
        (*editor).execute("undo");
        check(!(*editor).document().dirty((*text).text()) && !(*text).can_undo(),
              "Save establishes undo boundary");
        services.choices.push_back(gf::HostDialogChoice::yes);
        (*editor).execute("restore-opened");
        check((*text).text() == "one\ntwo\r\nthree\r", "Original opened snapshot survives save");
        (*editor).execute("newline-lf");
        check((*text).text() == "one\ntwo\nthree\n", "Explicit newline conversion");
        (*editor).execute("undo");
        check((*text).text() == "one\ntwo\r\nthree\r", "Newline conversion is one undo action");
        services.clipboard = std::string(500001, 'x');
        services.choices.push_back(gf::HostDialogChoice::no);
        (*editor).execute("paste");
        check((*text).text() == "one\ntwo\r\nthree\r", "Large paste cancellation preserves text");
        {
            std::ofstream out(path, std::ios::binary);
            out << std::string(5000, 'x');
        }
        bool refused = false;
        try {
            (*editor).open_file(path);
        } catch (const std::exception &) {
            refused = true;
        }
        check(refused && (*text).text() == "one\ntwo\r\nthree\r",
              "Oversize line open preserves current document");
        services.choices.push_back(gf::HostDialogChoice::cancel);
        (*editor).execute("new");
        check((*text).text() == "one\ntwo\r\nthree\r", "Unsaved Cancel preserves document");
        services.choices.push_back(gf::HostDialogChoice::yes);
        (*editor).execute(
            "new"); // External file was replaced with long-line fixture: save must fail.
        check((*text).text() == "one\ntwo\r\nthree\r", "Failed unsaved Save cancels New");
        services.choices.push_back(gf::HostDialogChoice::no);
        (*editor).execute("new");
        check((*text).text().empty() && (*editor).document().path.empty(),
              "Unsaved Discard permits New");
        const std::filesystem::path csv_path = path.string() + ".csv";
        struct CsvCleanup {
            std::filesystem::path path{};
            ~CsvCleanup() {
                std::error_code error{};
                std::filesystem::remove(path, error);
            }
        } csv_cleanup{csv_path};
        {
            std::ofstream csv_file(csv_path, std::ios::binary);
            csv_file << "2,=A1*3\r\n4,5";
        }
        (*editor).open_file(csv_path);
        (*editor).execute("csv-view");
        const std::shared_ptr<notepad::CsvView> grid = (*editor).csv_control();
        check((*grid).visible() && !(*text).visible(), "CSV table replaces source presentation");
        (*grid).select_cell({0, 1});
        (*grid).commit_cell("=A1*4");
        check((*text).text() == "2,=A1*4\r\n4,5", "Enter stores formula source");
        (*editor).execute("csv-convert-value");
        check((*text).text() == "2,8\r\n4,5", "CSV menu converts formula to value");
        (*editor).execute("undo");
        check((*text).text() == "2,=A1*4\r\n4,5", "Conversion is one undo step");
        const std::vector<gf::MenuItemSpec> &context_items = (*grid).context_menu().items();
        check(context_items.size() == 1 && context_items[0].text == "Convert to Value",
              "Cell context menu includes Convert to Value");
        window.perform_layout();
        const gf::Rect grid_bounds = (*grid).absolute_bounds();
        gf::PointerEvent context_press{};
        context_press.action = gf::PointerAction::down;
        context_press.button = gf::PointerButton::secondary;
        context_press.position = {grid_bounds.x + 56 + 144 + 12, grid_bounds.y + 68 + 12};
        (*grid).on_pointer(context_press);
        check((*grid).context_menu().is_open(), "Right click opens the cell menu");
        const bool context_executed = (*context_items[0].command).execute("csv.context");
        check(context_executed && (*text).text() == "2,8\r\n4,5",
              "Context menu invokes the same conversion operation");
        (*grid).context_menu().close();
        (*editor).execute("undo");
        (*editor).execute("copy");
        check(services.clipboard == "=A1*4", "Grid copy uses selected cell source");
        (*grid).commit_cell("=B1");
        const std::string cyclic_source((*text).text());
        (*grid).convert_to_value();
        check((*text).text() == cyclic_source, "Conversion error preserves formula source");
        check((*grid).status().find("Circular") != std::string::npos,
              "Grid exposes formula conversion error");
        (*editor).execute("csv-clear");
        check((*text).text() == "2,\r\n4,5", "Grid Delete clears contents without shifting");
        (*editor).execute("csv-view");
        check((*text).visible(), "Source toggle restores plain text view");
        const std::string before_render((*text).text());
        const gf::TextSelection before_render_selection = (*text).selection();
        (*editor).execute("markdown-view");
        check(!(*text).visible(), "Rendered Markdown replaces source presentation");
        (*editor).execute("markdown-view");
        check((*text).text() == before_render && (*text).selection() == before_render_selection,
              "Markdown toggle preserves source and selection");
        (*text).set_text("a?c a\xc3\xa9"
                         "c ae\xcc\x81"
                         "c aZZc");
        const std::shared_ptr<notepad::QueryField> query = (*editor).query_control();
        (*query).set_text("a?c");
        (*text).select(gf::Utf8Offset(0), gf::Utf8Offset(0));
        (*editor).execute("find-next");
        check((*text).selected_text() == "a?c", "GUI literal question-mark search");
        (*query).toggle_slot(1);
        (*editor).execute("find-next");
        check((*text).selected_text() == "a\xc3\xa9"
                                         "c",
              "GUI wildcard selects exact Unicode source bytes");
        (*editor).execute("find-next");
        check((*text).selected_text() == "ae\xcc\x81"
                                         "c",
              "GUI wildcard consumes one combining grapheme");
        (*editor).execute("find-next");
        check((*text).selected_text() == "a?c", "Wildcard skips two-character gap and wraps");
        std::cout << "Editor headless tests passed: native control input routing, CRLF, menu "
                     "edit/save, undo boundary, mixed endings, oversized-line refusal.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
