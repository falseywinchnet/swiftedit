#include "markdown_view.hpp"
#include <iostream>
#include <stdexcept>

namespace gf = gui_forms;
class ObservingPainter final : public gf::Painter {
public:
    std::size_t measurements{}, texts{};
    std::string drawn{};
    void save() override {}
    void restore() override {}
    void translate(gf::Point) override {}
    void clip_rect(gf::Rect) override {}
    void fill_rect(gf::Rect, gf::Color) override {}
    void stroke_rect(gf::Rect, gf::Color, double) override {}
    void draw_line(gf::Point, gf::Point, gf::Color, double) override {}
    void draw_image(gf::ImageId, gf::Rect, double) override {
        throw std::runtime_error("Markdown must not load image resources.");
    }
    void draw_text_utf8(gf::Point, std::string_view text, gf::FontSpec, gf::Color) override {
        ++texts;
        drawn.append(text);
    }
    gf::Size measure_text_utf8(std::string_view text, gf::FontSpec font) override {
        ++measurements;
        const gf::Size result = gf::Painter::measure_text_utf8(text, font);
        return result;
    }
};
void check(bool good, const char *message) {
    if (!good)
        throw std::runtime_error(message);
}
int main() {
    try {
        const std::shared_ptr<notepad::MarkdownView> view =
            gf::make_control<notepad::MarkdownView>(gf::StableId("test.markdown"));
        gf::Window window(view, {640, 480});
        (*view).set_source("# Title\n\n**Bold** [link](file:///inert)\n\n"
                           "| A | B |\n|---|---|\n| one | two |\n");
        window.perform_layout();
        ObservingPainter painter{};
        (*view).on_paint(painter, {0, 0, 640, 480});
        check(painter.drawn.find("Title") != std::string::npos &&
                  painter.drawn.find("Bold") != std::string::npos &&
                  painter.drawn.find("one") != std::string::npos,
              "Native Markdown paint contains parsed headings, styled text and table cells");
        const std::size_t measured = painter.measurements;
        (*view).on_paint(painter, {0, 0, 640, 480});
        check(painter.measurements == measured, "Warm Markdown paint reuses layout measurements");
        gf::PointerEvent click{};
        click.action = gf::PointerAction::down;
        click.position = {50, 70};
        (*view).on_pointer(click);
        check(click.handled, "Rendered clicks are inert");
        (*view).set_source("# Changed\n\nAfter revision.");
        painter.drawn.clear();
        (*view).on_paint(painter, {0, 0, 640, 480});
        check(painter.drawn.find("Changed") != std::string::npos,
              "Changed source invalidates render cache");
        std::cout << "Native Markdown paint/cache tests passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
