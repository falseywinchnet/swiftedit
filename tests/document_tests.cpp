#include "document.hpp"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace notepad;
void require(bool test,const char* label) { if(!test) throw std::runtime_error(label); }
template<class F> void refuses(F test) { bool threw=false; try { test(); } catch(const std::exception&) { threw=true; } require(threw,"Expected refusal"); }
void raw(const std::filesystem::path& path,std::string_view text) { std::ofstream out(path,std::ios::binary); out.write(text.data(),text.size()); }
int main() {
 try {
    for(auto e : {Encoding::utf8,Encoding::utf8_bom,Encoding::utf16_le,Encoding::utf16_be}) {
        for(const std::string text : {std::string(), std::string("alpha\r\nbeta\ngamma\rdelta\t"),std::string("A\xc3\xa9\xf0\x9f\x98\x80 e\xcc\x81")}) {
            auto bytes=encode(text,e); auto value=decode(bytes);
            require(value.text==text && value.encoding==e,"Encoding round trip");
            require(encode(value.text,value.encoding)==bytes,"Byte round trip");
        }
    }
    refuses([]{decode(std::string("\xc0\x80",2));});
    refuses([]{decode(std::string("\xff\xfe\0\xd8",4));});
    refuses([]{decode(std::string("\xff\xfe\0\0",4));});
    refuses([]{decode(std::string("x\0y",3));});
    refuses([]{decode(std::string(maximum_bytes+1,'a'));});
    require(newline_name("a\r\nb\nc\r")=="Mixed (preserved)","Mixed newline observation");
    require(preferred_newline("a\nb")=="\n","First newline");
    require(!find_literal("abc","",0,true),"Empty search");
    require(find_literal("AbA","ba",0,false)==1,"ASCII insensitive search");
    auto replaced=replace_all("aaaa","aa","b",true);
    require(replaced.text=="bb" && replaced.count==2,"Nonoverlapping replace all");
    require(replace_all("a","a","aa",true).text=="aa","No recursive replacement");
    const auto dir=std::filesystem::temp_directory_path()/("notepad-tests-"+std::to_string(GetCurrentProcessId()));
    require(std::filesystem::create_directory(dir),"Unique fixture directory");
    struct Cleanup { std::filesystem::path dir; ~Cleanup(){std::error_code ec; std::filesystem::remove_all(dir,ec);} } cleanup{dir};
    auto path=dir/L"test-\u00e9.txt";
    Document doc; doc.save(path,"one\r\n",{});
    require(read_file(path).bytes=="one\r\n","Create exact bytes");
    doc.save(path,"two\r\n",doc.snapshot);
    require(read_file(path).bytes=="two\r\n","Replace exact bytes");
    raw(path,"external");
    refuses([&]{doc.save(path,"lost",doc.snapshot);});
    require(read_file(path).bytes=="external","Conflict preserves external data");
    refuses([&]{write_file(path,"collision",{});});
    auto bad=dir/"bad.txt"; raw(bad,std::string("\xff",1));
    auto original=doc.saved_text; refuses([&]{doc.open(bad);});
    require(doc.saved_text==original,"Failed open retains document");
    auto observed=read_file(path); SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_READONLY);
    refuses([&]{write_file(path,"readonly",observed);});
    SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_NORMAL);
    auto hard=dir/"hard.txt"; require(CreateHardLinkW(hard.c_str(),path.c_str(),nullptr),"Create hardlink fixture");
    refuses([&]{read_file(path);}); std::filesystem::remove(hard);
    std::filesystem::remove(path);
    refuses([&]{write_file(path,"deleted",observed);});
    require(!std::filesystem::exists(path),"Deleted source not resurrected");
    auto utf16=dir/"utf16.txt";
    raw(utf16,encode("alpha\r\nbeta\n",Encoding::utf16_be));
    doc.open(utf16);doc.save(utf16,doc.saved_text+"tail",doc.snapshot);
    require(read_file(utf16).bytes==encode("alpha\r\nbeta\ntail",Encoding::utf16_be),"UTF16 save preserves format");
    auto locked=read_file(utf16);
    HANDLE writer=CreateFileW(utf16.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    require(writer!=INVALID_HANDLE_VALUE,"External writer fixture");
    refuses([&]{write_file(utf16,"contended",locked);});
    CloseHandle(writer);
    require(read_file(utf16)==locked,"Open writer refusal preserves original");
    for(auto& entry:std::filesystem::directory_iterator(dir)) require(!entry.path().filename().wstring().starts_with(L".notepad-"),"Successful saves clean temporary artifacts");
    std::cout<<"Document tests passed: Unicode, exact bytes, search, safe publication, conflicts, malformed input, read-only and hard links.\n";
    return 0;
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
