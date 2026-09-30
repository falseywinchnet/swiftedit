#include "editor.hpp"
#include <windows.h>

namespace notepad {
std::vector<gf::ApplicationWindow> Editor::application_windows(const std::filesystem::path& initial) {
    std::vector<gf::ApplicationWindow> result;
    auto self=std::static_pointer_cast<Editor>(shared_from_this());
    gf::ApplicationWindow main;
    main.stable_id="notepad.main";
    main.model=std::make_unique<gf::Window>(self,gf::Size{940,660});
    main.options.title="Notepad";main.options.initial_size={940,660};main.options.minimum_size={540,320};
    main.options.ready=[self,initial](auto& w,auto h){self->ready(w,h,initial);};
    main.options.closing=[self](auto& request){self->closing(request);};
    result.push_back(std::move(main));
    auto start=initial.empty()?std::filesystem::current_path():initial.parent_path();
    for(bool save_as:{false,true}) {
        auto& picker=save_as?save_picker_:open_picker_;
        file_manager::DocumentPickerRequest request;
        request.profile=save_as?file_manager::DocumentPickerProfile::save_as:file_manager::DocumentPickerProfile::open_file;
        request.protected_root=start.root_path();request.initial_location=start;
        request.owner_application_id="org.malkuth.notepad";
        request.show_hidden=true;
        request.authority=file_manager::DocumentPickerAuthority::trusted_local_host;
        request.home_location=start;
        const auto drives=GetLogicalDrives();
        for(unsigned i=0;i<26;++i) if(drives&(1u<<i)) {
            std::wstring root{wchar_t(L'A'+i),L':',L'\\'};
            const auto kind=GetDriveTypeW(root.c_str());
            if(kind==DRIVE_FIXED || kind==DRIVE_REMOVABLE || kind==DRIVE_RAMDISK) request.admitted_roots.emplace_back(root);
        }
        request.filters={{"all","All files",{}},{"text","Text documents",{"txt","log","ini","cfg","md"}}};
        request.active_filter_id="all";request.suggested_name="Untitled.txt";
        picker.view=std::make_unique<file_manager::DocumentPickerView>(std::move(request));
        subscriptions_.push_back(picker.view->completed().subscribe(*this,[this,save_as](const auto& r){picker_result(save_as,r);}));
        gf::ApplicationWindow child;
        child.stable_id=save_as?"notepad.save-picker":"notepad.open-picker";
        child.owner_id="notepad.main";child.tool_window=true;
        child.model=std::make_unique<gf::Window>(picker.view->root_control(),gf::Size{800,600});
        child.options.title=save_as?"Save As - Notepad":"Open - Notepad";
        child.options.initial_size={800,600};child.options.minimum_size={680,520};
        child.options.initially_visible=false;child.options.hide_on_close=true;
        child.options.ready=[this,save_as](auto& w,auto h){auto& p=save_as?save_picker_:open_picker_;p.window=&w;p.handle=h;p.view->attach_dialog(w);};
        child.options.closing=[this,save_as](auto&){auto& p=save_as?save_picker_:open_picker_;p.view->cancel();hide_picker(save_as);after_save_={};};
        result.push_back(std::move(child));
    }
    auto dialog=[&](Dialog& d,const char* id,const char* title,gf::Size size,const std::shared_ptr<gf::Button>& accept,const std::shared_ptr<gf::Button>& cancel){
        gf::ApplicationWindow child;child.stable_id=id;child.owner_id="notepad.main";child.tool_window=true;
        child.model=std::make_unique<gf::Window>(d.root,size);child.options.title=title;
        child.options.initial_size=size;child.options.minimum_size=size;child.options.initially_visible=false;child.options.hide_on_close=true;
        child.options.ready=[&d,accept,cancel](auto& w,auto h){d.window=&w;d.handle=h;w.set_accept_button(accept);w.set_cancel_button(cancel);};
        child.options.closing=[this](auto&){focus_text();};
        result.push_back(std::move(child));
    };
    dialog(find_,"notepad.find-window","Find and Replace - Notepad",{510,260},find_next_button_,find_close_);
    dialog(font_,"notepad.font-window","Font - Notepad",{410,220},font_apply_,font_close_);
    return result;
}
void Editor::show_picker(bool save_as) {
    auto& picker=save_as?save_picker_:open_picker_;
    if(!picker.window || !picker.handle.active()) throw std::runtime_error("The shared file picker is not ready.");
    auto start=document_.path.empty()?std::filesystem::current_path():document_.path.parent_path();
    picker.view->set_authority_valid(true);
    if(save_as) static_cast<void>(picker.view->controller().set_filename(document_.path.empty()?"Untitled.txt":path_utf8(document_.path.filename())));
    picker.view->present(start);picker.view->attach_dialog(*picker.window);
    // Suppress every editor command during the owned selection session.
    static_cast<void>(find_.handle.hide());static_cast<void>(font_.handle.hide());
    picker_active_=true;active_save_picker_=save_as;set_enabled(false);
    auto status=picker.handle.show();
    if(!status.accepted()){picker_active_=false;set_enabled(true);after_save_={};throw std::runtime_error("The file picker could not be shown.");}
}
void Editor::hide_picker(bool save_as) {
    auto& picker=save_as?save_picker_:open_picker_;
    static_cast<void>(picker.handle.hide());picker_active_=false;set_enabled(true);focus_text();
}
void Editor::picker_result(bool save_as,const file_manager::DocumentPickerResult& result) {
    try {
        auto& picker=save_as?save_picker_:open_picker_;
        if(result.terminal==file_manager::DocumentPickerTerminal::overwrite_confirmation_required) {
            if(message("Replace existing file?",result.message,gf::HostMessageButtons::yes_no)==gf::HostDialogChoice::yes) picker.view->confirm_overwrite();
            return;
        }
        if(result.terminal==file_manager::DocumentPickerTerminal::cancelled) {hide_picker(save_as);after_save_={};return;}
        if(!result.accepted() || result.selections.size()!=1) {error(result.message);return;}
        const auto path=result.selections.front().path;
        hide_picker(save_as);
        if(save_as) {
            // Host rechecks after the chooser and owns the final overwrite
            // consent against this exact byte/identity snapshot.
            const auto expected=read_file(path);
            if(expected.exists && message("Confirm Save As","Replace "+path_utf8(path)+" with this document?",gf::HostMessageButtons::yes_no)!=gf::HostDialogChoice::yes) {after_save_={};return;}
            auto next=std::move(after_save_);after_save_={};
            if(save_to(path,expected) && next) next();
        } else open_file(path);
    } catch(const std::exception& e){hide_picker(save_as);after_save_={};error(e.what());}
}
}
