#include "session.hpp"
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <windows.h>

namespace swiftedit {
namespace {
std::size_t sequence(std::string_view s, std::size_t i) {
    auto c = static_cast<unsigned char>(s[i]);
    if (c < 128)
        return 1;
    std::size_t n = c >= 0xc2 && c <= 0xdf   ? 2
                    : c >= 0xe0 && c <= 0xef ? 3
                    : c >= 0xf0 && c <= 0xf4 ? 4
                                             : 0;
    if (!n || n > s.size() - i)
        return 0;
    for (std::size_t j = 1; j < n; ++j)
        if ((static_cast<unsigned char>(s[i + j]) & 0xc0) != 0x80)
            return 0;
    auto second = static_cast<unsigned char>(s[i + 1]);
    if ((c == 0xe0 && second < 0xa0) || (c == 0xed && second >= 0xa0) ||
        (c == 0xf0 && second < 0x90) || (c == 0xf4 && second >= 0x90))
        return 0;
    return n;
}
void payload(std::string_view s) {
    if (s.find("\r\r") != s.npos)
        throw std::runtime_error(
            "Line markers are read-only metadata and must not be included in document text.");
    std::size_t bad{};
    text_copy(s, &bad);
    if (bad)
        throw std::runtime_error("Replacement must be valid UTF-8.");
}
void budget(std::size_t n) {
    if (!n || n > maximum_page)
        throw std::runtime_error("Page budget must be 1..65536 bytes.");
}
} // namespace
PagedFile::PagedFile(const std::filesystem::path &path) {
    auto h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                         OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot open read-only paged file.");
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(h, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
        CloseHandle(h);
        throw std::runtime_error("Paged source must be a regular file.");
    }
    handle_ = h;
    size_ = (std::uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
}
PagedFile::~PagedFile() {
    if (handle_)
        CloseHandle(handle_);
}
Page PagedFile::page(std::uint64_t offset, std::size_t n) const {
    budget(n);
    if (offset > size_)
        throw std::runtime_error("Page offset exceeds file size.");
    Page p{offset, offset, size_, {}};
    p.bytes.resize(static_cast<std::size_t>(std::min<std::uint64_t>(n, size_ - offset)));
    LARGE_INTEGER at{};
    at.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(handle_, at, nullptr, FILE_BEGIN))
        throw std::runtime_error("Cannot seek paged source.");
    DWORD got{};
    if (!ReadFile(handle_, p.bytes.data(), static_cast<DWORD>(p.bytes.size()), &got, nullptr) ||
        got != p.bytes.size())
        throw std::runtime_error("Paged read failed; no document changed.");
    p.next += got;
    return p;
}
std::string text_copy(std::string_view s, std::size_t *invalid) {
    std::string out;
    out.reserve(s.size());
    std::size_t bad{};
    for (std::size_t i = 0; i < s.size();) {
        auto n = sequence(s, i);
        if (n) {
            out.append(s.substr(i, n));
            i += n;
        } else {
            out += ' ';
            ++i;
            ++bad;
        }
    }
    if (invalid)
        *invalid = bad;
    return out;
}
void Session::open(const std::filesystem::path &source) {
    auto path = std::filesystem::absolute(source);
    auto paged = std::make_unique<PagedFile>(path);
    if (paged->size() >= editable_limit) {
        reset();
        path_ = std::move(path);
        large_ = std::move(paged);
        return;
    }
    paged.reset();
    auto snapshot = notepad::read_file(path);
    if (!snapshot.exists)
        throw std::runtime_error("File no longer exists.");
    if (snapshot.bytes.size() >= editable_limit)
        throw std::runtime_error("File grew; reopen it in read-only mode.");
    reset();
    path_ = std::move(path);
    snapshot_ = std::move(snapshot);
    text_ = saved_ = opened_ = snapshot_.bytes;
}
void Session::reset() {
    large_.reset();
    path_.clear();
    snapshot_ = {};
    text_.clear();
    saved_.clear();
    opened_.clear();
    undo_.clear();
    redo_.clear();
    previews_.clear();
    ++revision_;
}
void Session::editable() const {
    if (large_)
        throw std::runtime_error("Files at or above 16 MiB are read-only.");
}
Page Session::page(std::uint64_t offset, std::size_t n) const {
    budget(n);
    if (large_)
        return large_->page(offset, n);
    if (offset > text_.size())
        throw std::runtime_error("Page offset exceeds document size.");
    auto bytes = text_.substr(static_cast<std::size_t>(offset), n);
    return {offset, offset + bytes.size(), text_.size(), std::move(bytes)};
}
std::vector<std::size_t> Session::find(std::string_view query, std::size_t maximum) const {
    editable();
    if (query.empty() || !maximum || maximum > 1000)
        throw std::runtime_error("Use a nonempty query and a match limit of 1..1000.");
    std::vector<std::size_t> hits;
    for (std::size_t i = 0; (i = text_.find(query, i)) != text_.npos && hits.size() < maximum; ++i)
        hits.push_back(i);
    return hits;
}
std::vector<Preview> Session::preview(std::string_view before, std::string_view old,
                                      std::string_view after, std::string_view replacement) {
    editable();
    payload(replacement);
    if (before.empty() && old.empty() && after.empty() && !text_.empty())
        throw std::runtime_error("An edit needs exact source context.");
    if (replacement.size() >= editable_limit)
        throw std::runtime_error("Replacement exceeds editable limit.");
    std::string needle = std::string(before) + std::string(old) + std::string(after);
    std::vector<Preview> result;
    for (std::size_t i = 0;; ++i) {
        auto found = text_.find(needle, i);
        if (found == text_.npos)
            break;
        if (result.size() == 100 ||
            (result.size() + 1) * (old.size() + replacement.size() + 160) > 32 * 1024 * 1024)
            throw std::runtime_error("Preview is too broad; provide more exact context.");
        if (text_.size() - old.size() + replacement.size() >= editable_limit)
            throw std::runtime_error("Edit would reach the 16 MiB read-only threshold.");
        auto at = found + before.size();
        auto lo = at > 80 ? at - 80 : 0;
        result.push_back({next_token_++, revision_, at, old.size(), text_.substr(lo, at - lo),
                          std::string(old), std::string(replacement),
                          text_.substr(at + old.size(), 80)});
        if (needle.empty())
            break;
        i = found;
    }
    if (result.empty())
        throw std::runtime_error("Exact source context not found; no changes made.");
    previews_ = result;
    return result;
}
void Session::change(std::string next) {
    editable();
    if (next == text_)
        return;
    // Bound snapshot history to 32 MiB; never persist it to disk.
    std::size_t used = text_.size();
    for (auto &entry : undo_)
        used += entry.size();
    while (!undo_.empty() && (used > 32 * 1024 * 1024 || undo_.size() >= 256)) {
        used -= undo_.front().size();
        undo_.erase(undo_.begin());
    }
    undo_.push_back(text_);
    text_ = std::move(next);
    redo_.clear();
    previews_.clear();
    ++revision_;
}
void Session::commit(std::uint64_t token, std::uint64_t revision) {
    editable();
    if (revision != revision_)
        throw std::runtime_error("Stale preview: document revision changed.");
    auto p =
        std::find_if(previews_.begin(), previews_.end(), [&](auto &p) { return p.token == token; });
    if (p == previews_.end())
        throw std::runtime_error("Unknown or expired preview token.");
    auto next = text_;
    next.replace(p->offset, p->length, p->inserted);
    change(std::move(next));
    previews_.clear();
}
bool Session::undo() {
    editable();
    if (undo_.empty())
        return false;
    redo_.push_back(text_);
    text_ = std::move(undo_.back());
    undo_.pop_back();
    previews_.clear();
    ++revision_;
    return true;
}
bool Session::redo() {
    editable();
    if (redo_.empty())
        return false;
    undo_.push_back(text_);
    text_ = std::move(redo_.back());
    redo_.pop_back();
    previews_.clear();
    ++revision_;
    return true;
}
void Session::restore_opened() {
    editable();
    change(opened_);
}
std::size_t Session::illegal_bytes() const {
    editable();
    std::size_t n{};
    text_copy(text_, &n);
    return n;
}
void Session::published(const std::filesystem::path &path, notepad::FileSnapshot snapshot) {
    path_ = path;
    snapshot_ = std::move(snapshot);
    saved_ = text_;
    undo_.clear();
    redo_.clear();
    previews_.clear();
    ++revision_;
}
void Session::save() {
    editable();
    if (path_.empty())
        throw std::runtime_error("Untitled document requires save-as.");
    if (illegal_bytes())
        throw std::runtime_error("Illegal UTF-8 bytes remain. Edit them or use Save Text Copy.");
    auto written = notepad::write_file(path_, text_, snapshot_);
    published(path_, std::move(written));
}
void Session::save_as(const std::filesystem::path &target) {
    editable();
    if (illegal_bytes())
        throw std::runtime_error("Illegal UTF-8 bytes remain. Edit them or use Save Text Copy.");
    auto path = std::filesystem::absolute(target);
    auto written = notepad::write_file(path, text_, {});
    published(path, std::move(written));
}
void Session::save_text_copy(const std::filesystem::path &target) const {
    editable();
    notepad::write_file(std::filesystem::absolute(target), text_copy(text_), {});
}
std::string normalize_newlines(std::string_view s, std::string_view ending) {
    if (ending != "\n" && ending != "\r" && ending != "\r\n")
        throw std::runtime_error("Expected LF, CR or CRLF.");
    std::string result;
    for (std::size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\r' || s[i] == '\n') {
            if (s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n')
                ++i;
            result += ending;
        } else
            result += s[i];
    return result;
}
std::string suggested_name(std::string_view s) {
    auto first = s.substr(0, s.find_first_of("\r\n"));
    if (first.size() < 3 || first.front() != '[' || first.back() != ']')
        return "Untitled.txt";
    auto name = std::string(first.substr(1, first.size() - 2));
    if (name.size() > 200 || name.find_first_of("<>:\"/\\|?*") != name.npos || name.back() == '.' ||
        name.back() == ' ')
        return "Untitled.txt";
    for (unsigned char c : name)
        if (c < 32)
            return "Untitled.txt";
    std::string base = name.substr(0, name.find('.'));
    for (auto &c : base)
        if (c >= 'a' && c <= 'z')
            c -= 32;
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" ||
        (base.size() == 4 && (base.starts_with("COM") || base.starts_with("LPT")) &&
         base[3] >= '1' && base[3] <= '9'))
        return "Untitled.txt";
    return name;
}
std::filesystem::path versioned_name(const std::filesystem::path &path) {
    auto stem = path.stem().wstring(), ext = path.extension().wstring();
    auto dot = stem.rfind(L'.');
    std::uint64_t number = 1;
    if (dot != stem.npos && dot + 1 < stem.size()) {
        auto suffix = stem.substr(dot + 1);
        bool digits = std::all_of(suffix.begin(), suffix.end(),
                                  [](auto c) { return c >= L'0' && c <= L'9'; });
        if (digits) {
            try {
                auto old = std::stoull(suffix);
                if (old == UINT64_MAX)
                    throw std::runtime_error("Version number exhausted.");
                number = old + 1;
                stem.resize(dot);
            } catch (const std::out_of_range &) {
                throw std::runtime_error("Version number exhausted.");
            }
        }
    }
    return path.parent_path() / (stem + L'.' + std::to_wstring(number) + ext);
}
std::string escape_field(std::string_view s) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out;
    for (unsigned char c : s)
        if (c == '\\')
            out += "\\\\";
        else if (c < 32 || c >= 127) {
            out += "\\x";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else
            out += char(c);
    return out;
}
std::string unescape_field(std::string_view s) {
    std::string out;
    auto hex = [](char c) {
        return c >= '0' && c <= '9'   ? c - '0'
               : c >= 'a' && c <= 'f' ? c - 'a' + 10
               : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                      : -1;
    };
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\') {
            out += s[i];
            continue;
        }
        if (++i == s.size())
            throw std::runtime_error("Incomplete escape.");
        switch (s[i]) {
        case '\\':
            out += '\\';
            break;
        case 'n':
            out += '\n';
            break;
        case 'r':
            out += '\r';
            break;
        case 't':
            out += '\t';
            break;
        case 'x':
            if (i + 2 >= s.size() || hex(s[i + 1]) < 0 || hex(s[i + 2]) < 0)
                throw std::runtime_error("Invalid hex escape.");
            out += char(hex(s[i + 1]) * 16 + hex(s[i + 2]));
            i += 2;
            break;
        default:
            throw std::runtime_error("Unknown escape.");
        }
    }
    return out;
}
} // namespace swiftedit
