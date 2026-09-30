#include "document.hpp"
#include <gui_forms/text.hpp>
#include <algorithm>
#include <stdexcept>

namespace notepad {
namespace {
void check_text(std::string_view text) {
    if (text.size() > maximum_bytes) throw std::runtime_error("This build supports text up to 4 MiB.");
    const auto valid = gui_forms::validate_utf8(text);
    if (!valid.valid()) throw std::runtime_error("Malformed UTF-8 at byte " + std::to_string(valid.error_offset.value()) + ". No text was replaced. Legacy encodings require explicit conversion in another tool.");
    for (unsigned char ch : text) {
        if (ch < 32 && ch != '\r' && ch != '\n' && ch != '\t')
            throw std::runtime_error("Binary-looking control characters are unsupported. The original file has not been changed.");
    }
}
void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) out += char(cp);
    else if (cp < 0x800) { out += char(0xc0 | (cp >> 6)); out += char(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { out += char(0xe0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
    else { out += char(0xf0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 63)); out += char(0x80 | ((cp >> 6) & 63)); out += char(0x80 | (cp & 63)); }
}
unsigned char fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
}
Decoded decode(std::string_view bytes) {
    if (bytes.size() > maximum_bytes) throw std::runtime_error("This build opens files up to 4 MiB.");
    Decoded d;
    if (bytes.starts_with(std::string_view("\xff\xfe\0\0", 4)) || bytes.starts_with(std::string_view("\0\0\xfe\xff", 4)))
        throw std::runtime_error("UTF-32 is not supported by this build. No file was changed.");
    if (bytes.starts_with("\xef\xbb\xbf")) { d.encoding = Encoding::utf8_bom; d.text = bytes.substr(3); }
    else if (bytes.starts_with("\xff\xfe") || bytes.starts_with("\xfe\xff")) {
        const bool little = bytes.starts_with("\xff\xfe");
        d.encoding = little ? Encoding::utf16_le : Encoding::utf16_be;
        if (bytes.size() % 2) throw std::runtime_error("Malformed UTF-16: odd byte count.");
        auto unit = [&](std::size_t i) -> std::uint32_t {
            unsigned a = static_cast<unsigned char>(bytes[i]), b = static_cast<unsigned char>(bytes[i+1]);
            return little ? a | b << 8 : a << 8 | b;
        };
        for (std::size_t i = 2; i < bytes.size(); i += 2) {
            auto cp = unit(i);
            if (cp >= 0xd800 && cp <= 0xdbff) {
                if (i + 3 >= bytes.size()) throw std::runtime_error("Malformed UTF-16: missing low surrogate.");
                auto lo = unit(i + 2);
                if (lo < 0xdc00 || lo > 0xdfff) throw std::runtime_error("Malformed UTF-16: invalid low surrogate.");
                cp = 0x10000 + ((cp - 0xd800) << 10) + lo - 0xdc00; i += 2;
            } else if (cp >= 0xdc00 && cp <= 0xdfff) throw std::runtime_error("Malformed UTF-16: isolated low surrogate.");
            append_utf8(d.text, cp);
        }
    } else d.text = bytes;
    check_text(d.text);
    return d;
}
std::string encode(std::string_view text, Encoding encoding) {
    check_text(text);
    if (encoding == Encoding::utf8) return std::string(text);
    if (encoding == Encoding::utf8_bom) {
        if(text.size()+3>maximum_bytes) throw std::runtime_error("Encoded output exceeds the 4 MiB file limit.");
        return "\xef\xbb\xbf" + std::string(text);
    }
    std::string out = encoding == Encoding::utf16_le ? "\xff\xfe" : "\xfe\xff";
    auto emit = [&](unsigned u) {
        if (encoding == Encoding::utf16_le) { out += char(u & 255); out += char(u >> 8); }
        else { out += char(u >> 8); out += char(u & 255); }
    };
    for (std::size_t i = 0; i < text.size();) {
        auto first = static_cast<unsigned char>(text[i++]);
        unsigned cp = first; int more = 0;
        if (first >= 0xf0) { cp = first & 7; more = 3; }
        else if (first >= 0xe0) { cp = first & 15; more = 2; }
        else if (first >= 0xc0) { cp = first & 31; more = 1; }
        while (more--) cp = (cp << 6) | (static_cast<unsigned char>(text[i++]) & 63);
        if (cp < 0x10000) emit(cp);
        else { cp -= 0x10000; emit(0xd800 + (cp >> 10)); emit(0xdc00 + (cp & 1023)); }
    }
    if (out.size() > maximum_bytes) throw std::runtime_error("Encoded output exceeds the 4 MiB file limit.");
    return out;
}
std::string encoding_name(Encoding e) {
    switch (e) { case Encoding::utf8: return "UTF-8"; case Encoding::utf8_bom: return "UTF-8 BOM"; case Encoding::utf16_le: return "UTF-16 LE BOM"; case Encoding::utf16_be: return "UTF-16 BE BOM"; }
    return "Unknown";
}
std::string newline_name(std::string_view s) {
    bool lf = false, cr = false, crlf = false;
    for (std::size_t i=0;i<s.size();++i) {
        if(s[i]=='\r') { if(i+1<s.size() && s[i+1]=='\n') { crlf=true; ++i; } else cr=true; }
        else if(s[i]=='\n') lf=true;
    }
    if(int(lf)+int(cr)+int(crlf)>1) return "Mixed (preserved)";
    return crlf ? "CRLF" : lf ? "LF" : cr ? "CR" : "No line endings";
}
std::string preferred_newline(std::string_view s) {
    const auto i = s.find_first_of("\r\n");
    if (i == std::string_view::npos) return "\r\n";
    return s[i] == '\n' ? "\n" : i+1<s.size() && s[i+1]=='\n' ? "\r\n" : "\r";
}
std::optional<std::size_t> find_literal(std::string_view s, std::string_view q, std::size_t start, bool match_case) {
    if(q.empty() || start>s.size()) return {};
    auto it=std::search(s.begin()+start,s.end(),q.begin(),q.end(),[&](unsigned char a,unsigned char b){return match_case ? a==b : fold(a)==fold(b);});
    if(it==s.end()) return {};
    return std::size_t(it-s.begin());
}
Replacement replace_all(std::string_view s, std::string_view q, std::string_view r, bool match_case) {
    Replacement result; std::size_t cursor=0;
    while(auto found=find_literal(s,q,cursor,match_case)) {
        if(result.text.size()+*found-cursor+r.size()>maximum_bytes) throw std::runtime_error("Replacement exceeds the 4 MiB text limit.");
        result.text.append(s.substr(cursor,*found-cursor)); result.text+=r; ++result.count; cursor=*found+q.size();
    }
    result.text.append(s.substr(cursor)); check_text(result.text); return result;
}
void Document::open(const std::filesystem::path& source) {
    auto observed=read_file(source);
    if(!observed.exists) throw std::runtime_error("The selected file no longer exists.");
    auto decoded=decode(observed.bytes);
    path=source; saved_text=std::move(decoded.text); encoding=decoded.encoding; snapshot=std::move(observed);
}
void Document::save(const std::filesystem::path& target,std::string_view text,const FileSnapshot& expected) {
    auto bytes=encode(text,encoding);
    auto written=write_file(target,bytes,expected);
    path=target; saved_text=text; snapshot=std::move(written);
}
}
