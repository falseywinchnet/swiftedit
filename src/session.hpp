#pragma once
#include "document.hpp"
#include <memory>
#include <vector>

namespace swiftedit {
constexpr std::size_t editable_limit = 16 * 1024 * 1024;
constexpr std::size_t maximum_page = 64 * 1024;
struct Page {
    std::uint64_t offset{}, next{}, size{};
    std::string bytes;
};
struct Preview {
    std::uint64_t token{}, revision{};
    std::size_t offset{}, length{};
    std::string before, removed, inserted, after;
};
// Holds a read handle, not a whole-file allocation. No write sharing is granted.
class PagedFile {
  public:
    explicit PagedFile(const std::filesystem::path &);
    ~PagedFile();
    PagedFile(const PagedFile &) = delete;
    PagedFile &operator=(const PagedFile &) = delete;
    Page page(std::uint64_t offset, std::size_t budget) const;
    std::uint64_t size() const { return size_; }

  private:
    void *handle_{};
    std::uint64_t size_{};
};
// Byte-faithful command model. Malformed input remains editable; publication
// requires valid UTF-8 or an explicitly requested sanitized text copy.
class Session {
  public:
    void open(const std::filesystem::path &);
    void reset();
    Page page(std::uint64_t offset = 0, std::size_t budget = 4096) const;
    std::vector<std::size_t> find(std::string_view query, std::size_t maximum = 100) const;
    std::vector<Preview> preview(std::string_view before, std::string_view old,
                                 std::string_view after, std::string_view replacement);
    void commit(std::uint64_t token, std::uint64_t revision);
    bool undo();
    bool redo();
    void restore_opened();
    void save();
    void save_as(const std::filesystem::path &);              // new target only
    void save_text_copy(const std::filesystem::path &) const; // new target only
    bool dirty() const { return !large_ && text_ != saved_; }
    bool read_only() const { return bool(large_); }
    std::uint64_t revision() const { return revision_; }
    std::uint64_t size() const { return large_ ? large_->size() : text_.size(); }
    const std::filesystem::path &path() const { return path_; }
    const std::string &text() const { return text_; }
    std::size_t illegal_bytes() const;

  private:
    void editable() const;
    void change(std::string);
    void published(const std::filesystem::path &, notepad::FileSnapshot);
    std::filesystem::path path_;
    std::unique_ptr<PagedFile> large_;
    notepad::FileSnapshot snapshot_;
    std::string text_, saved_, opened_;
    std::vector<std::string> undo_, redo_;
    std::vector<Preview> previews_;
    std::uint64_t revision_{1}, next_token_{1};
};
std::string text_copy(std::string_view, std::size_t *invalid = nullptr);
std::string normalize_newlines(std::string_view, std::string_view ending);
std::string suggested_name(std::string_view);
std::filesystem::path versioned_name(const std::filesystem::path &);
// Transport escapes every control byte; terminal escape sequences never execute.
std::string escape_field(std::string_view);
std::string unescape_field(std::string_view);
} // namespace swiftedit
