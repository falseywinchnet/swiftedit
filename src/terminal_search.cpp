#include "terminal_search.hpp"
#include <stdexcept>
namespace swiftedit {
void TerminalSearch::begin(TerminalBuffer &buffer, SearchPattern pattern, bool match_case) {
    if (buffer.session().read_only())
        throw std::runtime_error(
            "Search of large read-only files is not available in this terminal yet.");
    const TerminalSelection selection = buffer.selection();
    std::unique_ptr<PatternScan> prepared = std::make_unique<PatternScan>(
        buffer.session().text(), std::move(pattern), selection.caret, match_case);
    scan_ = std::move(prepared);
    stamp_ = buffer.session().stamp();
    selection_ = selection;
    start_ = selection.caret;
    wrapped_ = false;
    state_ = TerminalSearchState::pending;
}
TerminalSearchState TerminalSearch::step(TerminalBuffer &buffer, std::size_t budget) {
    if (state_ != TerminalSearchState::pending)
        return state_;
    const DocumentStamp current = buffer.session().stamp();
    if (current.identity != stamp_.identity || current.revision != stamp_.revision) {
        cancel();
        return state_;
    }
    const TerminalSelection selection = buffer.selection();
    if (selection.anchor != selection_.anchor || selection.caret != selection_.caret) {
        cancel();
        return state_;
    }
    const SearchProgress progress = (*scan_).step(budget);
    if (progress.match) {
        if (wrapped_ && (*progress.match).offset >= start_)
            state_ = TerminalSearchState::not_found;
        else {
            buffer.select_range(*progress.match, stamp_);
            state_ = TerminalSearchState::found;
        }
        scan_.reset();
    } else if (progress.complete) {
        if (!wrapped_ && start_) {
            (*scan_).restart();
            wrapped_ = true;
        } else {
            state_ = TerminalSearchState::not_found;
            scan_.reset();
        }
    }
    return state_;
}
void TerminalSearch::cancel() {
    scan_.reset();
    state_ = TerminalSearchState::cancelled;
}
void TerminalReplace::begin(TerminalBuffer &buffer, SearchPattern pattern, std::string replacement,
                            bool match_case) {
    if (buffer.session().read_only())
        throw std::runtime_error("Large-file pages are read-only.");
    const TerminalSelection selection = buffer.selection();
    std::unique_ptr<SessionReplacement> prepared =
        buffer.prepare_replacement(std::move(pattern), std::move(replacement), match_case);
    scan_ = std::move(prepared);
    stamp_ = buffer.session().stamp();
    selection_ = selection;
    count_ = 0;
    state_ = TerminalReplaceState::pending;
}
TerminalReplaceState TerminalReplace::step(TerminalBuffer &buffer, std::size_t budget) {
    if (state_ != TerminalReplaceState::pending)
        return state_;
    const DocumentStamp current = buffer.session().stamp();
    if (current.identity != stamp_.identity || current.revision != stamp_.revision) {
        cancel();
        return state_;
    }
    const TerminalSelection selection = buffer.selection();
    if (selection.anchor != selection_.anchor || selection.caret != selection_.caret) {
        cancel();
        return state_;
    }
    if (!(*scan_).step(budget))
        return state_;
    state_ = TerminalReplaceState::cancelled;
    count_ = buffer.commit_replacement(*scan_);
    scan_.reset();
    state_ = TerminalReplaceState::complete;
    return state_;
}
void TerminalReplace::cancel() {
    scan_.reset();
    state_ = TerminalReplaceState::cancelled;
    count_ = 0;
}
} // namespace swiftedit
