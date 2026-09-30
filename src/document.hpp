#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <cstdint>

namespace notepad {
constexpr std::size_t maximum_bytes = 4 * 1024 * 1024;
enum class Encoding { utf8, utf8_bom, utf16_le, utf16_be };
struct Decoded {
    std::string text;
    Encoding encoding{Encoding::utf8};
};
Decoded decode(std::string_view bytes);
std::string encode(std::string_view text, Encoding encoding);
std::string encoding_name(Encoding encoding);
std::string newline_name(std::string_view text);
std::string preferred_newline(std::string_view text);
std::optional<std::size_t> find_literal(std::string_view text, std::string_view query,
                                      std::size_t start, bool match_case);
struct Replacement { std::string text; std::size_t count{}; };
Replacement replace_all(std::string_view text, std::string_view query,
                        std::string_view replacement, bool match_case);
struct FileSnapshot {
    bool exists{};
    std::uint64_t volume{}, identity{}, modified{};
    std::string bytes;
    bool operator==(const FileSnapshot&) const = default;
};
FileSnapshot read_file(const std::filesystem::path& path);
// Expected absence is distinct from an observed existing file. Never truncates.
FileSnapshot write_file(const std::filesystem::path& path, std::string_view bytes,
                        const FileSnapshot& expected);
struct Document {
    std::filesystem::path path;
    std::string saved_text;
    Encoding encoding{Encoding::utf8};
    FileSnapshot snapshot;
    void open(const std::filesystem::path& source);
    void save(const std::filesystem::path& target, std::string_view text,
              const FileSnapshot& expected);
    bool dirty(std::string_view text) const { return text != saved_text; }
};
}
