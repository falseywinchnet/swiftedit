#include "editor.hpp"
#include "session.hpp"
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace notepad {
std::string path_utf8(const std::filesystem::path& path) { auto s=path.u8string(); return {reinterpret_cast<const char*>(s.data()),s.size()}; }
Editor::Editor(gf::StableId id) : Control(std::move(id)) {}
gf::MenuItemSpec Editor::item(std::string id,std::string label,std::string shortcut_text) {
    auto command=std::make_shared<gf::Command>(id,label);
    command->set_shortcut(shortcut_text);
    subscriptions_.push_back(command->invoked().subscribe(*this,[this,id](const auto&){ execute(id); }));
    commands_[id]=command;
    return {id,gf::MenuItemKind::command,command,label};
}
void Editor::initialize_control_tree() {
    menu_=gf::make_control<gf::MenuStrip>(gf::StableId("notepad.menus"));
    menu_->set_items({
        {"file","&File",{item("new","&New","Ctrl+N"),item("open","&Open...","Ctrl+O"),item("save","&Save","Ctrl+S"),item("save-as","Save &As...","Ctrl+Shift+S"),item("exit","E&xit","Alt+F4")}},
        {"edit","&Edit",{item("undo","&Undo","Ctrl+Z"),item("redo","&Redo","Ctrl+Y"),item("cut","Cu&t","Ctrl+X"),item("copy","&Copy","Ctrl+C"),item("paste","&Paste","Ctrl+V"),item("delete","&Delete","Del"),item("find","&Find...","Ctrl+F"),item("find-next","Find &Next","F3"),item("replace","&Replace...","Ctrl+H"),item("select-all","Select &All","Ctrl+A")}},
        {"document","&Document",{item("restore-opened","Restore As &Opened..."),item("date-time","Insert &Date and Time","F5"),item("newline-lf","Convert Line Endings to &LF"),item("newline-crlf","Convert Line Endings to &CRLF")}},
        {"format","F&ormat",{item("wrap","&Word Wrap"),item("font","&Font...")}},
        {"view","&View",{item("status","&Status Bar")}},
        {"help","&Help",{item("help","View &Help","F1"),item("about","&About SwiftEdit")}}
    });
    add_child(menu_);
    name_=gf::make_control<gf::Label>(gf::StableId("notepad.document-name")); add_child(name_);
    text_=gf::make_control<gf::TextBox>(gf::StableId("notepad.document"));
    text_->set_multiline(true); text_->set_word_wrap(false);
    text_->set_accepts_tab(true); text_->set_newline_sequence("\r\n");
    text_->set_maximum_length(gf::TextBox::maximum_multiline_bytes);
    text_->set_font({gf::FontRole::monospace,14,400,false});
    text_->set_accessible_name("Document text");
    add_child(text_);
    status_=gf::make_control<gf::Label>(gf::StableId("notepad.status")); add_child(status_);
    subscriptions_.push_back(text_->text_changed().subscribe(*this,[this](const auto&){refresh();}));
    subscriptions_.push_back(text_->selection_changed().subscribe(*this,[this](const auto&){refresh();}));
    build_find(); build_font(); refresh();
}
void Editor::arrange(gf::Rect bounds) {
    arrange_self(bounds);
    set_child_layout(menu_,{0,0,bounds.width,28});
    set_child_layout(name_,{8,28,std::max(0.0,bounds.width-16),24});
    const double bottom=show_status_ ? 26 : 0;
    set_child_layout(text_,{0,52,bounds.width,std::max(0.0,bounds.height-52-bottom)});
    set_child_layout(status_,{8,std::max(52.0,bounds.height-bottom),std::max(0.0,bounds.width-16),bottom});
}
void Editor::refresh() {
    if(!text_ || !status_) return;
    const auto content=text_->text();
    name_->set_text((document_.dirty(content)?"* ":"")+(document_.path.empty()?"Untitled":path_utf8(document_.path)));
    auto caret=std::min(text_->selection().caret.value(),content.size());
    std::size_t line=1,column=1;
    for(std::size_t i=0;i<caret;++i) {
        if(content[i]=='\r') { ++line; column=1; if(i+1<caret && content[i+1]=='\n') ++i; }
        else if(content[i]=='\n') { ++line; column=1; }
        else if((static_cast<unsigned char>(content[i])&0xc0)!=0x80) ++column;
    }
    if(content!=counted_text_) { counted_text_=content; character_count_=gf::TextStore(content).grapheme_count().value(); }
    const auto selected=gf::TextStore(text_->selected_text()).grapheme_count().value();
    status_->set_text("Ln "+std::to_string(line)+", Col "+std::to_string(column)+" | "+std::to_string(character_count_)+" characters | "+std::to_string(selected)+" selected | "+encoding_name(document_.encoding)+" | "+newline_name(content));
    commands_.at("undo")->set_enabled(text_->can_undo());
    commands_.at("redo")->set_enabled(text_->can_redo());
    for(auto id:{"cut","copy","delete"}) commands_.at(id)->set_enabled(!text_->selection().empty());
    commands_.at("wrap")->set_checked(text_->word_wrap());
    commands_.at("status")->set_checked(show_status_);
}
void Editor::focus_text() { if(window()) window()->request_focus(text_); }
gf::HostDialogChoice Editor::message(std::string title,std::string text,gf::HostMessageButtons buttons) {
    auto* owner=picker_active_?(active_save_picker_?save_picker_.window:open_picker_.window):window();
    if(!owner || !owner->host_services()) throw std::runtime_error("No dialog service is attached.");
    gf::HostMessageDialogRequest payload;
    payload.title=std::move(title); payload.message=std::move(text); payload.buttons=buttons;
    payload.default_choice=buttons==gf::HostMessageButtons::ok ? gf::HostDialogChoice::ok :
        buttons==gf::HostMessageButtons::yes_no ? gf::HostDialogChoice::no : gf::HostDialogChoice::cancel;
    gf::HostDialogRequest request; request.request_id=dialog_sequence_++;
    request.owner_id=picker_active_?(active_save_picker_?"notepad.save-picker":"notepad.open-picker"):"notepad.main";
    request.payload=std::move(payload);
    auto result=owner->host_services()->show_dialog(request);
    if(!result.status.accepted()) throw std::runtime_error("The dialog service could not complete the request.");
    return std::get<gf::HostMessageDialogResult>(result.payload).choice;
}
void Editor::error(const std::string& text) {
    status_->set_text(text); status_->set_visible(true);
    try { message("SwiftEdit",text); } catch(...) { /* Keep the error visible if native services fail. */ }
}
void Editor::after_unsaved(std::function<void()> next) {
    if(!document_.dirty(text_->text())) { next(); return; }
    const auto choice=message("Save changes?","Save changes to "+(document_.path.empty()?"Untitled":path_utf8(document_.path.filename()))+"?",gf::HostMessageButtons::yes_no_cancel);
    if(choice==gf::HostDialogChoice::no) next();
    else if(choice==gf::HostDialogChoice::yes) save(false,std::move(next));
}
void Editor::open_file(const std::filesystem::path& source) {
    Document next; next.open(source);
    const auto supported=gf::TextBox::validate_multiline_text(next.saved_text);
    if(supported!=gf::TextBox::MultilineValidation::valid)
        throw std::runtime_error("This development editor supports up to 1 MiB of UTF-8 text and 4096 UTF-8 bytes per logical line. The current document was kept.");
    text_->set_text(next.saved_text);
    document_=std::move(next);
    text_->set_newline_sequence(preferred_newline(document_.saved_text));
    text_->select(gf::Utf8Offset(0),gf::Utf8Offset(0));
    refresh(); focus_text();
}
bool Editor::save_to(const std::filesystem::path& path,const FileSnapshot& expected) {
    try { document_.save(path,text_->text(),expected); text_->clear_undo_history(); refresh(); return true; }
    catch(const std::exception& e) { error(e.what()); return false; }
}
void Editor::save(bool save_as,std::function<void()> continuation) {
    if(save_as || document_.path.empty()) { after_save_=std::move(continuation); show_picker(true); }
    else if(save_to(document_.path,document_.snapshot) && continuation) continuation();
}
void Editor::execute(const std::string& id) {
    if(picker_active_) return;
    try {
        if(id=="new") after_unsaved([this]{document_={};text_->set_text("");text_->set_newline_sequence("\r\n");refresh();focus_text();});
        else if(id=="open") after_unsaved([this]{show_picker(false);});
        else if(id=="save") save(false);
        else if(id=="save-as") save(true);
        else if(id=="exit") static_cast<void>(handle_.request_close());
        else if(id=="undo") text_->undo();
        else if(id=="redo") text_->redo();
        else if(id=="cut") text_->cut();
        else if(id=="copy") text_->copy();
        else if(id=="paste") {
            if(!window()||!window()->host_services())throw std::runtime_error("Clipboard service is unavailable.");
            auto clip=window()->host_services()->read_clipboard_text();
            if(!clip.status.accepted())throw std::runtime_error("Cannot read plain-text clipboard.");
            if(clip.text_utf8.size()>500000 && message("Large paste","Paste "+std::to_string(clip.text_utf8.size())+" bytes of plain text?",gf::HostMessageButtons::yes_no)!=gf::HostDialogChoice::yes)return;
            auto candidate=std::string(text_->text());candidate.replace(text_->selection().start().value(),text_->selection().length(),clip.text_utf8);
            if(gf::TextBox::validate_multiline_text(candidate)!=gf::TextBox::MultilineValidation::valid)throw std::runtime_error("Paste exceeds this GUI's document/line limits or contains invalid UTF-8. No text was changed.");
            text_->replace_selection(clip.text_utf8);
        }
        else if(id=="restore-opened") {
            if(message("Restore as opened?","Replace the working text with the original opened text? The file on disk will not change until you save.",gf::HostMessageButtons::yes_no)==gf::HostDialogChoice::yes){text_->select_all();text_->replace_selection(document_.opened_text);}
        }
        else if(id=="date-time") {
            auto now=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::tm local{};localtime_s(&local,&now);
            std::ostringstream out;out<<std::put_time(&local,"%Y-%m-%d %H:%M:%S");text_->replace_selection(out.str());
        }
        else if(id=="newline-lf"||id=="newline-crlf") {
            const std::string ending=id=="newline-lf"?"\n":"\r\n";auto converted=swiftedit::normalize_newlines(text_->text(),ending);
            if(gf::TextBox::validate_multiline_text(converted)!=gf::TextBox::MultilineValidation::valid)throw std::runtime_error("Converted text exceeds GUI limits.");
            text_->select_all();text_->replace_selection(converted);text_->set_newline_sequence(ending);
        }
        else if(id=="delete") text_->delete_selection();
        else if(id=="select-all") text_->select_all();
        else if(id=="find" || id=="replace") show_find();
        else if(id=="find-next") { if(query_->text().empty()) show_find(); else find_next(); }
        else if(id=="wrap") {text_->set_word_wrap(!text_->word_wrap());refresh();}
        else if(id=="status") {show_status_=!show_status_;status_->set_visible(show_status_);invalidate(gf::Dirty::layout);refresh();}
        else if(id=="font") static_cast<void>(font_.handle.show());
        else if(id=="help") message("SwiftEdit help","Use File > Open to edit a plain-text document. Ctrl+S saves. A star beside the filename means unsaved changes.\n\nFind/Replace is literal; Match case off folds English A-Z only. Search wraps once. Replace All is one undo action.\n\nThis build preserves UTF-8 and BOM-marked UTF-16, including existing line endings. Malformed or unsupported encodings are refused. Files changed externally are never silently overwritten: use Save As or reopen.\n\nWrap and font affect display only. Editable text is limited to 1 MiB of UTF-8 and 4096 UTF-8 bytes per logical line. Settings are session-only. Save resets ordinary Undo. Document > Restore As Opened recovers the original session text. Document also offers explicit newline conversion and date/time insertion (F5). The separate command-session executable supports bounded large-file pages and CSV calculations.");
        else if(id=="about") message("About SwiftEdit","SwiftEdit 0.2\nA standalone plain-text editor using GUI.Forms and the shared File Manager Document Picker.\n\nWindows development build; expanded objectives and implementation status are recorded in docs/SWIFTEDIT_OBJECTIVES.md.");
        refresh();
    } catch(const std::exception& e) { error(e.what()); }
}
void Editor::shortcut(gf::Window& w,std::uint32_t key,gf::Modifier mods,const std::string& id) {
    accelerators_.push_back(w.register_accelerator(*this,{key,mods},[this,id]{execute(id);return true;}));
}
void Editor::ready(gf::Window& w,gf::ApplicationWindowHandle handle,const std::filesystem::path& initial) {
    handle_=handle;
    using K=gf::PhysicalKey; using M=gf::Modifier;
    for(auto [key,id] : std::vector<std::pair<std::uint32_t,std::string>>{{K::n,"new"},{K::o,"open"},{K::s,"save"},{K::f,"find"},{K::h,"replace"}}) shortcut(w,key,M::control,id);
    shortcut(w,K::s,M::control|M::shift,"save-as"); shortcut(w,K::f3,M::none,"find-next"); shortcut(w,K::f1,M::none,"help");
    shortcut(w,K::v,M::control,"paste");shortcut(w,K::f5,M::none,"date-time");
    if(!initial.empty()) {try {open_file(initial);}catch(const std::exception& e){error(e.what());}}
    focus_text();
}
void Editor::closing(gf::HostCloseRequest& request) {
    if(close_authorized_) return;
    if(picker_active_) {request.cancel=true;return;}
    if(!document_.dirty(text_->text())) return;
    request.cancel=true;
    try { after_unsaved([this]{close_authorized_=true; if(window()) static_cast<void>(window()->begin_invoke(shared_from_this(),[this]{static_cast<void>(handle_.request_close());}));}); }
    catch(const std::exception& e){error(e.what());}
}
void Editor::build_find() {
    find_.root=gf::make_control<DialogLayout>(gf::StableId("notepad.find"));
    auto label=[&](const char* id,const char* text,gf::Rect bounds){auto c=gf::make_control<gf::Label>(gf::StableId(id),text);find_.root->place(c,bounds);};
    label("find.label","Find what:",{16,18,105,28});
    label("replace.label","Replace with:",{16,60,105,28});
    query_=gf::make_control<gf::TextBox>(gf::StableId("find.query"));query_->set_accessible_name("Find what");query_->set_maximum_length(4096);find_.root->place(query_,{125,16,360,30});
    replacement_=gf::make_control<gf::TextBox>(gf::StableId("find.replacement"));replacement_->set_accessible_name("Replace with");replacement_->set_maximum_length(4096);find_.root->place(replacement_,{125,58,360,30});
    match_case_=gf::make_control<gf::CheckBox>(gf::StableId("find.case"),"Match case");match_case_->set_checked(true);find_.root->place(match_case_,{16,102,200,28});
    auto button=[&](const char* id,const char* text,gf::Rect rect,std::function<void()> action){auto b=gf::make_control<gf::Button>(gf::StableId(id),text);find_.root->place(b,rect);subscriptions_.push_back(b->clicked().subscribe(*this,[this,action](auto&){try{action();}catch(const std::exception& e){find_status_->set_text(e.what());}}));return b;};
    find_next_button_=button("find.next","Find Next",{16,144,108,32},[this]{find_next();});
    button("find.replace","Replace",{136,144,100,32},[this]{replace_one();});
    button("find.all","Replace All",{248,144,108,32},[this]{replace_every();});
    find_close_=button("find.close","Close",{368,144,116,32},[this]{close_dialog(find_);});
    find_status_=gf::make_control<gf::Label>(gf::StableId("find.status"),"Literal search. Search wraps at the end.");find_.root->place(find_status_,{16,190,470,50});
}
void Editor::show_find() {
    if(!text_->selection().empty() && text_->selection().length()<4096) {
        auto selected=text_->selected_text(); if(selected.find_first_of("\r\n")==std::string::npos) query_->set_text(selected);
    }
    static_cast<void>(find_.handle.show()); if(find_.window) find_.window->request_focus(query_);
}
void Editor::find_next() {
    if(query_->text().empty()) {find_status_->set_text("Enter the literal text to find.");return;}
    auto found=find_literal(text_->text(),query_->text(),text_->selection().end().value(),match_case_->checked());
    bool wrapped=false;
    if(!found) {found=find_literal(text_->text(),query_->text(),0,match_case_->checked());wrapped=true;}
    if(!found) {find_status_->set_text("Text not found.");return;}
    text_->select(gf::Utf8Offset(*found),gf::Utf8Offset(*found+query_->text().size()));
    find_status_->set_text(wrapped?"Found (wrapped to beginning).":"Found.");refresh();
}
void Editor::replace_one() {
    auto selected=text_->selected_text();
    auto match=find_literal(selected,query_->text(),0,match_case_->checked());
    if(match && *match==0 && selected.size()==query_->text().size()) text_->replace_selection(replacement_->text());
    find_next();
}
void Editor::replace_every() {
    if(query_->text().empty()) {find_status_->set_text("Enter the literal text to replace.");return;}
    auto result=replace_all(text_->text(),query_->text(),replacement_->text(),match_case_->checked());
    if(gf::TextBox::validate_multiline_text(result.text)!=gf::TextBox::MultilineValidation::valid) {
        find_status_->set_text("Replacement exceeds editor document or line limits. No changes made.");return;
    }
    if(result.count) {text_->select_all();if(!text_->replace_selection(result.text)){find_status_->set_text("Replacement was not applied.");return;}}
    find_status_->set_text(std::to_string(result.count)+" replacement(s).");refresh();
}
void Editor::build_font() {
    font_.root=gf::make_control<DialogLayout>(gf::StableId("notepad.font"));
    auto label=gf::make_control<gf::Label>(gf::StableId("font.label"),"Font role and size (display only)");font_.root->place(label,{16,16,380,28});
    font_role_=gf::make_control<gf::ComboBox>(gf::StableId("font.role"));font_role_->set_items({"Monospace","Content"});font_role_->set_selected_index(0);font_role_->set_accessible_name("Font role");font_.root->place(font_role_,{16,58,220,30});
    font_size_=gf::make_control<gf::ComboBox>(gf::StableId("font.size"));font_size_->set_items({"10","12","14","16","18","20","24","28","32"});font_size_->set_selected_index(2);font_size_->set_accessible_name("Font size");font_.root->place(font_size_,{252,58,130,30});
    font_bold_=gf::make_control<gf::CheckBox>(gf::StableId("font.bold"),"Bold");font_.root->place(font_bold_,{16,106,160,28});
    font_italic_=gf::make_control<gf::CheckBox>(gf::StableId("font.italic"),"Italic");font_.root->place(font_italic_,{200,106,160,28});
    font_apply_=gf::make_control<gf::Button>(gf::StableId("font.apply"),"Apply");font_.root->place(font_apply_,{170,156,100,32});
    font_close_=gf::make_control<gf::Button>(gf::StableId("font.close"),"Close");font_.root->place(font_close_,{282,156,100,32});
    subscriptions_.push_back(font_apply_->clicked().subscribe(*this,[this](auto&){text_->set_font({font_role_->selected_index()==0?gf::FontRole::monospace:gf::FontRole::content,std::stod(std::string(font_size_->selected_text())),std::uint16_t(font_bold_->checked()?700:400),font_italic_->checked()});}));
    subscriptions_.push_back(font_close_->clicked().subscribe(*this,[this](auto&){close_dialog(font_);}));
}
void Editor::close_dialog(Dialog& dialog) {static_cast<void>(dialog.handle.hide());focus_text();}
}
