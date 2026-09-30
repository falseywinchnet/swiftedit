#include "session.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <windows.h>

void check(bool b, const char *s) {
    if (!b)
        throw std::runtime_error(s);
}
template <class F> void refuses(F f) {
    bool threw = false;
    try {
        f();
    } catch (const std::exception &) {
        threw = true;
    }
    check(threw, "Expected refusal");
}
void raw(const std::filesystem::path &p, std::string_view s) {
    std::ofstream f(p, std::ios::binary);
    f.write(s.data(), s.size());
}
int main() {
    try {
        using namespace swiftedit;
        auto dir = std::filesystem::temp_directory_path() /
                   ("swiftedit-session-" + std::to_string(GetCurrentProcessId()));
        check(std::filesystem::create_directory(dir), "unique fixture");
        struct Cleanup {
            std::filesystem::path dir;
            ~Cleanup() {
                std::error_code ec;
                std::filesystem::remove_all(dir, ec);
            }
        } cleanup{dir};
        auto file = dir / "sample.txt";
        raw(file, "before old after\r\nsecond old after");
        Session s;
        s.open(file);
        check(!s.dirty(), "clean open");
        auto p = s.preview("", "old", " after", "new");
        check(p.size() == 2, "ambiguity enumerated");
        s.commit(p[1].token, p[1].revision);
        check(s.text() == "before old after\r\nsecond new after", "selected exact occurrence");
        refuses([&] { s.commit(p[0].token, p[0].revision); });
        check(s.undo() && s.text() == "before old after\r\nsecond old after", "undo exact bytes");
        check(s.redo(), "redo");
        s.save();
        check(!s.undo() && !s.dirty(), "successful save resets history");
        s.restore_opened();
        check(s.dirty() && s.text() == "before old after\r\nsecond old after",
              "opened baseline survives save");
        check(s.undo() && !s.dirty(), "restore itself undoable");
        auto revision = s.revision();
        refuses([&] { s.preview("", "new", "", "\r\rL1\r\r"); });
        check(s.revision() == revision, "marker refusal atomic");
        p = s.preview("", "new", "", "changed");
        s.commit(p[0].token, p[0].revision);
        raw(file, "external");
        refuses([&] { s.save(); });
        check(s.dirty() && s.undo(), "failed save retains undo");
        check(notepad::read_file(file).bytes == "external", "external bytes preserved");
        auto bad = dir / "bad.bin";
        std::string source = "A";
        source += char(0xff);
        source += "B\r\rC";
        raw(bad, source);
        s.open(bad);
        check(s.text() == source && s.illegal_bytes() == 1, "invalid source byte fidelity");
        refuses([&] { s.save(); });
        auto copy = dir / "copy.txt";
        s.save_text_copy(copy);
        check(notepad::read_file(copy).bytes == "A B\r\rC", "one illegal byte one space");
        check(notepad::read_file(bad).bytes == source && !s.dirty(),
              "copy preserves original and state");
        refuses([&] { s.save_text_copy(copy); });
        p = s.preview("A", std::string(1, char(0xff)), "B", "valid");
        s.commit(p[0].token, p[0].revision);
        s.save();
        check(notepad::read_file(bad).bytes == "AvalidB\r\rC",
              "repair invalid bytes with source controls preserved");
        std::string all;
        for (unsigned i = 0; i < 256; ++i)
            all += char(i);
        check(unescape_field(escape_field(all)) == all, "transport all bytes roundtrip");
        check(escape_field("\x1b[31m") == "\\x1b[31m", "terminal controls escaped");
        refuses([] { unescape_field("\\xq0"); });
        check(text_copy(std::string("\xc0\x80", 2)) == "  ", "overlong sequence illegal per byte");
        check(text_copy("e\xcc\x81\xf0\x9f\x98\x80") == "e\xcc\x81\xf0\x9f\x98\x80",
              "Unicode preserved");
        check(normalize_newlines("a\r\nb\nc\rd", "\n") == "a\nb\nc\nd",
              "explicit normalization preserves missing EOF newline");
        check(suggested_name("[notes.md]\r\nhello") == "notes.md", "bracket name");
        check(suggested_name("[CON.txt]") == "Untitled.txt", "reserved name");
        check(suggested_name("[../oops]") == "Untitled.txt", "path not filename");
        check(versioned_name("report.csv") == std::filesystem::path("report.1.csv"),
              "version suffix");
        check(versioned_name("report.9.csv") == std::filesystem::path("report.10.csv"),
              "version increment");
        auto big = dir / "large.txt";
        {
            std::ofstream f(big, std::ios::binary);
            f.seekp(editable_limit - 1);
            f.put('Z');
        }
        s.open(big);
        check(s.read_only() && s.size() == editable_limit, "exact large threshold");
        check(s.page(editable_limit - 1, 16).bytes == "Z", "bounded final page");
        refuses([&] { s.preview("", "Z", "", "z"); });
        refuses([&] { s.save(); });
        refuses([&] { s.page(0, maximum_page + 1); });
        auto small = dir / "nearly-large.txt";
        raw(small, std::string(editable_limit - 1, 'x'));
        s.open(small);
        check(!s.read_only() && s.size() == editable_limit - 1, "below threshold editable");
        refuses([&] {
            s.preview("", std::string(editable_limit - 1, 'x'), "",
                      std::string(editable_limit, 'y'));
        });
        std::cout << "Session tests passed: exact previews, stale guard, ambiguity, save undo "
                     "boundary, original restore, malformed bytes, copy safety, escaped transport, "
                     "version names, bounded large pages.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
