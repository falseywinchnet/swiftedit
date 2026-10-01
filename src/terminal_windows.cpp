#include "terminal_row.hpp"
#include "terminal_wrap_view.hpp"
#include "terminal_page.hpp"
#include "terminal_search.hpp"
#include "terminal_query.hpp"
#include "date_time.hpp"
#include "session_word_count.hpp"
#include "session_copy.hpp"
#include <algorithm>
#include <iostream>
#ifdef SWIFTEDIT_TERMINAL_SMOKE
#include <fstream>
#endif
#include <stdexcept>
#include <windows.h>

namespace {
std::wstring utf16(std::string_view source) {
    if (source.empty())
        return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source.data(),
                                           static_cast<int>(source.size()), nullptr, 0);
    if (!length)
        throw std::runtime_error("Terminal output is not valid UTF-8.");
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, source.data(),
                             static_cast<int>(source.size()), result.data(), length))
        throw std::runtime_error("Cannot encode terminal output.");
    return result;
}
std::string utf8(std::wstring_view source) {
    const int length =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, source.data(),
                            static_cast<int>(source.size()), nullptr, 0, nullptr, nullptr);
    if (!length)
        throw std::runtime_error("Incomplete Unicode keyboard input.");
    std::string result(static_cast<std::size_t>(length), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, source.data(),
                             static_cast<int>(source.size()), result.data(), length, nullptr,
                             nullptr))
        throw std::runtime_error("Cannot decode keyboard input.");
    return result;
}
class Console final : public swiftedit::TerminalWrapControl {
public:
    Console() = default;
    Console(const Console &) = delete;
    Console &operator=(const Console &) = delete;
    ~Console() { close(); }
    void start() {
        input_ = GetStdHandle(STD_INPUT_HANDLE);
        original_ = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD output_mode = 0;
        if (!GetConsoleMode(input_, &input_mode_) || !GetConsoleMode(original_, &output_mode))
            throw std::runtime_error("Run swiftedit-terminal in an interactive Windows console.");
        screen_ = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                            CONSOLE_TEXTMODE_BUFFER, nullptr);
        if (screen_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create terminal screen buffer.");
        try {
            if (!SetConsoleMode(screen_,
                                ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING) ||
                !SetConsoleMode(input_, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT))
                throw std::runtime_error("Cannot configure interactive terminal mode.");
            input_changed_ = true;
            if (!SetConsoleActiveScreenBuffer(screen_))
                throw std::runtime_error("Cannot activate terminal screen.");
            active_ = true;
        } catch (...) {
            close();
            throw;
        }
    }
    void close() noexcept {
        if (active_)
            SetConsoleActiveScreenBuffer(original_);
        if (input_changed_)
            SetConsoleMode(input_, input_mode_);
        if (screen_ != INVALID_HANDLE_VALUE)
            CloseHandle(screen_);
        screen_ = INVALID_HANDLE_VALUE;
        input_changed_ = false;
        active_ = false;
    }
    COORD size() const {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(screen_, &info))
            throw std::runtime_error("Cannot read console dimensions.");
        const COORD result{static_cast<SHORT>(info.srWindow.Right - info.srWindow.Left + 1),
                           static_cast<SHORT>(info.srWindow.Bottom - info.srWindow.Top + 1)};
        return result;
    }
    void write(std::string_view text) const {
        const std::wstring encoded = utf16(text);
        std::size_t offset = 0;
        while (offset < encoded.size()) {
            const DWORD count =
                static_cast<DWORD>(std::min<std::size_t>(encoded.size() - offset, 16384));
            DWORD written = 0;
            if (!WriteConsoleW(screen_, encoded.data() + offset, count, &written, nullptr) ||
                !written)
                throw std::runtime_error("Cannot draw terminal screen.");
            offset += written;
        }
    }
    bool input_ready() const {
        const DWORD result = WaitForSingleObject(input_, 0);
        if (result == WAIT_FAILED)
            throw std::runtime_error("Cannot poll console input.");
        const bool ready = result == WAIT_OBJECT_0;
        return ready;
    }
    bool cancel_requested() override {
        if (!wrap_cancel_enabled_)
            return false;
        // Consume only ignored key releases and an Escape at the queue head.
        // Preserve all other input ordering, including resize and typed text.
        for (std::size_t index = 0; index < 32; ++index) {
            INPUT_RECORD event{};
            DWORD count = 0;
            if (!PeekConsoleInputW(input_, &event, 1, &count))
                throw std::runtime_error("Cannot inspect terminal cancellation input.");
            if (!count || event.EventType != KEY_EVENT)
                return false;
            if (event.Event.KeyEvent.bKeyDown) {
                if (event.Event.KeyEvent.wVirtualKeyCode != VK_ESCAPE)
                    return false;
                static_cast<void>(read());
                return true;
            }
            static_cast<void>(read());
        }
        return false;
    }
    void allow_wrap_cancel(bool enabled) { wrap_cancel_enabled_ = enabled; }
    INPUT_RECORD read() const {
        INPUT_RECORD input{};
        DWORD count = 0;
        if (!ReadConsoleInputW(input_, &input, 1, &count) || count != 1)
            throw std::runtime_error("Cannot read terminal input.");
        return input;
    }

