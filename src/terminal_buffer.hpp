#pragma once
#include "session.hpp"
#include <gui_forms/text.hpp>
#include <optional>

namespace swiftedit {
enum class TerminalMotion { left, right, up, down, home, end, document_start, document_end };
struct TerminalSelection {
    std::size_t anchor{}, caret{};
};
// Terminal editing authority; all mutations go through the shared Session.
// Unicode navigation metadata is lazy and revision-bound, never file content.
class TerminalBuffer {
public:
    void open(const std::filesystem::path &);
    void reset(bool discard = false);
    const Session &session() const { return session_; }
    TerminalSelection selection();
    SourceRange selected_range();
    std::string selected_text();
    void move(TerminalMotion, bool extend = false, std::size_t rows = 1);
    void select_all();
    void select_range(SourceRange, DocumentStamp);
    void insert(std::string_view);
    void replace_document(std::string_view, DocumentStamp);
    void enter();
    void erase(bool backward);
    std::string cut();
    bool undo();
    bool redo();
    void save();
    void save_as(const std::filesystem::path &);
    void save_text_copy(const std::filesystem::path &) const;
    std::size_t line_index();
    SourceRange line_range(std::size_t);
    std::size_t line_count();
    SourceRange grapheme_range(std::size_t byte_offset);

private:
    void synchronize();
    std::size_t line_at(std::size_t) const;
    std::size_t snap(std::size_t) const;
    Session session_{};
    std::unique_ptr<gui_forms::TextStore> navigation_{};
    DocumentStamp navigation_stamp_{};
    TerminalSelection selection_{};
    std::optional<std::size_t> desired_column_{};
    std::string newline_{"\r\n"};
};
} // namespace swiftedit
