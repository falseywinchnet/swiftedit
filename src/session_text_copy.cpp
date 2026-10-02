#include "session_text_copy.hpp"
#include <stdexcept>
namespace swiftedit {
SessionTextCopy::SessionTextCopy(const Session &session, const std::filesystem::path &path)
    : stamp_(session.stamp()), size_(session.size()),
      writer_(std::make_unique<notepad::NewFileWriter>(std::filesystem::absolute(path))) {}
void SessionTextCopy::validate(const Session &session) const {
    const DocumentStamp current = session.stamp();
    if (current.identity != stamp_.identity || current.revision != stamp_.revision || session.size() != size_)
        throw std::runtime_error("Text copy source changed; copy cancelled.");
}
bool SessionTextCopy::step(const Session &session, const std::size_t budget) {
    if (!budget || budget > maximum_page)
        throw std::runtime_error("Text copy read budget must be 1..65536 bytes.");
    if (state_ != TextCopyState::copying)
        throw std::runtime_error("Text copy is not accepting more source data.");
    try {
        validate(session);
        const Page source = session.page(offset_, budget);
        const bool final = source.next == size_;
        const TextCopyChunk output = stream_.append(source.bytes, final);
        (*writer_).append(output.bytes);
        offset_ = source.next;
        invalid_ += output.invalid_bytes;
        if (final)
            state_ = TextCopyState::ready;
        return final;
    } catch (...) {
        state_ = TextCopyState::failed;
        writer_.reset();
        throw;
    }
}
void SessionTextCopy::publish(const Session &session) {
    if (state_ != TextCopyState::ready)
        throw std::runtime_error("Text copy is not ready to publish.");
    try {
        validate(session);
        // Paged sources also validate their retained file metadata at this read.
        static_cast<void>(session.page(size_, 1));
        (*writer_).publish();
        state_ = TextCopyState::published;
        writer_.reset();
    } catch (...) {
        state_ = TextCopyState::failed;
        writer_.reset();
        throw;
    }
}
} // namespace swiftedit
