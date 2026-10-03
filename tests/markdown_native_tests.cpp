#include "markdown_view.hpp"
#include "native_font_check.hpp"
#include <gui_forms/application.hpp>
#include <iostream>
#include <stdexcept>
#ifdef __APPLE__
#include "mac_view_snapshot.hpp"
#endif

namespace gf = gui_forms;
class MarkdownProbe final : public gf::Painter {
public:
    std::size_t words{};
    void save() override {}
    void restore() override {}
    void translate(gf::Point) override {}
    void clip_rect(gf::Rect) override {}
    void fill_rect(gf::Rect, gf::Color) override {}
    void stroke_rect(gf::Rect, gf::Color, double) override {}
    void draw_line(gf::Point, gf::Point, gf::Color, double) override {}
    void draw_image(gf::ImageId, gf::Rect, double) override {}
    void draw_text_utf8(gf::Point, std::string_view text, gf::FontSpec, gf::Color) override {
        if (text == "stressword")
            ++words;
    }
};
// Synchronous Application::run owns the host. Named callbacks borrow this stack
// owner until shutdown; no native handle or timer survives run().
class MarkdownNative final {
public:
    struct Ready {
        MarkdownNative *owner{};
        void operator()(gf::Window &window, gf::ApplicationWindowHandle handle) const {
            (*owner).ready(window, handle);
        }
    };
    struct Tick {
        MarkdownNative *owner{};
        void operator()() const { (*owner).observe(); }
    };
    void run() {
        stress_source_ = "> ";
        for (std::size_t word = 0; word < 10000; ++word)
            stress_source_ += "stressword ";
        view_ = gf::make_control<notepad::MarkdownView>(gf::StableId("native.markdown"));
        (*view_).set_source(
            "# SwiftEdit Markdown\n\n"
            "Plain text with **bold**, *italic*, ~~strike~~ and `inline code`.\n\n"
            "> A quoted paragraph stays readable.\n\n"
            "- [x] Completed task\n- [ ] Remaining task\n\n"
            "| Item | Value |\n| --- | --- |\n| Alpha | 42 |\n| Beta | 7 |\n\n"
            "```text\ncode block <literal> & text\nsecond line\n```\n\n"
            "[Inert example link](https://example.invalid/?a=1&amp;b=2)\n\n"
            "![Image alt text only](https://example.invalid/image.png)\n");
        gf::ApplicationWindow entry{};
        entry.stable_id = "markdown.native";
        entry.model = std::make_unique<gf::Window>(view_, gf::Size{900, 800});
        entry.options.title = "SwiftEdit Markdown native regression";
        entry.options.initial_size = {900, 800};
        entry.options.ready = Ready{this};
        std::vector<gf::ApplicationWindow> windows{};
        windows.push_back(std::move(entry));
        const gf::ApplicationResult result = gf::Application::run(std::move(windows));
        subscription_ = {};
        timer_.reset();
        view_.reset();
        if (close_started_ != gf::FrameTime{}) {
            const std::chrono::duration<double, std::milli> elapsed = gf::FrameClock::now() - close_started_;
            std::cout << "Markdown pending-work close through owner release ms: " << elapsed.count() << '\n';
        }
        if (result.callback_exception)
            std::rethrow_exception(result.callback_exception);
        if (failure_)
            std::rethrow_exception(failure_);
        if (!result.accepted() || !passed_ || !stress_passed_ || close_started_ == gf::FrameTime{})
            throw std::runtime_error("Native Markdown did not complete a visible paint.");
    }

private:
    void observe() {
        try {
            tick();
        } catch (...) {
            failure_ = std::current_exception();
            (*timer_).stop();
            static_cast<void>(handle_.request_close());
        }
    }
    void ready(gf::Window &window, gf::ApplicationWindowHandle handle) {
        require_native_fonts(window);
        window_ = &window;
        handle_ = handle;
        timer_ = std::make_unique<gf::Timer>(window, std::chrono::milliseconds(50));
        subscription_ = (*timer_).tick().subscribe(Tick{this});
        (*timer_).start();
    }
    void tick() {
        ++ticks_;
        const gf::MetricsSnapshot metrics = (*window_).metrics().snapshot();
        if (stress_passed_) {
            if (!idle_started_) {
                // Exclude the observer's interval change and the completing
                // scheduler callback before measuring a settled interval.
                (*window_).reset_activity_metrics();
                idle_started_ = true;
                return;
            }
            std::cout << "Markdown settled idle: " << metrics.to_json() << '\n';
            if (metrics.scheduled_frame_requests || metrics.frame_deadlines_fired)
                throw std::runtime_error("Completed Markdown retained calculation scheduling.");
            (*view_).set_source(stress_source_ + " changed");
            if (!(*view_).preparation_pending())
                throw std::runtime_error("Markdown close fixture did not create pending work.");
            (*timer_).stop();
            close_started_ = gf::FrameClock::now();
            static_cast<void>(handle_.request_close());
            return;
        }
        if (stress_started_ != gf::FrameTime{}) {
            const std::chrono::duration<double, std::milli> elapsed = gf::FrameClock::now() - stress_started_;
            if (metrics.paint_passes == 0 || !(*view_).presentation_ready()) {
                if (elapsed.count() < 5000)
                    return;
                throw std::runtime_error("Native Markdown stress paint readiness timeout.");
            }
            MarkdownProbe probe{};
            (*view_).on_paint(probe, (*view_).arranged_bounds());
            if (!probe.words || probe.words > 2000)
                throw std::runtime_error("Native Markdown stress viewport did not retain bounded text.");
            std::cout << "Markdown stress first paint observed ms: " << elapsed.count()
                      << "; visible words: " << probe.words << "; metrics: " << metrics.to_json() << '\n';
            stress_passed_ = true;
            (*timer_).set_interval(std::chrono::milliseconds(200));
            return;
        }
        if ((*window_).occluded() || metrics.paint_passes == 0 || !(*view_).presentation_ready()) {
            if (ticks_ < 100)
                return;
        } else {
            try {
                if ((*view_).block_count() < 8)
                    throw std::runtime_error("Markdown native fixture did not parse its blocks.");
#ifdef __APPLE__
                capture_native_view("SwiftEdit Markdown native regression", "markdown");
#endif
                passed_ = true;
                std::cout << "Native Markdown observed paint: " << metrics.to_json() << '\n';
                (*window_).reset_activity_metrics();
                stress_started_ = gf::FrameClock::now();
                (*view_).set_source(stress_source_);
                (*timer_).set_interval(std::chrono::milliseconds(10));
                return;
            } catch (...) {
                failure_ = std::current_exception();
            }
        }
        (*timer_).stop();
        static_cast<void>(handle_.request_close());
    }
    std::shared_ptr<notepad::MarkdownView> view_{};
    gf::ApplicationWindowHandle handle_{};
    gf::Window *window_{};
    std::unique_ptr<gf::Timer> timer_{};
    gf::SubscriptionToken subscription_{};
    std::exception_ptr failure_{};
    std::size_t ticks_{};
    bool passed_{}, stress_passed_{}, idle_started_{};
    std::string stress_source_{};
    gf::FrameTime stress_started_{};
    gf::FrameTime close_started_{};
};
int main() {
    try {
        MarkdownNative test{};
        test.run();
        std::cout << "Native Markdown paint and shutdown completed; pixels require inspection.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
