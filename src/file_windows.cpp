#include "document.hpp"
#include <windows.h>
#include <atomic>
#include <stdexcept>
#include <vector>

namespace notepad {
namespace {
struct Handle {
    HANDLE value{INVALID_HANDLE_VALUE};
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if(value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
[[noreturn]] void fail(const char* action) {
    throw std::runtime_error(std::string(action) + " (Windows error " + std::to_string(GetLastError()) + "). The document remains in memory.");
}
std::uint64_t pair(DWORD high, DWORD low) { return (std::uint64_t(high)<<32)|low; }
FileSnapshot snapshot(HANDLE handle) {
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(handle,&info)) fail("Cannot inspect file");
    if(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))
        throw std::runtime_error("Links, reparse points and directories are not supported. Choose an ordinary file.");
    if(info.nNumberOfLinks != 1) throw std::runtime_error("Files with multiple hard links are not supported by this save policy.");
    if(pair(info.nFileSizeHigh,info.nFileSizeLow)>maximum_bytes) throw std::runtime_error("This build supports files up to 4 MiB.");
    FileSnapshot result{true,info.dwVolumeSerialNumber,pair(info.nFileIndexHigh,info.nFileIndexLow),pair(info.ftLastWriteTime.dwHighDateTime,info.ftLastWriteTime.dwLowDateTime),{}};
    LARGE_INTEGER zero{};
    if(!SetFilePointerEx(handle,zero,nullptr,FILE_BEGIN)) fail("Cannot seek file");
    result.bytes.resize(info.nFileSizeLow);
    DWORD read{};
    if(!result.bytes.empty() && (!ReadFile(handle,result.bytes.data(),DWORD(result.bytes.size()),&read,nullptr) || read!=result.bytes.size())) fail("Cannot read complete file");
    BY_HANDLE_FILE_INFORMATION after{};
    if(!GetFileInformationByHandle(handle,&after)) fail("Cannot revalidate file");
    if(info.nFileSizeLow!=after.nFileSizeLow || info.nFileSizeHigh!=after.nFileSizeHigh ||
       CompareFileTime(&info.ftLastWriteTime,&after.ftLastWriteTime)!=0) throw std::runtime_error("The file changed while it was being read. Try opening it again.");
    return result;
}
void validate_path(const std::filesystem::path& path) {
    if(path.empty() || !path.is_absolute()) throw std::runtime_error("An absolute file path is required.");
    // Alternate streams are outside the first editor's file contract.
    const auto text=path.native();
    if(text.find(L':',2)!=std::wstring::npos) throw std::runtime_error("Alternate data stream paths are unsupported.");
    for(auto parent=path.parent_path(); !parent.empty();) {
        auto attrs=GetFileAttributesW(parent.c_str());
        if(attrs==INVALID_FILE_ATTRIBUTES) fail("Cannot inspect parent directory");
        if(attrs&FILE_ATTRIBUTE_REPARSE_POINT) throw std::runtime_error("Paths through reparse-point directories are unsupported by this build.");
        auto next=parent.parent_path(); if(next==parent) break; parent=std::move(next);
    }
}
}
FileSnapshot read_file(const std::filesystem::path& path) {
    validate_path(path);
    Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(file.value==INVALID_HANDLE_VALUE) {
        if(GetLastError()==ERROR_FILE_NOT_FOUND) return {};
        fail("Cannot open file");
    }
    return snapshot(file.value);
}
FileSnapshot write_file(const std::filesystem::path& path,std::string_view bytes,const FileSnapshot& expected) {
    if(bytes.size()>maximum_bytes) throw std::runtime_error("Output exceeds 4 MiB.");
    validate_path(path);
    // Keep a no-write-sharing read handle alive through publication. Existing
    // incompatible writers cause refusal rather than a last-writer-wins save.
    Handle current(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    FileSnapshot actual;
    if(current.value==INVALID_HANDLE_VALUE) {
        if(GetLastError()!=ERROR_FILE_NOT_FOUND) fail("Cannot validate save destination");
    } else actual=snapshot(current.value);
    if(actual!=expected) throw std::runtime_error("The destination changed outside Notepad. Open it again or use Save As to another name. Nothing was overwritten.");
    if(actual.exists && (GetFileAttributesW(path.c_str())&FILE_ATTRIBUTE_READONLY)) throw std::runtime_error("The destination is read-only. Use Save As to another name.");
    static std::atomic<unsigned long> sequence{};
    std::filesystem::path temporary;
    HANDLE created=INVALID_HANDLE_VALUE;
    for(int attempt=0;attempt<100;++attempt) {
        temporary=path.parent_path()/(L".notepad-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++sequence)+L".tmp");
        created=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(created!=INVALID_HANDLE_VALUE) break;
        if(GetLastError()!=ERROR_FILE_EXISTS) fail("Cannot create sibling temporary file");
    }
    if(created==INVALID_HANDLE_VALUE) fail("Cannot reserve temporary filename");
    struct Cleanup { std::filesystem::path path; ~Cleanup(){DeleteFileW(path.c_str());} } cleanup{temporary};
    {
        Handle temp(created); DWORD written{};
        if(!WriteFile(temp.value,bytes.data(),DWORD(bytes.size()),&written,nullptr) || written!=bytes.size()) fail("Cannot write complete temporary file");
        if(!FlushFileBuffers(temp.value)) fail("Cannot flush temporary file");
    }
    // The visible name must still identify the exact original snapshot.
    if(read_file(path)!=expected) throw std::runtime_error("The destination changed before publication. Nothing was overwritten.");
    if(actual.exists) {
        // ReplaceFile preserves the original file's security and named streams.
        // A backup also keeps original bytes recoverable on unusual OS failure.
        auto backup=temporary; backup+=L".previous";
        if(GetFileAttributesW(backup.c_str())!=INVALID_FILE_ATTRIBUTES) throw std::runtime_error("A recovery filename already exists; save refused.");
        if(!ReplaceFileW(path.c_str(),temporary.c_str(),backup.c_str(),0,nullptr,nullptr)) {
            const DWORD error=GetLastError();
            throw std::runtime_error("Windows could not replace the file (error "+std::to_string(error)+"). Original bytes may be retained in "+backup.string()+". Keep this document open and use Save As.");
        }
        DeleteFileW(backup.c_str());
    } else if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)) fail("Cannot publish new file without overwriting");
    return read_file(path);
}
}
