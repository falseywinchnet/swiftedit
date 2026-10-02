#include "markdown_view.hpp"
#include "native_font_check.hpp"
#include <gui_forms/application.hpp>
#include <iostream>
#include <stdexcept>
#ifdef __APPLE__
#include "mac_view_snapshot.hpp"
#endif

namespace gf = gui_forms;
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
        void operator()() const { (*owner).tick(); }
    };
    void run() {
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
        if ((*view_).block_count() < 8)
            throw std::runtime_error("Markdown native fixture did not parse its blocks.");
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
        if (result.callback_exception)
            std::rethrow_exception(result.callback_exception);
        if (failure_)
            std::rethrow_exception(failure_);
        if (!result.accepted() || !passed_)
            throw std::runtime_error("Native Markdown did not complete a visible paint.");
    }

private:
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
        if ((*window_).occluded() || metrics.paint_passes == 0) {
            if (ticks_ < 100)
                return;
        } else {
            try {
#ifdef __APPLE__
                capture_native_view("SwiftEdit Markdown native regression", "markdown");
#endif
                passed_ = true;
                std::cout << "Native Markdown observed paint: " << metrics.to_json() << '\n';
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
    bool passed_{};
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
