#include "csv.hpp"
#include "session.hpp"
#include <charconv>
#include <iostream>
#include <stdexcept>

namespace {
std::uint64_t number(const std::string &s) {
    std::uint64_t v{};
    auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    if (r.ec != std::errc() || r.ptr != s.data() + s.size())
        throw std::runtime_error("Expected unsigned integer.");
    return v;
}
void field(std::string_view s) { std::cout << '\t' << swiftedit::escape_field(s); }
void page(const swiftedit::Page &p) {
    std::cout << "page\t" << p.offset << '\t' << p.next << '\t' << p.size;
    field(p.bytes);
    std::cout << '\n';
}
std::filesystem::path path(const std::string &s) {
    return std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t *>(s.data()), s.size()));
}
void previews(const std::vector<swiftedit::Preview> &list) {
    for (auto &p : list) {
        std::cout << "preview\t" << p.token << '\t' << p.revision << '\t' << p.offset << '\t'
                  << p.length << '\t' << p.inserted.size();
        field(p.before);
        field(p.removed.substr(0, 240));
        field(p.inserted.substr(0, 240));
        field(p.after);
        std::cout << '\n';
    }
}
std::vector<std::string> split(std::string_view s) {
    std::vector<std::string> f;
    std::size_t start = 0;
    for (;;) {
        auto end = s.find('\t', start);
        f.push_back(swiftedit::unescape_field(
            s.substr(start, end == s.npos ? s.size() - start : end - start)));
        if (end == s.npos)
            break;
        start = end + 1;
    }
    return f;
}
} // namespace
int main(int argc, char **argv) {
    if (argc > 1) {
        std::cout << "SwiftEdit command session v1\nRun without arguments and send tab-separated "
                     "escaped fields on stdin.\nSee docs/COMMAND_PROTOCOL.md. This process never "
                     "interprets shell commands.\n";
        return 0;
    }
    swiftedit::Session session;
    std::string line;
    std::cout << "ready\tSwiftEdit\t1\n" << std::flush;
    while (true) {
        // Bound transport allocation before reading arbitrary input.
        line.clear();
        bool overflow = false;
        char c{};
        while (std::cin.get(c) && c != '\n') {
            if (line.size() < 2 * swiftedit::editable_limit)
                line += c;
            else
                overflow = true;
        }
        if (!std::cin && line.empty() && !overflow)
            break;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        try {
            if (overflow)
                throw std::runtime_error("Request exceeds transport limit.");
            auto f = split(line);
            auto arity = [&](std::size_t n) {
                if (f.size() != n)
                    throw std::runtime_error("Incorrect field count; see command protocol.");
            };
            const auto &cmd = f.front();
            if (cmd == "quit") {
                arity(1);
                std::cout << "ok\tquit\n" << std::flush;
                break;
            } else if (cmd == "info") {
                arity(1);
                std::cout << "info\t" << session.revision() << '\t' << session.size() << '\t'
                          << session.dirty() << '\t' << session.read_only();
                auto p = session.path().u8string();
                field(std::string(reinterpret_cast<const char *>(p.data()), p.size()));
                std::cout << '\n';
            } else if (cmd == "open") {
                arity(2);
                if (session.dirty())
                    throw std::runtime_error("Unsaved edits: save or explicitly discard first.");
                session.open(path(f[1]));
                page(session.page());
            } else if (cmd == "discard") {
                arity(1);
                session.reset();
            } else if (cmd == "page") {
                arity(3);
                auto n = number(f[2]);
                if (n > swiftedit::maximum_page)
                    throw std::runtime_error("Page budget exceeds 65536 bytes.");
                page(session.page(number(f[1]), static_cast<std::size_t>(n)));
            } else if (cmd == "find") {
                arity(2);
                for (auto at : session.find(f[1])) {
                    std::cout << "match\t" << at << '\n';
                    page(session.page(at > 80 ? at - 80 : 0, 240));
                }
            } else if (cmd == "preview") {
                arity(5);
                previews(session.preview(f[1], f[2], f[3], f[4]));
            } else if (cmd.starts_with("csv-")) {
                auto ext = session.path().extension().wstring();
                for (auto &c : ext)
                    if (c >= L'A' && c <= L'Z')
                        c += 32;
                if (ext != L".csv" || session.read_only())
                    throw std::runtime_error("CSV commands require an editable .csv file.");
                swiftedit::Csv csv(session.text());
                if (cmd == "csv-get") {
                    arity(2);
                    std::cout << "cell";
                    field(f[1]);
                    field(csv.cell(swiftedit::cell_address(f[1])).value);
                    std::cout << '\n';
                } else if (cmd == "csv-calculate") {
                    arity(2);
                    auto result = swiftedit::calculate(csv, f[1]);
                    std::cout << "calculation";
                    field(result.result);
                    for (auto cell : result.references)
                        field(swiftedit::cell_name(cell));
                    std::cout << '\n';
                } else {
                    std::string next;
                    if (cmd == "csv-set") {
                        arity(3);
                        next = csv.set(swiftedit::cell_address(f[1]), f[2]);
                    } else if (cmd == "csv-clear") {
                        arity(3);
                        next =
                            csv.clear(swiftedit::cell_address(f[1]), swiftedit::cell_address(f[2]));
                    } else
                        throw std::runtime_error("Unknown CSV command.");
                    previews(session.preview("", session.text(), "", next));
                }
            } else if (cmd == "commit") {
                arity(3);
                session.commit(number(f[1]), number(f[2]));
            } else if (cmd == "undo") {
                arity(1);
                if (!session.undo())
                    throw std::runtime_error("At last-save undo boundary.");
            } else if (cmd == "redo") {
                arity(1);
                if (!session.redo())
                    throw std::runtime_error("Nothing to redo.");
            } else if (cmd == "restore-opened") {
                arity(1);
                session.restore_opened();
            } else if (cmd == "save") {
                arity(1);
                session.save();
            } else if (cmd == "save-as") {
                arity(2);
                session.save_as(path(f[1]));
            } else if (cmd == "save-text-copy") {
                arity(2);
                session.save_text_copy(path(f[1]));
            } else
                throw std::runtime_error("Unknown command.");
            std::cout << "ok\t" << cmd << '\t' << session.revision() << '\n';
        } catch (const std::exception &e) {
            std::cout << "error";
            field(e.what());
            std::cout << '\n';
        }
        std::cout << std::flush;
    }
}
