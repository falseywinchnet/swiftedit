#include "editor.hpp"
#include <deque>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "platform.hpp"

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
void verify_conflict_fields() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("swiftedit-conflict-ui-" + std::to_string(test_process_id()));
    check(std::filesystem::create_directory(dir), "Unique conflict fixture");
    struct Cleanup {
        std::filesystem::path path{};
        ~Cleanup() {
            std::error_code error{};
            std::filesystem::remove_all(path, error);
        }
    } cleanup{dir};
    const std::filesystem::path path = dir / "original.txt";
    {
        std::ofstream file(path, std::ios::binary);
        file << "opened";
    }
    const std::shared_ptr<notepad::Editor> editor =
        gf::make_control<notepad::Editor>(gf::StableId("conflict-test.editor"));
    std::vector<gf::ApplicationWindow> windows = (*editor).application_windows({});
    (*editor).open_file(path);
    (*(*editor).text_control()).set_text("mine\n");
    {
        std::ofstream file(path, std::ios::binary);
        file << "external";
    }
    (*editor).execute("save");
    (*editor).execute("conflict-copy");
    const gf::Control::Ptr root = (*windows[8].model).root();
    const std::shared_ptr<gf::TextBox> filename =
        std::dynamic_pointer_cast<gf::TextBox>(find_control(root, "conflict.filename"));
    const std::shared_ptr<gf::ComboBox> encoding =
        std::dynamic_pointer_cast<gf::ComboBox>(find_control(root, "conflict.encoding"));
    const std::shared_ptr<gf::ComboBox> endings =
        std::dynamic_pointer_cast<gf::ComboBox>(find_control(root, "conflict.endings"));
    check(filename && encoding && endings, "Review controls are real owned dialog children");
    check((*filename).text() == notepad::path_utf8(dir / "original.1.txt"),
          "New copy suggests versioned name");
    (*encoding).set_selected_index(3);
    (*endings).set_selected_index(2);
    (*editor).execute("conflict-review");
    const std::filesystem::path copy = dir / "edited-name.txt";
    (*filename).set_text(notepad::path_utf8(copy));
    (*editor).execute("conflict-save");
    check((*editor).conflict_pending() && !std::filesystem::exists(copy),
          "Changed filename requires another review");
    (*editor).execute("conflict-review");
    (*editor).execute("conflict-save");
    const notepad::Decoded copied = notepad::decode(notepad::read_file(copy).bytes);
    check(!(*editor).conflict_pending() && (*editor).document().path == copy &&
              copied.encoding == notepad::Encoding::utf16_be && copied.text == "mine\r\n" &&
              notepad::read_file(path).bytes == "external" &&
              (*(*editor).text_control()).text() == copied.text,
          "Reviewed new filename and metadata publish together while preserving original");
}
void verify_csv_keyboard_commands() {
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("swiftedit-csv-keys-" + std::to_string(test_process_id()));
    check(std::filesystem::create_directory(directory), "Unique CSV keyboard fixture");
    struct Cleanup {
        std::filesystem::path directory{};
        ~Cleanup() {
            std::error_code error{};
            std::filesystem::remove_all(directory, error);
        }
    } cleanup{directory};
    const std::filesystem::path path = directory / "keys.csv";
    {
        std::ofstream file(path, std::ios::binary);
        file << "2,=A1*3\r\n4,5";
    }
    const std::shared_ptr<notepad::Editor> editor =
        gf::make_control<notepad::Editor>(gf::StableId("keyboard.editor"));
    std::vector<gf::ApplicationWindow> windows = (*editor).application_windows({});
    gf::Window &window = *windows[0].model;
    TestServices services{};
    gf::HostSession host(window, capabilities(), &services);
    const gf::HostDispatchResult attached =
        host.dispatch({1, 0, gf::HostAttachEvent{{800, 600}, 1}});
    check(attached.accepted(), "Keyboard test host attached");
    windows[0].options.ready(window, {});
    const std::shared_ptr<gf::TextBox> text = (*editor).text_control();
    const std::shared_ptr<notepad::CsvView> grid = (*editor).csv_control();
    (*editor).open_file(path);
    (*editor).execute("csv-view");
    check((*grid).visible(), "Keyboard test has visible CSV grid");
    window.perform_layout();
    for (const gf::Modifier modifier : {gf::Modifier::control, gf::Modifier::meta}) {
        (*grid).select_cell({0, 1});
        window.request_focus(grid);
        gf::KeyEvent key{};
        key.modifiers = modifier;
        key.physical_key = gf::PhysicalKey::c;
        check(window.dispatch_key(key) && services.clipboard == "=A1*3",
              "CSV Ctrl/Cmd+C copies formula source");
        key.physical_key = gf::PhysicalKey::x;
        check(window.dispatch_key(key) && (*text).text() == "2,\r\n4,5" &&
                  services.clipboard == "=A1*3",
              "CSV Ctrl/Cmd+X copies then clears the cell");
        key.physical_key = gf::PhysicalKey::z;
        check(window.dispatch_key(key) && (*text).text() == "2,=A1*3\r\n4,5",
              "CSV Ctrl/Cmd+Z restores cut in one edit");
        services.clipboard = "7";
        key.physical_key = gf::PhysicalKey::v;
        check(window.dispatch_key(key) && (*text).text() == "2,7\r\n4,5",
              "CSV Ctrl/Cmd+V commits the clipboard to the active cell");
        key.physical_key = gf::PhysicalKey::z;
        window.dispatch_key(key);
        key.modifiers = modifier | gf::Modifier::shift;
        check(window.dispatch_key(key) && (*text).text() == "2,7\r\n4,5",
              "CSV Ctrl/Cmd+Shift+Z redoes the paste");
        key.modifiers = modifier;
        window.dispatch_key(key);
    }
    const std::shared_ptr<gf::TextBox> entry =
        std::dynamic_pointer_cast<gf::TextBox>(window.find("csv.entry"));
    check(static_cast<bool>(entry), "CSV entry exists");
    window.request_focus(entry);
    (*entry).select(gf::Utf8Offset(1), gf::Utf8Offset(3));
    gf::KeyEvent copy{};
    copy.physical_key = gf::PhysicalKey::c;
    copy.modifiers = gf::Modifier::control;
    check(window.dispatch_key(copy) && services.clipboard == "A1",
          "Focused cell entry keeps its own text-selection copy behavior");
    for (const bool keyboard : {false, true}) {
        (*text).select_all();
        (*grid).select_cell({1, 0});
        window.request_focus(grid);
        if (keyboard) {
            gf::KeyEvent insert_time{};
            insert_time.physical_key = gf::PhysicalKey::f5;
            check(window.dispatch_key(insert_time), "CSV F5 is handled");
        } else
            (*editor).execute("date-time");
        const swiftedit::Csv updated((*text).text());
        const std::string &timestamp = updated.cell({1, 0}).value;
        check(timestamp.size() == 19 && timestamp[4] == '-' && timestamp[7] == '-' &&
                  timestamp[10] == ' ' && timestamp[13] == ':' && timestamp[16] == ':',
              "CSV date/time command stores a plain timestamp in the selected cell");
        check(updated.cell({0, 0}).value == "2" && updated.cell({0, 1}).value == "=A1*3" &&
                  updated.cell({1, 1}).value == "5" && (*text).text().starts_with("2,=A1*3\r\n"),
              "Date/time insertion preserves other cells and original record endings");
        (*editor).execute("undo");
        check((*text).text() == "2,=A1*3\r\n4,5", "CSV date/time insertion is one undoable edit");
    }
    const std::shared_ptr<notepad::QueryField> query = (*editor).query_control();
    const std::shared_ptr<gf::TextBox> replacement = (*editor).replacement_control();
    (*editor).execute("csv-view");
    for (const std::string view : {"csv-view", "markdown-view"}) {
        for (const std::string command :
             {"find", "replace", "find-next", "replace-one", "replace-all"}) {
            (*text).set_text("2,=A1*3\r\n4,5");
            (*query).set_text("4");
            (*replacement).set_text("9");
            if (command == "replace-one")
                (*text).select(gf::Utf8Offset(9), gf::Utf8Offset(10));
            else
                (*text).select(gf::Utf8Offset(0), gf::Utf8Offset(0));
            (*editor).execute(view);
            check(!(*text).visible(), "Search fixture starts with source hidden");
            (*editor).execute(command);
            check((*text).visible(), "Source search commands reveal the editable source");
            if (command == "find-next")
                check((*text).selected_text() == "4",
                      "Find Next exposes the matched source selection");
            if (command == "replace-one" || command == "replace-all") {
                check((*text).text() == "2,=A1*3\r\n9,5",
                      "Replacement from a presentation view changes the intended source");
                (*editor).execute("undo");
                check((*text).text() == "2,=A1*3\r\n4,5",
                      "Replacement from a presentation view remains undoable");
            } else
                check((*text).text() == "2,=A1*3\r\n4,5",
                      "Revealing source for search does not mutate content");
        }
    }
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
void verify_grapheme_status() {
    const std::shared_ptr<notepad::Editor> editor =
        gf::make_control<notepad::Editor>(gf::StableId("test.status-editor"));
    gf::Window window(editor, {800, 600});
    const std::shared_ptr<gf::TextBox> text = (*editor).text_control();
    const std::shared_ptr<gf::Label> status =
        std::dynamic_pointer_cast<gf::Label>(window.find("notepad.status"));
    check(status != nullptr, "Status label exists");
    const std::string first_line = "e\xcc\x81\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb";
    (*text).set_text(first_line + "\r\nx\rz\n");
    (*text).select(gf::Utf8Offset(3), gf::Utf8Offset(3));
    check((*status).text().starts_with("Ln 1, Col 2 |"),
          "Combining sequence advances one displayed character column");
    (*text).select(gf::Utf8Offset(first_line.size()), gf::Utf8Offset(first_line.size()));
    check((*status).text().starts_with("Ln 1, Col 3 |"),
          "Joined emoji advances one displayed character column");
    const gf::Utf8Offset second_line(first_line.size() + 2);
    (*text).select(second_line, second_line);
    check((*status).text().starts_with("Ln 2, Col 1 |"), "CRLF is one line break");
    const gf::Utf8Offset end((*text).text().size());
    (*text).select(end, end);
    check((*status).text().starts_with("Ln 4, Col 1 |"),
          "Mixed endings and trailing empty line use indexed line starts");
    (*text).set_text("");
    check((*status).text().starts_with("Ln 1, Col 1 | 0 characters |"),
          "Empty document starts at line one column one");
    (*text).set_text("abc");
    (*text).select(gui_forms::Utf8Offset(0), gui_forms::Utf8Offset(3));
    (*text).replace_selection("e\xcc\x81");
    check((*status).text().starts_with("Ln 1, Col 2 | 1 characters |"),
          "Same-byte-length edit refreshes grapheme metadata before selection notification");
    (*editor).execute("undo");
    check((*status).text().find("3 characters |") != std::string::npos,
          "Undo refreshes cached character metadata");
    (*text).set_text("x\n");
    (*text).set_text("x\r");
    (*text).select(gui_forms::Utf8Offset(0), gui_forms::Utf8Offset(0));
    check((*status).text().starts_with("Ln 1, Col 1 |") && (*status).text().ends_with(" | CR"),
          "Caret-only update retains the latest same-length ending change");
}
int main() {
    try {
        verify_callback_revocation();
        verify_csv_keyboard_commands();
        verify_conflict_fields();
        verify_grapheme_status();
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
        const std::string entered = "first" + notepad::native_newline() + "second";
        check((*text).text() == entered, "New-document Enter uses native endings");
        (*editor).execute("select-all");
        const std::string selected = (*text).selected_text();
        check(selected == entered, "Select all");
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
        check((*text).text() == entered, "Menu undo");
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
            ("notepad-ui-tests-" + std::to_string(test_process_id()) + ".txt");
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
        (*editor).execute("save");
        check((*editor).save_choice_pending() && !(*editor).enabled(),
              "Mixed save presents an owned choice before changing text or disk");
        check(notepad::read_file(path).bytes == "changed\n", "Pending mixed choice does not write");
        (*editor).execute("cancel-save");
        check(!(*editor).save_choice_pending() && (*editor).enabled() && (*text).can_undo(),
              "Mixed save cancellation preserves history and restores editor authority");
        (*editor).execute("save");
        (*editor).execute("save-as-is");
        check(notepad::read_file(path).bytes == "one\ntwo\r\nthree\r" && !(*text).can_undo(),
              "Save As-Is preserves exact mixed bytes and establishes save boundary");
        (*text).select(gf::Utf8Offset(0), gf::Utf8Offset(0));
        (*text).replace_selection("prefix ");
        (*editor).execute("save");
        (*editor).execute("save-normalized");
        check((*text).text() == "prefix one\ntwo\nthree\n" &&
                  notepad::read_file(path).bytes == (*text).text() && !(*text).can_undo(),
              "Convert to document default publishes and displays exactly the normalized text");
        services.choices.push_back(gf::HostDialogChoice::yes);
        (*editor).execute("restore-opened");
        (*editor).execute("save");
        {
            std::ofstream changed_destination(path, std::ios::binary);
            changed_destination << "external during choice";
        }
        (*editor).execute("save-normalized");
        check((*text).text() == "one\ntwo\r\nthree\r" && (*text).can_undo() &&
                  notepad::read_file(path).bytes == "external during choice",
              "Destination race refuses normalization/save without losing source or undo");
        (*editor).execute("save");
        check((*editor).conflict_pending() && !(*editor).enabled(),
              "External change opens first-stage conflict choice");
        (*editor).execute("conflict-save");
        check(notepad::read_file(path).bytes == "external during choice",
              "Cannot skip conflict stages");
        (*editor).execute("conflict-over");
        (*editor).execute("conflict-review");
        {
            std::ofstream external(path, std::ios::binary);
            external << "changed after review";
        }
        (*editor).execute("conflict-save");
        check((*editor).conflict_pending() && (*text).can_undo() &&
                  notepad::read_file(path).bytes == "changed after review",
              "Conflict review race preserves source, history and external file");
        (*editor).execute("conflict-review");
        (*editor).execute("conflict-save");
        check(!(*editor).conflict_pending() && (*editor).enabled() && !(*text).can_undo() &&
                  notepad::read_file(path).bytes == "one\ntwo\r\nthree\r",
              "Explicit reviewed overwrite preserves mixed endings and clears history on success");
        (*text).select_all();
        (*text).replace_selection("new saved baseline\n");
        (*editor).execute("save");
        (*text).select_all();
        (*text).replace_selection("one\ntwo\r\nthree\r");
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
        (*editor).execute("cancel-save");
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
        const gf::TextSelection hidden_selection = (*text).selection();
        (*editor).execute("select-all");
        check((*text).selection() == hidden_selection,
              "CSV Select All does not change the hidden source selection");
        (*editor).execute("copy");
        check(services.clipboard == "2\t\r\n4\t5", "CSV Select All copies every cell");
        (*editor).execute("delete");
        check((*text).text() == ",\r\n,", "CSV Select All Delete preserves table structure");
        (*editor).execute("undo");
        check((*text).text() == "2,\r\n4,5", "Clearing the table is one undoable edit");
        (*text).set_text("a,b,c\r\nd\ne,f");
        (*editor).execute("select-all");
        (*editor).execute("copy");
        check(services.clipboard == "a\tb\tc\r\nd\r\ne\tf",
              "Select All copies ragged rows without inventing trailing fields");
        (*editor).execute("delete");
        check((*text).text() == ",,\r\n\n,", "Select All clears all existing ragged cells");
        (*editor).execute("undo");
        check((*text).text() == "a,b,c\r\nd\ne,f", "Ragged clear restores with one undo");
        (*editor).execute("select-all");
        (*grid).select_cell({2, 1});
        (*editor).execute("copy");
        check(services.clipboard == "f", "Selecting a cell leaves whole-table selection");
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
        std::string repeated_line(4000, 'a');
        (*text).set_text(repeated_line);
        (*text).select(gf::Utf8Offset(0), gf::Utf8Offset(0));
        (*query).set_text(std::string(127, 'a') + "b");
        (*editor).execute("find-next");
        check((*editor).search_pending(), "Adversarial Find yields with scheduled work pending");
        (*query).toggle_slot(0);
        static_cast<void>(
            window.poll_frame_schedule(gf::FrameClock::now() + std::chrono::seconds(1)));
        check(!(*editor).search_pending() && (*text).selection().empty(),
              "Query flag change cancels pending Find without publishing stale selection");
        (*editor).execute("find-next");
        check((*editor).search_pending(), "Restarted search is scheduled");
        (*text).select(gf::Utf8Offset(1), gf::Utf8Offset(1));
        static_cast<void>(
            window.poll_frame_schedule(gf::FrameClock::now() + std::chrono::seconds(1)));
        check(!(*editor).search_pending() && (*text).selection().caret.value() == 1,
              "Moving the caret cancels pending search without overriding navigation");
        const std::string replacement_source(3000, 'a');
        (*text).set_text(replacement_source);
        (*query).set_text("a");
        const std::shared_ptr<gf::TextBox> replacement = (*editor).replacement_control();
        (*replacement).set_text("b");
        (*editor).execute("replace-all");
        check((*editor).search_pending() && (*text).text() == replacement_source,
              "Replace All yields without publishing partial document changes");
        (*replacement).set_text("c");
        static_cast<void>(
            window.poll_frame_schedule(gf::FrameClock::now() + std::chrono::seconds(1)));
        check(!(*editor).search_pending() && (*text).text() == replacement_source,
              "Changing replacement text cancels the prepared edit intact");
        (*replacement).set_text("b");
        (*editor).execute("replace-all");
        for (std::size_t index = 0; (*editor).search_pending() && index < 20; ++index)
            static_cast<void>(
                window.poll_frame_schedule(gf::FrameClock::now() + std::chrono::seconds(1)));
        check(!(*editor).search_pending() && (*text).text() == std::string(3000, 'b'),
              "Scheduled Replace All publishes the complete result");
        (*editor).execute("undo");
        check((*text).text() == replacement_source, "Scheduled Replace All is one undoable edit");
        (*replacement).set_text("long");
        const gf::TextSelection before_failed_replace = (*text).selection();
        (*editor).execute("replace-all");
        for (std::size_t index = 0; (*editor).search_pending() && index < 20; ++index)
            static_cast<void>(
                window.poll_frame_schedule(gf::FrameClock::now() + std::chrono::seconds(1)));
        check(!(*editor).search_pending() && (*text).text() == replacement_source &&
                  (*text).selection() == before_failed_replace,
              "Oversized replacement line is refused without content or selection mutation");
        (*text).set_text("e\xcc\x81");
        (*text).select(gf::Utf8Offset(0), gf::Utf8Offset(0));
        const gf::TextSelection inspector_selection = (*text).selection();
        (*editor).execute("inspect-characters");
        check(services.last_message.find("COMBINING ACUTE ACCENT") != std::string::npos &&
                  services.last_message.find("U+0065") != std::string::npos,
              "Inspector at caret covers the complete combining grapheme");
        check((*text).text() == "e\xcc\x81" && (*text).selection() == inspector_selection,
              "Inspector preserves source and selection");
        const std::shared_ptr<notepad::CharacterPicker> ordinary_picker =
            (*editor).character_picker(false);
        (*ordinary_picker).select_codepoint(0x1f600);
        (*ordinary_picker).insert_selected();
        check((*text).text() == "\xf0\x9f\x98\x80"
                                "e\xcc\x81",
              "Unicode picker inserts the exact supplementary scalar");
        (*editor).execute("undo");
        check((*text).text() == "e\xcc\x81", "Character insertion is one undoable edit");
        const std::shared_ptr<notepad::CharacterPicker> control_picker =
            (*editor).character_picker(true);
        (*control_picker).select_codepoint(0x202e);
        check((*control_picker).status().find("RIGHT-TO-LEFT OVERRIDE") != std::string::npos &&
                  (*control_picker).status().find("\xe2\x80\xae") == std::string::npos,
              "Control dialog explains the selected scalar without executing it in its labels");
        (*control_picker).insert_selected();
        check((*text).text() == "\xe2\x80\xae"
                                "e\xcc\x81",
              "Explicit control insertion preserves literal source bytes");
        (*editor).execute("undo");
        check((*text).text() == "e\xcc\x81", "Control insertion can be undone exactly");
        bool separate_dialog = false;
        try {
            (*ordinary_picker).select_codepoint(0x202e);
        } catch (const std::runtime_error &) {
            separate_dialog = true;
        }
        check(separate_dialog && (*text).text() == "e\xcc\x81",
              "Ordinary picker refuses controls intact");
        {
            gf::Window character_window(ordinary_picker, {680, 600});
            gf::HostSession character_host(character_window, capabilities(), &services);
            const gf::HostDispatchResult attached_picker =
                character_host.dispatch({1, 0, gf::HostAttachEvent{{680, 600}, 1}});
            check(attached_picker.accepted(), "Character dialog has shared host services");
            (*ordinary_picker).copy_selected();
            check(services.clipboard == "\xf0\x9f\x98\x80" && (*text).text() == "e\xcc\x81",
                  "Unicode picker copies exact scalar without mutating the document");
            character_host.shutdown();
        }
        {
            gf::Window control_window(control_picker, {680, 600});
            gf::HostSession control_host(control_window, capabilities(), &services);
            const gf::HostDispatchResult attached_controls =
                control_host.dispatch({1, 0, gf::HostAttachEvent{{680, 600}, 1}});
            check(attached_controls.accepted(), "Control dialog has shared host services");
            (*control_picker).select_codepoint(0);
            services.clipboard = "preserved clipboard";
            bool nul_copy_refused = false;
            try {
                (*control_picker).copy_selected();
            } catch (const std::runtime_error &) {
                nul_copy_refused = true;
            }
            check(nul_copy_refused && services.clipboard == "preserved clipboard",
                  "NUL clipboard refusal preserves the existing clipboard");
            (*control_picker).insert_selected();
            check((*text).text() == std::string("\0e\xcc\x81", 4),
                  "NUL remains insertable as a literal source byte");
            (*editor).execute("undo");
            check((*text).text() == "e\xcc\x81", "NUL insertion undoes without loss");
            control_host.shutdown();
        }
        std::cout << "Editor headless tests passed: native control input routing, CRLF, menu "
                     "edit/save, undo boundary, mixed endings, oversized-line refusal.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
