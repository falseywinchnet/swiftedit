#include "markdown_preparation.hpp"
#include <utility>

namespace swiftedit {
MarkdownPreparation::MarkdownPreparation() : worker_(run, this) {}
MarkdownPreparation::~MarkdownPreparation() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        cancellation_.request_stop();
        pending_.reset();
    }
    changed_.notify_one();
    if (worker_.joinable())
        worker_.join();
}
void MarkdownPreparation::request(std::string source) {
    std::stop_source next{};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancellation_.request_stop();
        cancellation_ = std::move(next);
        pending_ = Request{std::move(source), cancellation_.get_token()};
        completed_.reset();
        failure_ = {};
    }
    changed_.notify_one();
}
void MarkdownPreparation::cancel() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cancellation_.request_stop();
        pending_.reset();
        completed_.reset();
        failure_ = {};
    }
    changed_.notify_one();
}
std::optional<std::vector<MarkdownBlock>> MarkdownPreparation::take() {
    std::optional<std::vector<MarkdownBlock>> result{};
    std::exception_ptr failure{};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        result = std::move(completed_);
        completed_.reset();
        failure = std::exchange(failure_, {});
    }
    if (failure)
        std::rethrow_exception(failure);
    return result;
}
bool MarkdownPreparation::Ready::operator()() const {
    const bool ready = (*owner).stopping_ || (*owner).pending_.has_value();
    return ready;
}
void MarkdownPreparation::run(MarkdownPreparation *owner) noexcept {
    MarkdownPreparation &work = *owner;
    for (;;) {
        Request request{};
        {
            std::unique_lock<std::mutex> lock(work.mutex_);
            work.changed_.wait(lock, Ready{owner});
            if (work.stopping_)
                return;
            request = std::move(*work.pending_);
            work.pending_.reset();
        }
        try {
            std::vector<MarkdownBlock> blocks = parse_markdown(request.source, request.cancellation);
            std::lock_guard<std::mutex> lock(work.mutex_);
            if (!work.stopping_ && !request.cancellation.stop_requested())
                work.completed_ = std::move(blocks);
        } catch (const MarkdownCancelled &) {
            // A cancelled request has no completion or error to publish.
        } catch (...) {
            std::lock_guard<std::mutex> lock(work.mutex_);
            if (!work.stopping_ && !request.cancellation.stop_requested())
                work.failure_ = std::current_exception();
        }
    }
}
}