private:
    HANDLE input_{INVALID_HANDLE_VALUE}, original_{INVALID_HANDLE_VALUE},
        screen_{INVALID_HANDLE_VALUE};
    DWORD input_mode_{};
    bool input_changed_{}, active_{}, wrap_cancel_enabled_{true};
};
void position(std::string &output, std::size_t row, std::size_t column) {
    output += "\x1b[" + std::to_string(row + 1) + ";" + std::to_string(column + 1) + "H";
}
void status_line(std::string &output, std::size_t row, std::string_view source, std::size_t width) {
    position(output, row, 0);
    const std::string safe = swiftedit::escape_field(source);
    output.append(safe, 0, std::min(width, safe.size()));
}
enum class Prompt { none, save_path, text_copy_path, open_path, exit_choice, find, replacement };
class Terminal {
public:
#ifdef SWIFTEDIT_TERMINAL_SMOKE
    std::size_t search_hits() const { return search_hits_; }
    std::size_t replacement_count() const { return replacement_.count(); }
    std::size_t viewport_width() const { return width_; }
    bool wraps() const { return wrap_; }
    std::size_t wrap_cancellations() const { return wrap_cancellations_; }
    std::size_t text_copy_saves() const { return text_copy_saves_; }
    std::size_t completed_counts() const { return completed_counts_; }
    std::uint64_t last_word_count() const { return last_word_count_; }
    std::size_t cancelled_counts() const { return cancelled_counts_; }
    std::size_t completed_copies() const { return completed_copies_; }
    std::size_t cancelled_copies() const { return cancelled_copies_; }
    std::string_view clipboard_bytes() const { return clipboard_.bytes(); }
    bool document_dirty() const { return buffer_.session().dirty(); }
    bool wildcard_enabled() const {
        const swiftedit::SearchPattern pattern = query_.pattern();
        return pattern.slots()[0].wildcard;
    }
#endif
    void open(const std::filesystem::path &path) {
        search_.cancel();
        replacement_.cancel();
        counting_.reset();
        copying_.reset();
        page_anchor_ = 0;
        page_caret_ = 0;
        buffer_.open(path);
        pager_.reset(buffer_.session());
        top_ = 0;
        left_ = 0;
        status_ = "Opened";
    }
    int run(const std::filesystem::path &path) {
        if (!path.empty())
            open(path);
        console_.start();
        bool redraw = true;
        while (!done_) {
            if (redraw) {
                try {
                    draw();
                } catch (const swiftedit::TerminalWrapInterrupt &cancelled) {
#ifdef SWIFTEDIT_TERMINAL_SMOKE
                    ++wrap_cancellations_;
#endif
                    wrap_suspended_ = true;
                    status_ = cancelled.what();
                    draw();
                } catch (const std::exception &failure) {
                    status_ = failure.what();
                    std::string fallback = "\x1b[?25l\x1b[0m\x1b[2J";
                    status_line(fallback, 0, status_, width_);
                    status_line(fallback, 1,
                                "Display unavailable. Ctrl+S Save, Ctrl+X Exit; edits retained.",
                                width_);
                    console_.write(fallback);
                }
                redraw = false;
            }
            try {
                if (copying_) {
                    if ((*copying_).state() == swiftedit::CopyState::running)
                        (*copying_).step(buffer_.session(), swiftedit::maximum_page);
                    if ((*copying_).state() == swiftedit::CopyState::complete) {
                        clipboard_ = (*copying_).take();
#ifdef SWIFTEDIT_TERMINAL_SMOKE
                        ++completed_copies_;
#endif
                        status_ = "Copied " + std::to_string(clipboard_.bytes().size()) + " bytes";
                        copying_.reset();
                        redraw = true;
                        continue;
                    }
                    if (!console_.input_ready())
                        continue;
                }
                if (counting_) {
                    if ((*counting_).state() == swiftedit::WordCountState::running)
                        (*counting_).step(buffer_.session(), swiftedit::maximum_page);
                    if ((*counting_).state() == swiftedit::WordCountState::complete) {
                        const std::uint64_t words = (*counting_).result();
#ifdef SWIFTEDIT_TERMINAL_SMOKE
                        ++completed_counts_;
                        last_word_count_ = words;
#endif
                        status_ = "Document: " + std::to_string(words) + " words";
                        counting_.reset();
                        redraw = true;
                        continue;
                    }
                    if (!console_.input_ready())
                        continue;
                }
                if (replacement_.state() == swiftedit::TerminalReplaceState::pending) {
                    const swiftedit::TerminalReplaceState state = replacement_.step(buffer_);
                    if (state != swiftedit::TerminalReplaceState::pending) {
                        status_ = state == swiftedit::TerminalReplaceState::complete
                                      ? "Replaced " + std::to_string(replacement_.count()) +
                                            " matches; Ctrl+Z undoes"
                                      : "Replacement cancelled";
                        redraw = true;
                        continue;
                    }
                    if (!console_.input_ready())
                        continue;
                }
                if (search_.state() == swiftedit::TerminalSearchState::pending) {
                    const swiftedit::TerminalSearchState state = search_.step(buffer_);
                    if (state != swiftedit::TerminalSearchState::pending) {
#ifdef SWIFTEDIT_TERMINAL_SMOKE
                        if (state == swiftedit::TerminalSearchState::found)
                            ++search_hits_;
#endif
                        status_ = state == swiftedit::TerminalSearchState::found ? "Found"
                                  : state == swiftedit::TerminalSearchState::not_found
                                      ? "Not found"
                                      : "Search cancelled";
                        redraw = true;
                        continue;
                    }
                    if (!console_.input_ready())
                        continue;
                }
            } catch (const std::exception &failure) {
                search_.cancel();
                replacement_.cancel();
                counting_.reset();
                copying_.reset();
                status_ = failure.what();
                redraw = true;
                continue;
            }
            const INPUT_RECORD input = console_.read();
            if (input.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                redraw = true;
                continue;
            }
            if (input.EventType != KEY_EVENT || !input.Event.KeyEvent.bKeyDown)
                continue;
            redraw = true;
            const KEY_EVENT_RECORD &event = input.Event.KeyEvent;
            const WORD repeats = std::min<WORD>(event.wRepeatCount, 1000);
            for (WORD i = 0; i < repeats && !done_; ++i) {
                try {
                    key(event);
                } catch (const swiftedit::TerminalWrapInterrupt &cancelled) {
#ifdef SWIFTEDIT_TERMINAL_SMOKE
                    ++wrap_cancellations_;
#endif
                    wrap_suspended_ = true;
                    status_ = cancelled.what();
                } catch (const std::exception &failure) {
                    status_ = failure.what();
                }
            }
        }
        return 0;
    }

private:
    void draw() {
        console_.allow_wrap_cancel(prompt_ == Prompt::none);
        const COORD size = console_.size();
        width_ = std::min<std::size_t>(static_cast<std::size_t>(std::max<SHORT>(size.X, 1)), 1000);
        const std::size_t height =
            std::min<std::size_t>(static_cast<std::size_t>(std::max<SHORT>(size.Y, 1)), 300);
        rows_ = height > 4 ? height - 4 : 1;
        std::string screen = "\x1b[?25l\x1b[0m\x1b[2J";
        const std::u8string path = buffer_.session().path().u8string();
        std::string title = "SwiftEdit  ";
        title.append(reinterpret_cast<const char *>(path.data()), path.size());
        if (path.empty())
            title += "Untitled";
        if (buffer_.session().dirty())
            title += " *";
        status_line(screen, 0, title, width_);
        if (height < 5 || width_ < 20) {
            status_line(screen, height - 1, "Enlarge terminal; Ctrl+X exits", width_);
            console_.write(screen);
            return;
        }
        std::optional<std::size_t> cursor_column{};
        std::size_t cursor_row = 1;
        if (buffer_.session().read_only()) {
            const swiftedit::TerminalPageFrame &page =
                pager_.frame(buffer_.session(), width_, rows_);
            for (const swiftedit::TerminalPageRun &run : page.runs) {
                position(screen, run.row + 1, run.column);
                const std::uint64_t first = std::min(page_anchor_, page_caret_);
                const std::uint64_t last = std::max(page_anchor_, page_caret_);
                const bool selected = run.source_offset < last &&
                    run.source_offset + run.source_length > first;
                if (selected)
                    screen += "\x1b[7m";
                screen += run.text;
                if (selected)
                    screen += "\x1b[0m";
            }
        } else if (wrap_ && wrap_suspended_) {
            // Leave the interrupted view unpublished until the next command.
            // Source, save/exit commands and the input loop remain live.
        } else if (wrap_) {
            const std::vector<swiftedit::TerminalWrappedRow> &wrapped =
                wrap_view_.frame(buffer_, width_, rows_);
            for (std::size_t row = 0; row < wrapped.size(); ++row) {
                const swiftedit::TerminalRow &visible = wrapped[row].display;
                for (const swiftedit::TerminalRun &run : visible.runs) {
                    position(screen, row + 1, run.column);
                    screen += run.selected ? "\x1b[7m" : "\x1b[0m";
                    screen += run.text;
                }
                if (visible.caret_column) {
                    cursor_column = visible.caret_column;
                    cursor_row = row + 1;
                }
            }
        } else {
            const std::size_t caret_line = buffer_.line_index();
            if (caret_line < top_)
                top_ = caret_line;
            if (caret_line >= top_ + rows_)
                top_ = caret_line - rows_ + 1;
            swiftedit::TerminalRow caret = row_cache_.row(buffer_, caret_line, left_, width_);
            if (!caret.caret_column) {
                // Reveal by source range, not byte-as-cell assumptions.
                const swiftedit::TerminalSelection selection = buffer_.selection();
                const std::size_t column =
                    row_cache_.source_column(buffer_, caret_line, selection.caret);
                left_ = column < left_ ? column : column >= width_ ? column - width_ + 1 : 0;
            }
            for (std::size_t row = 0; row < rows_ && top_ + row < buffer_.line_count(); ++row) {
                const swiftedit::TerminalRow visible =
                    row_cache_.row(buffer_, top_ + row, left_, width_);
                for (const swiftedit::TerminalRun &run : visible.runs) {
                    position(screen, row + 1, run.column);
                    screen += run.selected ? "\x1b[7m" : "\x1b[0m";
                    screen += run.text;
                }
                if (visible.caret_column) {
                    cursor_column = visible.caret_column;
                    cursor_row = row + 1;
                }
            }
        }
        screen += "\x1b[0m";
        std::string message = status_;
        if (prompt_ == Prompt::save_path)
            message = "Save new file: " + input_ + "  [Enter / Esc]";
        if (prompt_ == Prompt::text_copy_path)
            message = "Save Text Copy: " + input_ + "  [Enter / Esc]";
        if (prompt_ == Prompt::open_path)
            message = "Open file: " + input_ + "  [Enter / Esc]";
        if (prompt_ == Prompt::find)
            message = "Find: " + query_.display() + "  [Ctrl+? wildcard, Enter / Esc]";
        if (prompt_ == Prompt::replacement)
            message = "Replace ALL with: " + input_ + "  [Enter applies / Esc cancels]";
        if (prompt_ == Prompt::exit_choice)
            message = "Save changes before exit? Y=Save N=Discard Esc=Cancel";
        status_line(screen, height - 3, message, width_);
        if (prompt_ != Prompt::none && !status_.empty())
            status_line(screen, height - 2, status_, width_);
        else
            status_line(
                screen, height - 2,
                "^S Save  ^W Find  F3 Next  ^H Replace All  ^R Open  ^X Exit  ^Z Undo  ^Y Redo",
                width_);
        if (buffer_.session().read_only())
            status_line(screen, height - 1,
                        "Read-only: Shift+Up/Down/Pg Select  ^A All  ^C Copy  F6 Words",
                        width_);
        else
            status_line(
                screen, height - 1,
                "F6 Words  ^C Copy  ^K Cut  ^U Paste  ^T Text Copy  F2 Wrap  Shift+arrows Select",
                width_);
        if (prompt_ == Prompt::none && cursor_column) {
            position(screen, cursor_row, *cursor_column);
            screen += "\x1b[?25h";
        }
        console_.write(screen);
    }
    void save() {
        if (buffer_.session().path().empty()) {
            prompt_ = Prompt::save_path;
            status_.clear();
            input_ = swiftedit::suggested_name(buffer_.session().text());
            return;
        }
        buffer_.save();
        status_ = "Saved";
        if (exit_after_save_)
            done_ = true;
    }
    void text_copy() {
        if (buffer_.session().read_only())
            throw std::runtime_error("Save Text Copy is not yet available for read-only pages.");
        const std::filesystem::path source =
            buffer_.session().path().empty()
                ? std::filesystem::path(utf16(swiftedit::suggested_name(buffer_.session().text())))
                : buffer_.session().path();
        const std::u8string name = swiftedit::versioned_name(source).u8string();
        std::string prepared(reinterpret_cast<const char *>(name.data()), name.size());
        input_ = std::move(prepared);
        exit_after_save_ = false;
        prompt_ = Prompt::text_copy_path;
        status_ = "Each illegal byte becomes a space. Source unchanged. New file only.";
    }
    void character(wchar_t value) {
        if (!value)
            return;
        if (value >= 0xd800 && value <= 0xdbff) {
            high_surrogate_ = value;
            return;
        }
        std::wstring units{};
        if (high_surrogate_) {
            units += high_surrogate_;
            high_surrogate_ = 0;
        }
        units += value;
        const std::string text = utf8(units);
        if (prompt_ == Prompt::find) {
            query_.insert(text);
        } else if (prompt_ == Prompt::save_path || prompt_ == Prompt::text_copy_path ||
                   prompt_ == Prompt::open_path || prompt_ == Prompt::replacement) {
            const std::size_t limit = 32768;
            if (input_.size() + text.size() > limit)
                throw std::runtime_error("Prompt input exceeds its UTF-8 byte limit.");
            input_ += text;
        } else
            buffer_.insert(text);
    }
    void move(swiftedit::TerminalMotion motion, bool extend, std::size_t count = 1) {
        if (wrap_)
            wrap_view_.move(buffer_, motion, extend, count, width_);
        else
            buffer_.move(motion, extend, count);
    }
    void key(const KEY_EVENT_RECORD &event) {
        const bool ctrl = (event.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        const bool alt = (event.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        const bool shift = (event.dwControlKeyState & SHIFT_PRESSED) != 0;
        const WORD key = event.wVirtualKeyCode;
        const bool preserving_command =
            ctrl && !alt && (key == 'X' || key == 'S' || key == 'R' || key == 'T');
        if (key != VK_ESCAPE && prompt_ == Prompt::none && !preserving_command)
            wrap_suspended_ = false;
        if (key != 0 && event.uChar.UnicodeChar < 32)
            high_surrogate_ = 0;
        if (copying_) {
            (*copying_).cancel();
            copying_.reset();
#ifdef SWIFTEDIT_TERMINAL_SMOKE
            ++cancelled_copies_;
#endif
            status_ = "Copy cancelled; previous clipboard retained";
            if (key == VK_ESCAPE)
                return;
        }
        if (counting_) {
            (*counting_).cancel();
            counting_.reset();
#ifdef SWIFTEDIT_TERMINAL_SMOKE
            ++cancelled_counts_;
#endif
            status_ = "Word count cancelled";
            if (key == VK_ESCAPE)
                return;
        }
        if (replacement_.state() == swiftedit::TerminalReplaceState::pending) {
            replacement_.cancel();
            status_ = "Replacement cancelled";
            if (key == VK_ESCAPE)
                return;
        }
        if (search_.state() == swiftedit::TerminalSearchState::pending) {
            search_.cancel();
            status_ = "Search cancelled";
            if (key == VK_ESCAPE)
                return;
        }
        if (prompt_ != Prompt::none) {
            if (key == VK_ESCAPE) {
                prompt_ = Prompt::none;
                exit_after_save_ = false;
                high_surrogate_ = 0;
                status_ = "Cancelled";
            } else if (prompt_ == Prompt::exit_choice) {
                if (key == 'N')
                    done_ = true;
                else if (key == 'Y') {
                    prompt_ = Prompt::none;
                    exit_after_save_ = true;
                    save();
                }
            } else if (key == VK_RETURN && prompt_ == Prompt::find) {
                swiftedit::SearchPattern pattern = query_.pattern();
                if (replace_query_) {
                    prompt_ = Prompt::replacement;
                    input_.clear();
                    status_.clear();
                } else {
                    search_.begin(buffer_, std::move(pattern));
                    prompt_ = Prompt::none;
                    status_ = "Searching... Escape cancels";
                }
            } else if (key == VK_RETURN && prompt_ == Prompt::replacement) {
                swiftedit::SearchPattern pattern = query_.pattern();
                replacement_.begin(buffer_, std::move(pattern), input_);
                prompt_ = Prompt::none;
                status_ = "Preparing Replace All... Escape cancels before publication";
            } else if (prompt_ == Prompt::find && key == VK_LEFT) {
                query_.move(false);
            } else if (prompt_ == Prompt::find && key == VK_RIGHT) {
                query_.move(true);
            } else if (prompt_ == Prompt::find && key == VK_HOME) {
                query_.home();
            } else if (prompt_ == Prompt::find && key == VK_END) {
                query_.end();
            } else if (prompt_ == Prompt::find && key == VK_OEM_2 && ctrl && shift) {
                query_.toggle();
            } else if (prompt_ == Prompt::find && (key == VK_BACK || key == VK_DELETE)) {
                query_.erase(key == VK_BACK);
            } else if (key == VK_RETURN) {
                if (input_.empty())
                    throw std::runtime_error("Enter a filename or press Escape.");
                const std::filesystem::path path(utf16(input_));
                if (prompt_ == Prompt::save_path) {
                    buffer_.save_as(path);
                    status_ = "Saved";
                    if (exit_after_save_)
                        done_ = true;
                } else if (prompt_ == Prompt::text_copy_path) {
                    buffer_.save_text_copy(path);
#ifdef SWIFTEDIT_TERMINAL_SMOKE
                    ++text_copy_saves_;
#endif
                    status_ = "Text copy saved; open document unchanged";
                } else
                    open(path);
                prompt_ = Prompt::none;
            } else if (key == VK_BACK) {
                if (!input_.empty()) {
                    const gui_forms::TextStore text(input_);
                    const gui_forms::Utf8Offset previous =
                        text.previous_grapheme_boundary(text.utf8_size());
                    input_.resize(previous.value());
                }
            } else if ((!ctrl || alt) && event.uChar.UnicodeChar >= 32)
                character(event.uChar.UnicodeChar);
            return;
        }
        if (buffer_.session().read_only()) {
            if (ctrl && !alt && key == 'A') {
                page_anchor_ = 0;
                page_caret_ = buffer_.session().size();
                status_ = "Selected entire read-only document";
                return;
            }
            if (ctrl && !alt && key == 'C') {
                const std::uint64_t first = std::min(page_anchor_, page_caret_);
                const std::uint64_t last = std::max(page_anchor_, page_caret_);
                if (first == last)
                    throw std::runtime_error("Select rows with Shift+navigation or Ctrl+A first.");
                copying_ = std::make_unique<swiftedit::SessionCopy>(buffer_.session(), first,
                                                                  last - first);
                status_ = "Copying selection... Any key cancels";
                return;
            }
            if (key == VK_DOWN || key == VK_UP || key == VK_NEXT || key == VK_PRIOR ||
                (key == VK_HOME && ctrl)) {
                const std::uint64_t before = pager_.source_offset();
                if (key == VK_DOWN)
                    pager_.down();
                else if (key == VK_UP)
                    pager_.up();
                else if (key == VK_NEXT)
                    pager_.next();
                else if (key == VK_PRIOR)
                    pager_.previous();
                else
                    pager_.first();
                if (!shift || page_anchor_ == page_caret_)
                    page_anchor_ = shift ? before : pager_.source_offset();
                page_caret_ = pager_.source_offset();
                const std::uint64_t selected = std::max(page_anchor_, page_caret_) -
                                               std::min(page_anchor_, page_caret_);
                status_ = shift ? "Selected " + std::to_string(selected) + " source bytes"
                                : "Read-only navigation";
                return;
            }
        }
        if (ctrl && !alt) {
            switch (key) {
            case 'T':
                text_copy();
                return;
            case 'H':
                replace_query_ = true;
                prompt_ = Prompt::find;
                query_.end();
                status_.clear();
                return;
            case 'W':
                replace_query_ = false;
                prompt_ = Prompt::find;
                query_.end();
                status_.clear();
                return;
            case 'X':
                if (buffer_.session().dirty())
                    prompt_ = Prompt::exit_choice;
                else
                    done_ = true;
                return;
            case 'S':
                exit_after_save_ = false;
                save();
                return;
            case 'R':
                if (buffer_.session().dirty())
                    throw std::runtime_error("Save changes before opening another file.");
                prompt_ = Prompt::open_path;
                status_.clear();
                input_.clear();
                return;
            case 'Z':
                static_cast<void>(buffer_.undo());
                return;
            case 'Y':
                static_cast<void>(buffer_.redo());
                return;
            case 'C':
                clipboard_ = buffer_.copy();
                status_ = "Copied selection";
                return;
            case 'K':
                clipboard_ = buffer_.cut();
                status_ = "Cut to terminal clipboard";
                return;
            case 'U':
                buffer_.paste(clipboard_);
                return;
            }
        }
        switch (key) {
        case VK_F2:
            if (!buffer_.session().read_only()) {
                wrap_ = !wrap_;
                wrap_view_ = swiftedit::TerminalWrapView{&console_};
                status_ = wrap_ ? "Wrap to window" : "No wrap";
            }
            break;
        case VK_LEFT:
            move(swiftedit::TerminalMotion::left, shift);
            break;
        case VK_RIGHT:
            move(swiftedit::TerminalMotion::right, shift);
            break;
        case VK_UP:
            move(swiftedit::TerminalMotion::up, shift);
            break;
        case VK_DOWN:
            move(swiftedit::TerminalMotion::down, shift);
            break;
        case VK_HOME:
            move(ctrl ? swiftedit::TerminalMotion::document_start : swiftedit::TerminalMotion::home,
                 shift);
            break;
        case VK_END:
            move(ctrl ? swiftedit::TerminalMotion::document_end : swiftedit::TerminalMotion::end,
                 shift);
            break;
        case VK_PRIOR:
            move(swiftedit::TerminalMotion::up, shift, rows_);
            break;
        case VK_NEXT:
            move(swiftedit::TerminalMotion::down, shift, rows_);
            break;
        case VK_BACK:
            buffer_.erase(true);
            break;
        case VK_DELETE:
            buffer_.erase(false);
            break;
        case VK_RETURN:
            buffer_.enter();
            break;
        case VK_TAB:
            buffer_.insert("\t");
            break;
        case VK_F5: {
            const std::string timestamp = notepad::current_date_time();
            buffer_.insert(timestamp);
            break;
        }
        case VK_F6:
            counting_ = std::make_unique<swiftedit::SessionWordCount>(buffer_.session());
            status_ = "Counting document words... Any key cancels";
            break;
        case VK_F3: {
            swiftedit::SearchPattern pattern = query_.pattern();
            search_.begin(buffer_, std::move(pattern));
            status_ = "Searching... Escape cancels";
            break;
        }
        case VK_F1:
            status_ = "Ctrl+S saves; Ctrl+X asks before discarding. Clipboard is private to this "
                      "terminal.";
            break;
        default:
            if ((!ctrl || alt) && event.uChar.UnicodeChar >= 32)
                character(event.uChar.UnicodeChar);
            break;
        }
    }
#ifdef SWIFTEDIT_TERMINAL_SMOKE
    std::size_t search_hits_{};
    std::size_t wrap_cancellations_{};
    std::size_t text_copy_saves_{};
    std::size_t completed_counts_{}, cancelled_counts_{};
    std::size_t completed_copies_{}, cancelled_copies_{};
    std::uint64_t last_word_count_{};
#endif
    Console console_{};
    swiftedit::TerminalBuffer buffer_{};
    swiftedit::TerminalRowCache row_cache_{};
    swiftedit::TerminalWrapView wrap_view_{&console_};
    bool wrap_{}, wrap_suspended_{};
    swiftedit::TerminalPager pager_{};
    swiftedit::TerminalSearch search_{};
    swiftedit::TerminalReplace replacement_{};
    std::unique_ptr<swiftedit::SessionWordCount> counting_{};
    std::unique_ptr<swiftedit::SessionCopy> copying_{};
    std::uint64_t page_anchor_{}, page_caret_{};
    bool replace_query_{};
    swiftedit::TerminalQuery query_{};
    Prompt prompt_{Prompt::none};
    std::string input_{}, status_{"F1 Help"};
    swiftedit::SourceClipboard clipboard_{};
    std::size_t top_{}, left_{}, width_{80}, rows_{20};
    wchar_t high_surrogate_{};
    bool done_{}, exit_after_save_{};
};
} // namespace
#ifndef SWIFTEDIT_TERMINAL_SMOKE
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc > 2 || (argc == 2 && std::wstring_view(argv[1]) == L"--help")) {
            std::cout << "SwiftEdit interactive terminal\nUsage: swiftedit-terminal [file]\n"
                         "Ctrl+S Save, Ctrl+R Open, Ctrl+X Exit, Shift+arrows Select, "
                         "Ctrl+Z/Y Undo/Redo, Ctrl+K Cut, Ctrl+U Paste, Ctrl+T Save Text Copy, "
                         "F2 Wrap to window, F5 Insert Date and Time, F6 Word Count.\n"
                         "Read-only pages: Shift+Up/Down or Page keys select rows; "
                         "Ctrl+A selects all; Ctrl+C copies; any key cancels pending copy.\n";
            return argc > 2 ? 1 : 0;
        }
        Terminal terminal{};
        const std::filesystem::path path =
            argc == 2 ? std::filesystem::path(argv[1]) : std::filesystem::path{};
        const int result = terminal.run(path);
        return result;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}

