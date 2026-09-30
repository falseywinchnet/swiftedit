#pragma once
#include "document.hpp"
#include <gui_forms/gui_forms.hpp>
#include <gui_forms/application.hpp>
#include <file_manager/document_picker_view.hpp>
#include <functional>
#include <map>

namespace notepad {
namespace gf = gui_forms;
class DialogLayout final : public gf::Control {
public:
    explicit DialogLayout(gf::StableId id) : Control(std::move(id)) {}
    void place(const Ptr& control, gf::Rect bounds) { add_child(control); items.push_back({control,bounds}); }
    void arrange(gf::Rect bounds) override { arrange_self(bounds); for(auto& [child,rect]:items) set_child_layout(child,rect); }
private:
    std::vector<std::pair<Ptr,gf::Rect>> items;
};
class Editor final : public gf::Control {
public:
    explicit Editor(gf::StableId id);
    static constexpr bool initialize_tree_after_construction = true;
    void initialize_control_tree();
    void arrange(gf::Rect bounds) override;
    std::vector<gf::ApplicationWindow> application_windows(const std::filesystem::path& initial);
    void execute(const std::string& command);
    // Same entrypoints are used by the shell and deterministic consumer tests.
    void open_file(const std::filesystem::path& source);
    std::shared_ptr<gf::TextBox> text_control() const { return text_; }
    const Document& document() const { return document_; }
private:
    struct Picker {
        std::unique_ptr<file_manager::DocumentPickerView> view;
        gf::Window* window{};
        gf::ApplicationWindowHandle handle;
    };
    struct Dialog {
        std::shared_ptr<DialogLayout> root;
        gf::Window* window{};
        gf::ApplicationWindowHandle handle;
    };
    gf::MenuItemSpec item(std::string id,std::string label,std::string shortcut={});
    void ready(gf::Window&,gf::ApplicationWindowHandle,const std::filesystem::path&);
    void closing(gf::HostCloseRequest&);
    void refresh();
    void focus_text();
    void error(const std::string&);
    gf::HostDialogChoice message(std::string title,std::string text,gf::HostMessageButtons buttons=gf::HostMessageButtons::ok);
    void after_unsaved(std::function<void()> continuation);
    void save(bool save_as,std::function<void()> continuation={});
    bool save_to(const std::filesystem::path&,const FileSnapshot&);
    void show_picker(bool save_as);
    void picker_result(bool save_as,const file_manager::DocumentPickerResult&);
    void hide_picker(bool save_as);
    void show_find();
    void find_next();
    void replace_one();
    void replace_every();
    void build_find();
    void build_font();
    void close_dialog(Dialog&);
    void shortcut(gf::Window&,std::uint32_t,gf::Modifier,const std::string&);
    Document document_;
    std::shared_ptr<gf::TextBox> text_;
    std::shared_ptr<gf::MenuStrip> menu_;
    std::shared_ptr<gf::Label> name_,status_;
    std::map<std::string,std::shared_ptr<gf::Command>> commands_;
    std::vector<gf::SubscriptionToken> subscriptions_;
    std::vector<gf::AcceleratorToken> accelerators_;
    gf::ApplicationWindowHandle handle_;
    Picker open_picker_,save_picker_;
    Dialog find_,font_;
    std::shared_ptr<gf::TextBox> query_,replacement_;
    std::shared_ptr<gf::CheckBox> match_case_;
    std::shared_ptr<gf::Label> find_status_;
    std::shared_ptr<gf::Button> find_next_button_,find_close_,font_apply_,font_close_;
    std::shared_ptr<gf::ComboBox> font_role_,font_size_;
    std::shared_ptr<gf::CheckBox> font_bold_,font_italic_;
    bool show_status_{true},picker_active_{},active_save_picker_{},close_authorized_{};
    std::uint64_t dialog_sequence_{1};
    std::function<void()> after_save_;
    std::string counted_text_;
    std::size_t character_count_{};
};
std::string path_utf8(const std::filesystem::path&);
}
