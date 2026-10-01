#include "session.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "platform.hpp"

void check(bool b, const char *s) {
    if (!b)
        throw std::runtime_error(s);
}

void raw(const std::filesystem::path &p, std::string_view s) {
    std::ofstream f(p, std::ios::binary);
    f.write(s.data(), s.size());
}
int main() {
    try {
        using namespace swiftedit;
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() /
            ("swiftedit-session-" + std::to_string(test_process_id()));
        const bool observed_5 = std::filesystem::create_directory(dir);
        check(observed_5, "unique fixture");
        struct Cleanup {
            std::filesystem::path dir{};
            ~Cleanup() {
                std::error_code ec{};
                std::filesystem::remove_all(dir, ec);
            }
        } cleanup{dir};
        const std::filesystem::path file = dir / "sample.txt";
        raw(file, "before old after\r\nsecond old after");
        Session s{};
        Session other{};
        const DocumentStamp initial_stamp = s.stamp();
        check(initial_stamp.identity.value != 0 && other.identity() != s.identity(),
              "Independent sessions have distinct nonzero document identities");
        check(other.revision() == s.revision(), "Cross-session test has matching revisions");
        bool foreign_refused = false;
        try {
            other.replace_ranges({{0, 0}}, "foreign", initial_stamp);
        } catch (const std::exception &) {
            foreign_refused = true;
        }
        check(foreign_refused && other.text().empty(), "Foreign document stamp refused intact");
        other.replace_ranges({{0, 0}}, "local", other.stamp());
        const DocumentIdentity before_reset = other.identity();
        other.reset();
        check(other.identity() != before_reset, "Reset starts a distinct document lifetime");
        s.open(file);
        check(s.identity() != initial_stamp.identity, "Open starts a distinct document lifetime");
        const DocumentIdentity opened_identity = s.identity();
        const DocumentRevision opened_revision = s.revision();
        bool missing_refused = false;
        try {
            s.open(dir / "missing.txt");
        } catch (const std::exception &) {
            missing_refused = true;
        }
        check(missing_refused && s.identity() == opened_identity && s.revision() == opened_revision,
              "Failed open preserves document identity and revision");
        check(!s.dirty(), "clean open");
        std::vector<Preview> p = s.preview("", "old", " after", "new");
        check(p.size() == 2, "ambiguity enumerated");
        s.commit(p[1].token, p[1].revision);
        check(s.text() == "before old after\r\nsecond new after", "selected exact occurrence");
        {
            bool refused = false;
            try {
                s.commit(p[0].token, p[0].revision);
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        const bool undone = s.undo();
        check(undone && s.text() == "before old after\r\nsecond old after", "undo exact bytes");
        const bool redone = s.redo();
        check(redone, "redo");
        s.save();
        check(s.identity() == opened_identity, "Editing, undo, redo and save preserve identity");
        const bool undo_after_save = s.undo();
        check(!undo_after_save && !s.dirty(), "successful save resets history");
        s.restore_opened();
        check(s.dirty() && s.text() == "before old after\r\nsecond old after",
              "opened baseline survives save");
        const bool restore_undone = s.undo();
        check(restore_undone && !s.dirty(), "restore itself undoable");
        const DocumentRevision revision = s.revision();
        {
            bool refused = false;
            try {
                static_cast<void>(s.preview("", "new", "", "\r\rL1\r\r"));
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        check(s.revision() == revision, "marker refusal atomic");
        p = s.preview("", "new", "", "changed");
        s.commit(p[0].token, p[0].revision);
        raw(file, "external");
        {
            bool refused = false;
            try {
                s.save();
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        check(s.dirty(), "failed save retains undo");
        const bool failed_save_undone = s.undo();
        check(failed_save_undone, "failed save retains undo");
        const notepad::FileSnapshot observed_1 = notepad::read_file(file);
        check(observed_1.bytes == "external", "external bytes preserved");
        const std::filesystem::path bad = dir / "bad.bin";
        std::string source = "A";
        source += static_cast<char>(0xff);
        source += "B\r\rC";
        raw(bad, source);
        s.open(bad);
        check(s.text() == source && s.illegal_bytes() == 1, "invalid source byte fidelity");
        {
            bool refused = false;
            try {
                s.save();
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        const std::filesystem::path copy = dir / "copy.txt";
        s.save_text_copy(copy);
        const notepad::FileSnapshot observed_2 = notepad::read_file(copy);
        check(observed_2.bytes == "A B\r\rC", "one illegal byte one space");
        const notepad::FileSnapshot observed_3 = notepad::read_file(bad);
        check(observed_3.bytes == source && !s.dirty(), "copy preserves original and state");
        {
            bool refused = false;
            try {
                s.save_text_copy(copy);
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        p = s.preview("A", std::string(1, static_cast<char>(0xff)), "B", "valid");
        s.commit(p[0].token, p[0].revision);
        s.save();
        const notepad::FileSnapshot observed_4 = notepad::read_file(bad);
        check(observed_4.bytes == "AvalidB\r\rC",
              "repair invalid bytes with source controls preserved");
        std::string all{};
        for (unsigned i = 0; i < 256; ++i)
            all += static_cast<char>(i);
        const std::string escaped_all = escape_field(all);
        const std::string roundtrip = unescape_field(escaped_all);
        check(roundtrip == all, "transport all bytes roundtrip");
        const std::string calculated_1 = escape_field("\x1b[31m");
        check(calculated_1 == "\\x1b[31m", "terminal controls escaped");
        {
            bool refused = false;
            try {
                static_cast<void>(unescape_field("\\xq0"));
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        const std::string calculated_2 = text_copy(std::string("\xc0\x80", 2));
        check(calculated_2 == "  ", "overlong sequence illegal per byte");
        const std::string calculated_3 = text_copy("e\xcc\x81\xf0\x9f\x98\x80");
        check(calculated_3 == "e\xcc\x81\xf0\x9f\x98\x80", "Unicode preserved");
        const std::string calculated_4 = normalize_newlines("a\r\nb\nc\rd", "\n");
        check(calculated_4 == "a\nb\nc\nd", "explicit normalization preserves missing EOF newline");
        const std::string calculated_5 = suggested_name("[notes.md]\r\nhello");
        check(calculated_5 == "notes.md", "bracket name");
        const std::string calculated_6 = suggested_name("[CON.txt]");
        check(calculated_6 == "Untitled.txt", "reserved name");
        const std::string calculated_7 = suggested_name("[../oops]");
        check(calculated_7 == "Untitled.txt", "path not filename");
        const std::filesystem::path calculated_8 = versioned_name("report.csv");
        check(calculated_8 == std::filesystem::path("report.1.csv"), "version suffix");
        const std::filesystem::path calculated_9 = versioned_name("report.9.csv");
        check(calculated_9 == std::filesystem::path("report.10.csv"), "version increment");
        const std::filesystem::path big = dir / "large.txt";
        {
            std::ofstream f(big, std::ios::binary);
            f.seekp(editable_limit - 1);
            f.put('Z');
        }
        s.open(big);
        check(s.read_only() && s.size() == editable_limit, "exact large threshold");
        const Page final_page = s.page(editable_limit - 1, 16);
        check(final_page.bytes == "Z", "bounded final page");
        {
            bool refused = false;
            try {
                static_cast<void>(s.preview("", "Z", "", "z"));
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        {
            bool refused = false;
            try {
                s.save();
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        {
            bool refused = false;
            try {
                static_cast<void>(s.page(0, maximum_page + 1));
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        const std::filesystem::path small = dir / "nearly-large.txt";
        raw(small, std::string(editable_limit - 1, 'x'));
        s.open(small);
        check(!s.read_only() && s.size() == editable_limit - 1, "below threshold editable");
        {
            bool refused = false;
            try {
                static_cast<void>(s.preview("", std::string(editable_limit - 1, 'x'), "",
                                            std::string(editable_limit, 'y')));
            } catch (const std::exception &failure) {
                refused = true;
            }
            check(refused, "Expected refusal");
        }
        std::cout << "Session tests passed: exact previews, stale guard, ambiguity, save undo "
                     "boundary, original restore, malformed bytes, copy safety, escaped transport, "
                     "version names, bounded large pages.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
