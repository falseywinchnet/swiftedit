#include "save_review.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "platform.hpp"

namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void write(const std::filesystem::path &path, std::string_view text) {
    std::ofstream output(path, std::ios::binary);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output)
        throw std::runtime_error("Fixture write failed");
}
struct Cleanup {
    std::filesystem::path path{};
    ~Cleanup() {
        std::error_code error{};
        std::filesystem::remove_all(path, error);
    }
};
} // namespace
int main() {
    try {
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() /
            ("swiftedit-save-review-" + std::to_string(test_process_id()));
        check(std::filesystem::create_directory(dir), "Unique fixture directory");
        Cleanup cleanup{dir};
        const std::filesystem::path path = dir / "report.txt";
        write(path, "opened");
        notepad::Document document{};
        document.open(path);
        swiftedit::Session identity{};
        const swiftedit::DocumentStamp stamp = identity.stamp();
        write(path, "external");
        swiftedit::SaveReview review(path, stamp, document.encoding);
        bool first_stage_refused = false;
        try {
            review.publish(document, "mine", stamp);
        } catch (const std::exception &) {
            first_stage_refused = true;
        }
        check(first_stage_refused && notepad::read_file(path).bytes == "external",
              "Cannot skip first choice and review");
        review.choose(swiftedit::ConflictChoice::save_over);
        check(!review.ready(), "First-stage choice is not write authority");
        review.review(path, notepad::Encoding::utf16_le, swiftedit::SaveEndings::crlf);
        check(review.ready() && review.destructive() && review.observed().bytes == "external",
              "Review exposes exact destructive target observation");
        bool invalid_metadata = false;
        try {
            review.review(path, static_cast<notepad::Encoding>(99), swiftedit::SaveEndings::crlf);
        } catch (const std::exception &) {
            invalid_metadata = true;
        }
        check(invalid_metadata && !review.ready(), "Invalid metadata revokes earlier review");
        review.review(path, notepad::Encoding::utf16_le, swiftedit::SaveEndings::crlf);
        write(path, "later external");
        bool race_refused = false;
        try {
            review.publish(document, "mine\n", stamp);
        } catch (const std::exception &) {
            race_refused = true;
        }
        check(race_refused && !review.ready() && document.encoding == notepad::Encoding::utf8 &&
                  document.saved_text == "opened" &&
                  notepad::read_file(path).bytes == "later external",
              "Race invalidates consent and preserves external file and document metadata");
        review.review(path, notepad::Encoding::utf16_le, swiftedit::SaveEndings::crlf);
        review.publish(document, "mine\n", stamp);
        const notepad::Decoded written = notepad::decode(notepad::read_file(path).bytes);
        check(written.text == "mine\r\n" && document.saved_text == written.text &&
                  document.encoding == notepad::Encoding::utf16_le,
              "Confirmed encoding and ending choices publish together");
        bool reused = false;
        try {
            review.choose(swiftedit::ConflictChoice::save_over);
        } catch (const std::exception &) {
            reused = true;
        }
        check(reused, "Published review is one use");
        swiftedit::SaveReview copy(path, stamp, document.encoding);
        copy.choose(swiftedit::ConflictChoice::new_copy);
        const std::filesystem::path copy_path = copy.target();
        check(copy_path.filename() == "report.1.txt", "Copy suggests dot-version before extension");
        copy.review(copy_path, notepad::Encoding::utf8, swiftedit::SaveEndings::preserve);
        check(!copy.destructive(), "New copy has no destructive target");
        copy.publish(document, "copy\n", stamp);
        check(notepad::read_file(copy_path).bytes == "copy\n" &&
                  notepad::decode(notepad::read_file(path).bytes).text == "mine\r\n",
              "Copy preserves original and changes the document target only after success");
        swiftedit::SaveReview collision(path, stamp, document.encoding);
        collision.choose(swiftedit::ConflictChoice::new_copy);
        bool collision_refused = false;
        try {
            collision.review(copy_path, document.encoding, swiftedit::SaveEndings::preserve);
        } catch (const std::exception &) {
            collision_refused = true;
        }
        check(collision_refused && !collision.ready(),
              "New copy never inherits overwrite authority");
        swiftedit::SaveReview stale(path, stamp, document.encoding);
        stale.choose(swiftedit::ConflictChoice::save_over);
        stale.review(path, document.encoding, swiftedit::SaveEndings::preserve);
        identity.reset();
        bool stale_refused = false;
        try {
            stale.publish(document, "stale", identity.stamp());
        } catch (const std::exception &) {
            stale_refused = true;
        }
        check(stale_refused && !stale.ready(), "New document identity revokes prior review");
        std::cout << "Save review state and publication tests passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
