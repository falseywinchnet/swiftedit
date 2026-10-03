#include "selection_set.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        const std::size_t sizes[] = {65536, 1048576, swiftedit::editable_limit - 1};
        std::cout << "source_bytes,sample,selection_ms\n";
        for (const std::size_t size : sizes) {
            std::string source(size, 'a');
            for (std::size_t index = 79; index < source.size(); index += 80)
                source[index] = '\n';
            swiftedit::Session session{};
            session.replace_ranges({{0, 0}}, source, session.stamp());
            const std::size_t offset = (size / 160) * 80;
            const std::vector<swiftedit::SourceRange> ranges{{offset, 1}};
            for (std::size_t sample = 0; sample < 6; ++sample) {
                const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                {
                    const swiftedit::SelectionSet selection(session, ranges);
                    if (!selection.can_rewrite() || selection.ranges()[0].offset != offset)
                        throw std::runtime_error("Selection benchmark correctness failure");
                }
                const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - begin;
                if (sample)
                    std::cout << size << ',' << sample << ',' << elapsed.count() << '\n';
            }
        }
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
