#include "markdown_view.hpp"
#include "query_field.hpp"
#include "csv_view.hpp"
#include <iostream>
#include <stdexcept>

namespace gf = gui_forms;
class ObservingPainter final : public gf::Painter {
public:
    std::size_t measurements{}, texts{};
    std::string drawn{};
    bool fail_measurement{};
    gf::Point translation{};
    void save() override {}
    void restore() override {}
    void translate(gf::Point point) override { translation = point; }
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
        if (fail_measurement)
            throw std::runtime_error("Injected measurement failure");
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
        const std::shared_ptr<notepad::CsvView> grid =
            gf::make_control<notepad::CsvView>(gf::StableId("test.csv"));
        gf::Window grid_window(grid, {800, 600});
        (*grid).set_source("a,b,c,d,e,f,g,h");
        grid_window.perform_layout();
        const std::shared_ptr<gf::HScrollBar> horizontal =
            std::dynamic_pointer_cast<gf::HScrollBar>(grid_window.find("csv.horizontal"));
        check(static_cast<bool>(horizontal), "CSV horizontal scrollbar exists");
        for (std::size_t column = 0; column < 5; ++column) {
            gf::KeyEvent right{};
            right.physical_key = gf::PhysicalKey::right;
            (*grid).on_key(right);
            check(right.handled, "CSV arrow navigation handled");
        }
        check((*grid).selected() == swiftedit::CellAddress{0, 5} && (*horizontal).value() == 1,
              "Selecting the partially visible sixth cell scrolls it fully into view");
        (*grid).select_cell({0, 7});
        check((*horizontal).value() == 3, "Explicit cell selection also reveals its whole column");
        (*grid).arrange({0, 0, 400, 600});
        check((*horizontal).value() == 6,
              "Narrowing the viewport retains whole selected-cell visibility");
        (*grid).arrange({0, 0, 1600, 600});
        check((*horizontal).value() == 0, "Expanding to fit all columns resets horizontal offset");
        (*grid).arrange({0, 0, 400, 600});
        (*grid).set_source("a,b");
        check((*horizontal).value() == 0, "A smaller source resets obsolete horizontal extent");
        const std::shared_ptr<gf::VScrollBar> vertical =
            std::dynamic_pointer_cast<gf::VScrollBar>(grid_window.find("csv.vertical"));
        gf::PointerEvent fitting_wheel{};
        fitting_wheel.action = gf::PointerAction::wheel;
        fitting_wheel.wheel_delta.y = -1;
        (*grid).on_pointer(fitting_wheel);
        check(vertical && (*vertical).value() == 0,
              "Wheel input cannot scroll fitting rows into a disabled placeholder range");
        const std::shared_ptr<notepad::QueryField> query =
            gf::make_control<notepad::QueryField>(gf::StableId("test.query"));
        gf::Window query_window(query, {300, 32});
        (*query).set_text("a?c");
        query_window.perform_layout();
        ObservingPainter query_painter{};
        (*query).on_paint(query_painter, {0, 0, 300, 32});
        check(query_painter.drawn == "a?c", "Question mark is initially rendered literally");
        (*query).select(gf::Utf8Offset(1), gf::Utf8Offset(1));
        (*query).on_focus_changed(true);
        gf::KeyEvent toggle{};
        toggle.physical_key = 0x38;
        toggle.modifiers = gf::Modifier::control | gf::Modifier::shift;
        (*query).on_key(toggle);
        const swiftedit::SearchPattern flagged = (*query).pattern();
        check(toggle.handled && flagged.slots()[1].wildcard && (*query).text() == "a?c",
              "Ctrl-question toggles metadata without rewriting the literal query");
        query_painter.drawn.clear();
        (*query).on_paint(query_painter, {0, 0, 300, 32});
        check(query_painter.drawn == "a\xe2\x80\xa2"
                                     "c",
              "Flagged position renders as a dot");
        gf::PointerEvent right_click{};
        right_click.action = gf::PointerAction::down;
        right_click.button = gf::PointerButton::secondary;
        right_click.position = {20, 10};
        (*query).on_pointer(right_click);
        const swiftedit::SearchPattern unflagged = (*query).pattern();
        check(right_click.handled && !unflagged.slots()[1].wildcard,
              "Right-click toggles the hit character back to literal");
        (*query).toggle_slot(1);
        (*query).set_text("a?cd");
        const swiftedit::SearchPattern extended = (*query).pattern();
        check(extended.slots()[1].wildcard && !extended.slots()[3].wildcard,
              "Typing outside a flagged position retains its flag");
        (*query).set_text("a\xc3\xa9"
                          "c");
        const swiftedit::SearchPattern edited = (*query).pattern();
        check(!edited.slots()[1].wildcard, "Replaced query character starts as literal");
        (*query).select(gf::Utf8Offset(3), gf::Utf8Offset(3));
        gf::TextInputEvent inserted{};
        inserted.text_utf8 = "?";
        (*query).on_text_input(inserted);
        check(inserted.handled && (*query).text() == "a\xc3\xa9?c",
              "Query field routes standard Unicode text input");
        (*query).on_focus_changed(false);
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
        gf::PointerEvent wheel{};
        wheel.action = gf::PointerAction::wheel;
        wheel.wheel_delta.y = -1;
        (*view).on_pointer(wheel);
        (*view).on_paint(painter, {0, 0, 640, 480});
        check(painter.translation.y == 28, "Fitting content cannot scroll into artificial range");
        (*view).arrange({0, 0, 640, 90});
        (*view).on_pointer(wheel);
        (*view).on_paint(painter, {0, 0, 640, 90});
        check(painter.translation.y < 28, "Height-only shrink updates scroll extent");
        const std::size_t before_expand = painter.measurements;
        (*view).arrange({0, 0, 640, 480});
        (*view).on_paint(painter, {0, 0, 640, 480});
        check(painter.translation.y == 28 && painter.measurements == before_expand,
              "Height-only expansion resets scrolling without measuring text again");
        (*view).set_source("# Recovery");
        painter.fail_measurement = true;
        painter.drawn.clear();
        (*view).on_paint(painter, {0, 0, 640, 480});
        check(painter.drawn.find("Injected measurement failure") != std::string::npos,
              "Layout failures are reported");
        painter.fail_measurement = false;
        painter.drawn.clear();
        (*view).arrange({0, 0, 600, 480});
        (*view).on_paint(painter, {0, 0, 600, 480});
        check(painter.drawn.find("Recovery") != std::string::npos &&
                  painter.drawn.find("Injected measurement failure") == std::string::npos,
              "Successful layout retry clears the previous error");
        std::cout << "Native Markdown paint/cache tests passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
