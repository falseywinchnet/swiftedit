#pragma once
#include "session.hpp"
#include "text_copy_stream.hpp"
#include "new_file_writer.hpp"
namespace swiftedit {
enum class TextCopyState { copying, ready, published, failed };
// One bounded read/conversion/write per step. Publication is separate so callers
// can accept cancellation before installing the completed file. Source/session
// must stay current; destruction cancels an unpublished temporary.
class SessionTextCopy final {
public:
    SessionTextCopy(const Session &, const std::filesystem::path &);
    [[nodiscard]] bool step(const Session &, std::size_t budget = 65536);
    void publish(const Session &);
    [[nodiscard]] TextCopyState state() const { return state_; }
    [[nodiscard]] std::uint64_t offset() const { return offset_; }
    [[nodiscard]] std::uint64_t size() const { return size_; }
    [[nodiscard]] std::uint64_t invalid_bytes() const { return invalid_; }
private:
    void validate(const Session &) const;
    DocumentStamp stamp_{};
    std::uint64_t size_{}, offset_{}, invalid_{};
    TextCopyStream stream_{};
    std::unique_ptr<notepad::NewFileWriter> writer_{};
    TextCopyState state_{TextCopyState::copying};
};
} // namespace swiftedit
