#include "csv_view.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace gf = gui_forms;
class ResultPainter final : public gf::Painter {
public:
    std::size_t results{};
    bool error{};
    void save() override {}
    void restore() override {}
    void translate(gf::Point) override {}
    void clip_rect(gf::Rect) override {}
    void fill_rect(gf::Rect, gf::Color) override {}
    void stroke_rect(gf::Rect, gf::Color, double) override {}
    void draw_line(gf::Point, gf::Point, gf::Color, double) override {}
    void draw_image(gf::ImageId, gf::Rect, double) override {}
    void draw_text_utf8(gf::Point, std::string_view text, gf::FontSpec, gf::Color) override {
        if (text == "1024")
            ++results;
        if (text == "#ERROR")
            error = true;
    }
};
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("Usage: swiftedit-csv-view-bench samples.csv");
        std::ofstream output(argv[1]);
        if (!output)
            throw std::runtime_error("Cannot write benchmark samples.");
        output << "sample,milliseconds\n" << std::fixed << std::setprecision(6);
        std::string source{};
        for (std::size_t row = 0; row < 512; ++row) {
            if (row)
                source += '\n';
            source += '2';
            for (std::size_t column = 1; column < 8; ++column)
                source += ",=SUM(A1:A512)";
        }
        const std::shared_ptr<notepad::CsvView> view =
            gf::make_control<notepad::CsvView>(gf::StableId("bench.csv"));
        gf::Window window(view, {800, 600});
        (*view).set_source(source);
        window.perform_layout();
        std::vector<double> samples{};
        samples.reserve(31);
        for (std::size_t sample = 0; sample < 36; ++sample) {
            gf::PointerEvent wheel{};
            wheel.action = gf::PointerAction::wheel;
            wheel.wheel_delta.y = sample % 2 == 0 ? -1 : 1;
            const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            (*view).on_pointer(wheel);
            const std::chrono::duration<double, std::milli> elapsed =
                std::chrono::steady_clock::now() - start;
            ResultPainter painter{};
            (*view).on_paint(painter, {0, 0, 800, 600});
            if (!wheel.handled || painter.error || painter.results != 85)
                throw std::runtime_error(
                    "CSV viewport did not display all 85 exact formula results.");
            if (sample >= 5) {
                samples.push_back(elapsed.count());
                output << sample - 5 << ',' << elapsed.count() << '\n';
            }
        }
        std::sort(samples.begin(), samples.end());
        std::cout << "source_bytes=" << source.size() << " samples=" << samples.size()
                  << " p50_ms=" << samples[15] << " p95_ms=" << samples[29]
                  << " p99_ms=" << samples[30] << " worst_ms=" << samples.back() << '\n';
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
