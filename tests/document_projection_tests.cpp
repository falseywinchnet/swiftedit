#include "document_projection.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

namespace gf = gui_forms;
namespace se = swiftedit;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
gf::DocumentPageRequest request(const se::Session &session, std::uint64_t begin, std::uint64_t end) {
    const se::DocumentStamp stamp = session.stamp();
    gf::DocumentPageRequest result{};
    result.revision = {stamp.identity.value, stamp.revision.value};
    result.serial = 1;
    result.permitted = {gf::SourceByteOffset(begin), gf::SourceByteOffset(end)};
    result.viewport.anchor = gf::SourceByteOffset(begin);
    return result;
}
void finish(se::DocumentProjection &producer, const se::Session &session) {
    for (std::size_t step = 0; step < 12 && producer.state() == se::ProjectionState::pending; ++step)
        static_cast<void>(producer.step(session));
}
void write(const std::filesystem::path &path, std::string_view bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    check(static_cast<bool>(file), "Fixture write failed");
}
int main() {
    try {
        const std::filesystem::path directory = std::filesystem::temp_directory_path() /
            ("swiftedit-projection-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(std::filesystem::create_directory(directory), "Unique projection fixture directory required");
        struct Cleanup {
            std::filesystem::path directory{};
            ~Cleanup() {
                std::error_code error{};
                std::filesystem::remove_all(directory, error);
            }
        } cleanup{directory};
        const std::filesystem::path path = directory / "bytes.txt";
        const std::string source = std::string("[U+0000]\0\xff\r\n", 12) + "e\xcc\x81\nlast";
        write(path, source);
        se::Session session{};
        session.open(path);
        gf::DocumentViewState view{};
        const gf::DocumentPageRequest initial = request(session, 0, session.size());
        check(view.bind(initial.revision, gf::SourceByteOffset(session.size())) == gf::DocumentViewStatus::success,
              "Installed D1 view accepts Session revision");
        const gf::DocumentRequestResult issued = view.request_page(initial.viewport, initial.permitted);
        check(issued.request.has_value(), "Installed D1 issues page request");
        se::DocumentProjection producer(session, *issued.request);
        check(producer.step(session) == se::ProjectionState::pending, "Read and projection are separate steps");
        finish(producer, session);
        gf::DocumentPage page{};
        check(producer.publish(session, page), "Complete current projection publishes");
        check(page.display_utf8 == "[U+0000][U+0000][BYTE FF]\r\ne\xcc\x81\nlast",
              "Literal label collisions, invalid bytes, controls and combining source are preserved");
        check(page.mapping[0].kind == gf::DocumentMapKind::identity_utf8 &&
                  page.mapping[8].kind == gf::DocumentMapKind::atomic_token,
              "Literal label source differs from generated atomic control label");
        check(view.publish(std::move(page)) == gf::DocumentViewStatus::success,
              "Installed public D1 accepts produced payload");
        check(session.text() == source && !session.dirty(), "Projection never mutates document bytes");
        check(!producer.publish(session, page), "Projection publication is single use");
        se::DocumentProjection split(session, request(session, 0, 11));
        check(split.step(session) == se::ProjectionState::context_required, "Split CRLF end requires context");
        se::DocumentProjection interior(session, request(session, 11, session.size()));
        check(interior.step(session) == se::ProjectionState::context_required, "Split CRLF start requires context");
        se::DocumentProjection valid_suffix(session, request(session, 12, session.size()));
        finish(valid_suffix, session);
        check(valid_suffix.publish(session, page) && page.mapping.front().source.begin.value == 12,
              "Nonzero page uses absolute source mapping");
        se::DocumentProjection occupied(session, initial);
        finish(occupied, session);
        const std::string retained = page.display_utf8;
        check(!occupied.publish(session, page) && page.display_utf8 == retained,
              "Occupied output remains unchanged");
        occupied.cancel();
        check(occupied.state() == se::ProjectionState::cancelled, "Cancel retires prepared payload");
        se::DocumentProjection stale(session, initial);
        static_cast<void>(stale.step(session));
        session.replace_ranges({{0, 1}}, "X", session.stamp());
        check(stale.step(session) == se::ProjectionState::stale, "Revision change cancels source preparation");
        gf::DocumentPage empty{};
        check(!stale.publish(session, empty) && !empty.request.serial, "Stale work cannot publish");
        se::DocumentProjection late(session, request(session, 0, session.size()));
        finish(late, session);
        session.reset();
        check(!late.publish(session, empty) && late.state() == se::ProjectionState::stale,
              "Document replacement revokes already completed projection");
        se::DocumentProjection blank(session, request(session, 0, 0));
        finish(blank, session);
        check(blank.publish(session, empty) && empty.display_utf8.empty() && empty.mapping.empty(),
              "Empty document publishes without invented mapping");
        std::string context(8191, 'a');
        context += "e\xcc\x81\r\nend";
        write(path, context);
        session.open(path);
        se::DocumentProjection crossing(session, request(session, 0, session.size()));
        finish(crossing, session);
        gf::DocumentPage crossed{};
        check(crossing.publish(session, crossed) && crossed.display_utf8 == context,
              "Read-step boundary does not split or relabel a combining grapheme");
        se::DocumentProjection interrupted(session, request(session, 0, session.size()));
        check(interrupted.step(session) == se::ProjectionState::pending, "Partial source preparation pending");
        interrupted.cancel();
        gf::DocumentPage cancelled{};
        check(interrupted.step(session) == se::ProjectionState::cancelled && !interrupted.publish(session, cancelled),
              "Cancelled partial source cannot resume or publish");
        gf::DocumentPageRequest invalid = request(session, 0, session.size());
        invalid.serial = 0;
        se::DocumentProjection missing_serial(session, invalid);
        check(missing_serial.state() == se::ProjectionState::invalid_request, "Missing request identity refused");
        invalid = request(session, 0, session.size());
        ++invalid.revision.document;
        se::DocumentProjection wrong_document(session, invalid);
        check(wrong_document.state() == se::ProjectionState::stale, "Foreign document stamp refused");
        write(path, std::string(70000, 'a'));
        session.open(path);
        se::DocumentProjection unfinished_line(session, request(session, 0, 65536));
        check(unfinished_line.step(session) == se::ProjectionState::context_required,
              "Long paragraph is not falsely certified complete at the source budget");
        const std::filesystem::path large_path = directory / "large.txt";
        std::string block{};
        for (std::size_t index = 0; index < 32768; ++index)
            block += "a\n";
        {
            std::ofstream file(large_path, std::ios::binary);
            for (std::size_t index = 0; index < 256; ++index)
                file.write(block.data(), static_cast<std::streamsize>(block.size()));
            file.close();
            check(static_cast<bool>(file), "Large fixture write");
        }
        session.open(large_path);
        check(session.read_only(), "Large fixture is read only");
        se::DocumentProjection large(session, request(session, 0, 65536));
        finish(large, session);
        gf::DocumentPage large_page{};
        check(large.publish(session, large_page) && large_page.display_utf8 == block && !session.dirty(),
              "Read-only Session supplies exact bounded document pages");
        se::DocumentProjection too_large(session, request(session, 0, 65537));
        check(too_large.state() == se::ProjectionState::budget_exceeded, "Projection enforces installed source budget");
        std::cout << "Session document projection tests passed\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
