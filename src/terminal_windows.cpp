#include "terminal_row.hpp"
#include "terminal_page.hpp"
#include "terminal_search.hpp"
#include "terminal_query.hpp"
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
class Console {
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
    bool input_changed_{}, active_{};
};
void position(std::string &output, std::size_t row, std::size_t column) {
    output += "\x1b[" + std::to_string(row + 1) + ";" + std::to_string(column + 1) + "H";
}
void status_line(std::string &output, std::size_t row, std::string_view source, std::size_t width) {
    position(output, row, 0);
    const std::string safe = swiftedit::escape_field(source);
    output.append(safe, 0, std::min(width, safe.size()));
}
enum class Prompt { none, save_path, open_path, exit_choice, find, replacement };
class Terminal {
public:
#ifdef SWIFTEDIT_TERMINAL_SMOKE
    std::size_t search_hits() const { return search_hits_; }
    std::size_t replacement_count() const { return replacement_.count(); }
    bool wildcard_enabled() const {
        const swiftedit::SearchPattern pattern = query_.pattern();
        return pattern.slots()[0].wildcard;
    }
#endif
    void open(const std::filesystem::path &path) {
        search_.cancel();
        replacement_.cancel();
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
                } catch (const std::exception &failure) {
                    status_ = failure.what();
                }
            }
        }
        return 0;
    }

private:
    void draw() {
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
                screen += run.text;
            }
        } else {
            const std::size_t caret_line = buffer_.line_index();
            if (caret_line < top_)
                top_ = caret_line;
            if (caret_line >= top_ + rows_)
                top_ = caret_line - rows_ + 1;
            swiftedit::TerminalRow caret =
                swiftedit::terminal_row(buffer_, caret_line, left_, width_);
            if (!caret.caret_column) {
                // Reveal by source range, not byte-as-cell assumptions.
                const swiftedit::TerminalSelection selection = buffer_.selection();
                const swiftedit::SourceRange line = buffer_.line_range(caret_line);
                std::size_t column = 0;
                for (std::size_t offset = line.offset; offset < selection.caret;) {
                    const swiftedit::SourceRange range = buffer_.grapheme_range(offset);
                    const std::string_view text(buffer_.session().text().data() + range.offset,
                                                range.length);
                    const swiftedit::TerminalGlyph glyph = swiftedit::terminal_glyph(text, column);
                    column += glyph.cells;
                    offset += range.length;
                }
                left_ = column < left_ ? column : column >= width_ ? column - width_ + 1 : 0;
            }
            for (std::size_t row = 0; row < rows_ && top_ + row < buffer_.line_count(); ++row) {
                const swiftedit::TerminalRow visible =
                    swiftedit::terminal_row(buffer_, top_ + row, left_, width_);
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
                        "Read-only: PgDn Next  PgUp Previous  Ctrl+Home First  ^X Exit", width_);
        else
            status_line(screen, height - 1,
                        "Shift+arrows Select  ^C Copy  ^K Cut line/selection  ^U Paste", width_);
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
        } else if (prompt_ == Prompt::save_path || prompt_ == Prompt::open_path ||
                   prompt_ == Prompt::replacement) {
            const std::size_t limit = 32768;
            if (input_.size() + text.size() > limit)
                throw std::runtime_error("Prompt input exceeds its UTF-8 byte limit.");
            input_ += text;
        } else
            buffer_.insert(text);
    }
    void key(const KEY_EVENT_RECORD &event) {
        const bool ctrl = (event.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        const bool alt = (event.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        const bool shift = (event.dwControlKeyState & SHIFT_PRESSED) != 0;
        const WORD key = event.wVirtualKeyCode;
        if (key != 0 && event.uChar.UnicodeChar < 32)
            high_surrogate_ = 0;
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
            if (key == VK_NEXT || key == VK_DOWN) {
                pager_.next();
                return;
            }
            if (key == VK_PRIOR || key == VK_UP) {
                pager_.previous();
                return;
            }
            if (key == VK_HOME && ctrl) {
                pager_.first();
                return;
            }
        }
        if (ctrl && !alt) {
            switch (key) {
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
                clipboard_ = buffer_.selected_text();
                status_ = "Copied selection";
                return;
            case 'K':
                clipboard_ = buffer_.cut();
                status_ = "Cut to terminal clipboard";
                return;
            case 'U':
                buffer_.insert(clipboard_);
                return;
            }
        }
        switch (key) {
        case VK_LEFT:
            buffer_.move(swiftedit::TerminalMotion::left, shift);
            break;
        case VK_RIGHT:
            buffer_.move(swiftedit::TerminalMotion::right, shift);
            break;
        case VK_UP:
            buffer_.move(swiftedit::TerminalMotion::up, shift);
            break;
        case VK_DOWN:
            buffer_.move(swiftedit::TerminalMotion::down, shift);
            break;
        case VK_HOME:
            buffer_.move(ctrl ? swiftedit::TerminalMotion::document_start
                              : swiftedit::TerminalMotion::home,
                         shift);
            break;
        case VK_END:
            buffer_.move(ctrl ? swiftedit::TerminalMotion::document_end
                              : swiftedit::TerminalMotion::end,
                         shift);
            break;
        case VK_PRIOR:
            buffer_.move(swiftedit::TerminalMotion::up, shift, rows_);
            break;
        case VK_NEXT:
            buffer_.move(swiftedit::TerminalMotion::down, shift, rows_);
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
#endif
    Console console_{};
    swiftedit::TerminalBuffer buffer_{};
    swiftedit::TerminalPager pager_{};
    swiftedit::TerminalSearch search_{};
    swiftedit::TerminalReplace replacement_{};
    bool replace_query_{};
    swiftedit::TerminalQuery query_{};
    Prompt prompt_{Prompt::none};
    std::string input_{}, clipboard_{}, status_{"F1 Help"};
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
                         "Ctrl+Z/Y Undo/Redo, Ctrl+K Cut, Ctrl+U Paste.\n";
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
        }
        DWORD restored = 0;
        if (!GetConsoleMode(console.input, &restored) || restored != mode)
            throw std::runtime_error("Terminal failed to restore console input mode.");
        const std::string expected = "a" + notepad::preferred_newline("") + "\xf0\x9f\x98\x80";
        if (notepad::read_file(path).bytes != expected)
            throw std::runtime_error(
                "Console input/save/undo did not preserve expected UTF-8 bytes.");
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
        enqueue(console.input, 'S', 0, LEFT_CTRL_PRESSED);
        enqueue(console.input, 'X', 0, LEFT_CTRL_PRESSED);
        {
            Terminal terminal{};
            if (terminal.run(large_path) != 0)
                throw std::runtime_error("Read-only terminal run failed.");
        }
        if (std::filesystem::last_write_time(large_path) != original_time ||
            std::filesystem::file_size(large_path) != swiftedit::editable_limit)
            throw std::runtime_error("Read-only terminal navigation/save altered the file.");
        if (!GetConsoleMode(console.input, &restored) || restored != mode)
            throw std::runtime_error("Read-only terminal did not restore input mode.");
        std::cout << "Owned console smoke passed: input, supplementary Unicode, save, dirty-exit "
                     "cancel, undo, large-file pages and mode restoration.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
#endif