#else
namespace {
struct SmokeConsole {
    HANDLE input{INVALID_HANDLE_VALUE}, output{INVALID_HANDLE_VALUE};
    bool allocated{};
    ~SmokeConsole() {
        if (input != INVALID_HANDLE_VALUE)
            CloseHandle(input);
        if (output != INVALID_HANDLE_VALUE)
            CloseHandle(output);
        if (allocated)
            FreeConsole();
    }
};
void enqueue(HANDLE input, WORD key, wchar_t character = 0, DWORD modifiers = 0) {
    INPUT_RECORD record{};
    record.EventType = KEY_EVENT;
    record.Event.KeyEvent.bKeyDown = TRUE;
    record.Event.KeyEvent.wRepeatCount = 1;
    record.Event.KeyEvent.wVirtualKeyCode = key;
    record.Event.KeyEvent.uChar.UnicodeChar = character;
    record.Event.KeyEvent.dwControlKeyState = modifiers;
    DWORD written = 0;
    if (!WriteConsoleInputW(input, &record, 1, &written) || written != 1)
        throw std::runtime_error("Cannot enqueue input in owned test console.");
    record.Event.KeyEvent.bKeyDown = FALSE;
    if (!WriteConsoleInputW(input, &record, 1, &written) || written != 1)
        throw std::runtime_error("Cannot enqueue key release in owned test console.");
}
} // namespace
int wmain() {
    try {
        SmokeConsole console{};
        // Always detach before allocating: fixture input must never reach a
        // caller's console, including when this test is launched manually.
        FreeConsole();
        if (!AllocConsole())
            throw std::runtime_error("Cannot allocate private test console.");
        console.allocated = true;
        ShowWindow(GetConsoleWindow(), SW_HIDE);
        console.input =
            CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, 0, nullptr);
        console.output =
            CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (console.input == INVALID_HANDLE_VALUE || console.output == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot open owned test console.");
        SetStdHandle(STD_INPUT_HANDLE, console.input);
        SetStdHandle(STD_OUTPUT_HANDLE, console.output);
        DWORD mode = 0;
        if (!GetConsoleMode(console.input, &mode))
            throw std::runtime_error("Cannot read initial input mode.");
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() /
            ("swiftedit-console-" + std::to_string(GetCurrentProcessId()));
        if (!std::filesystem::create_directory(dir))
            throw std::runtime_error("Test fixture already exists.");
        struct Cleanup {
            std::filesystem::path path{};
            ~Cleanup() {
                std::error_code error{};
                std::filesystem::remove_all(path, error);
            }
        } cleanup{dir};
        const std::filesystem::path path = dir / "edit.txt";
        {
            std::ofstream file(path, std::ios::binary);
        }
        if (!FlushConsoleInputBuffer(console.input))
            throw std::runtime_error("Cannot reset test input.");
        enqueue(console.input, 'A', L'a');
        enqueue(console.input, VK_RETURN, L'\r');
        enqueue(console.input, 0, 0xd83d);
        enqueue(console.input, 0, 0xde00);
        enqueue(console.input, 'S', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_HOME, 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, 'W', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, 'A', L'a');
        enqueue(console.input, VK_OEM_2, 0, LEFT_CTRL_PRESSED | SHIFT_PRESSED);
        enqueue(console.input, VK_RETURN, L'\r');
        enqueue(console.input, 'H', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_RETURN, L'\r');
        enqueue(console.input, 'B', L'b');
        enqueue(console.input, VK_RETURN, L'\r');
        enqueue(console.input, 'Z', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_END, 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, 'Q', L'q');
        enqueue(console.input, 'X', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_ESCAPE);
        enqueue(console.input, 'Z', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_F6);
        enqueue(console.input, 'X', 0, LEFT_CTRL_PRESSED);
        {
            Terminal terminal{};
            if (terminal.run(path) != 0)
                throw std::runtime_error("Terminal run failed.");
            if (terminal.replacement_count() != 3)
                throw std::runtime_error(
                    "Terminal Replace All did not replace all three graphemes.");
            if (terminal.search_hits() != 1 || !terminal.wildcard_enabled())
                throw std::runtime_error(
                    "Terminal wildcard Find did not publish the expected match.");
            if (terminal.completed_counts() != 1 || terminal.last_word_count() != 2)
                throw std::runtime_error("F6 did not count the saved document.");
        }
        DWORD restored = 0;
        if (!GetConsoleMode(console.input, &restored) || restored != mode)
            throw std::runtime_error("Terminal failed to restore console input mode.");
        const std::string expected = "a" + notepad::preferred_newline("") + "\xf0\x9f\x98\x80";
        if (notepad::read_file(path).bytes != expected)
            throw std::runtime_error(
                "Console input/save/undo did not preserve expected UTF-8 bytes.");
        const std::filesystem::path wrap_path = dir / "wrap.txt";
        const std::string wrap_source(4096, 'a');
        {
            std::ofstream file(wrap_path, std::ios::binary);
            file << wrap_source;
        }
        FlushConsoleInputBuffer(console.input);
        enqueue(console.input, VK_F2);
        enqueue(console.input, VK_DOWN);
        enqueue(console.input, VK_HOME);
        enqueue(console.input, 'X', L'X');
        enqueue(console.input, 'S', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_F2);
        enqueue(console.input, 'X', 0, LEFT_CTRL_PRESSED);
        std::size_t wrap_width = 0;
        {
            Terminal terminal{};
            if (terminal.run(wrap_path) != 0 || terminal.wraps())
                throw std::runtime_error("Terminal wrap toggle did not return to no-wrap.");
            wrap_width = terminal.viewport_width();
        }
        std::string wrap_expected = wrap_source;
        wrap_expected.insert(wrap_width, "X");
        if (notepad::read_file(wrap_path).bytes != wrap_expected)
            throw std::runtime_error("Wrapped Down/Home/edit saved the wrong source position.");
        if (!GetConsoleMode(console.input, &restored) || restored != mode)
            throw std::runtime_error("Wrapped terminal did not restore input mode.");
        FlushConsoleInputBuffer(console.input);
        enqueue(console.input, VK_F2);
        enqueue(console.input, VK_END, 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_ESCAPE);
        enqueue(console.input, VK_HOME, 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_DOWN);
        enqueue(console.input, VK_HOME);
        enqueue(console.input, 'Y', L'Y');
        enqueue(console.input, 'X', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_ESCAPE);
        enqueue(console.input, 'S', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, 'X', 0, LEFT_CTRL_PRESSED);
        {
            Terminal terminal{};
            if (terminal.run(wrap_path) != 0 || terminal.wrap_cancellations() != 1)
                throw std::runtime_error(
                    "Wrap cancellation/recovery did not execute exactly once.");
        }
        wrap_expected.insert(wrap_width, "Y");
        if (notepad::read_file(wrap_path).bytes != wrap_expected)
            throw std::runtime_error("Wrap cancellation/recovery or exit cancel changed source.");
        if (!GetConsoleMode(console.input, &restored) || restored != mode)
            throw std::runtime_error("Cancelled wrapped terminal did not restore input mode.");
        const std::filesystem::path illegal_path = dir / "illegal.txt";
        const std::filesystem::path copy_path = dir / "illegal.1.txt";
        const std::string illegal_source("A\xff\xfe\r\rZ", 6);
        {
            std::ofstream file(illegal_path, std::ios::binary);
            file.write(illegal_source.data(), static_cast<std::streamsize>(illegal_source.size()));
        }
        FlushConsoleInputBuffer(console.input);
        enqueue(console.input, 'T', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_ESCAPE);
        enqueue(console.input, 'X', 0, LEFT_CTRL_PRESSED);
        {
            Terminal terminal{};
            if (terminal.run(illegal_path) != 0 || std::filesystem::exists(copy_path))
                throw std::runtime_error("Cancelled Text Copy wrote a destination.");
        }
        FlushConsoleInputBuffer(console.input);
        enqueue(console.input, 'Q', L'Q');
        enqueue(console.input, 'T', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_RETURN, L'\r');
        // A second attempt must refuse this now-existing destination.
        enqueue(console.input, 'T', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_RETURN, L'\r');
        enqueue(console.input, VK_ESCAPE);
        enqueue(console.input, 'X', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, 'N', L'n');
        {
            Terminal terminal{};
            if (terminal.run(illegal_path) != 0 || terminal.text_copy_saves() != 1 ||
                !terminal.document_dirty())
                throw std::runtime_error("Text Copy terminal run failed.");
        }
        if (notepad::read_file(copy_path).bytes != "QA  \r\rZ" ||
            notepad::read_file(illegal_path).bytes != illegal_source)
            throw std::runtime_error(
                "Text Copy changed source or failed byte-for-space conversion.");
        if (!GetConsoleMode(console.input, &restored) || restored != mode)
            throw std::runtime_error("Text Copy terminal did not restore input mode.");
        const std::filesystem::path large_path = dir / "large.txt";
        {
            std::ofstream file(large_path, std::ios::binary);
            file << "read-only\r\n\xf0\x9f\x98\x80";
            file.seekp(swiftedit::editable_limit - 1);
            file.put('z');
        }
        const std::filesystem::file_time_type original_time =
            std::filesystem::last_write_time(large_path);
        FlushConsoleInputBuffer(console.input);
        enqueue(console.input, VK_NEXT);
        enqueue(console.input, VK_PRIOR);
        enqueue(console.input, VK_HOME, 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_DOWN, 0, SHIFT_PRESSED);
        enqueue(console.input, 'C', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, 'A', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, 'C', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, VK_ESCAPE);
        enqueue(console.input, VK_F6);
        enqueue(console.input, VK_ESCAPE);
        enqueue(console.input, 'S', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, 'X', 0, LEFT_CTRL_PRESSED);
        {
            Terminal terminal{};
            if (terminal.run(large_path) != 0 || terminal.cancelled_counts() != 1 ||
                terminal.completed_counts() != 0 || terminal.completed_copies() != 1 ||
                terminal.cancelled_copies() != 1 || terminal.clipboard_bytes() != "read-only\r\n")
                throw std::runtime_error("Read-only terminal run failed.");
        }
        if (std::filesystem::last_write_time(large_path) != original_time ||
            std::filesystem::file_size(large_path) != swiftedit::editable_limit)
            throw std::runtime_error("Read-only terminal navigation/save altered the file.");
        if (!GetConsoleMode(console.input, &restored) || restored != mode)
            throw std::runtime_error("Read-only terminal did not restore input mode.");
        std::cout << "Owned console smoke passed: input, supplementary Unicode, save, dirty-exit "
                     "cancel, undo, wrapped editing, large-file pages and mode restoration.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
#endif
