#include "csv_view.hpp"
#include <gui_forms/application.hpp>
#include <iostream>
#include <stdexcept>

namespace gf = gui_forms;
// Application::run synchronously owns the window. Named hooks borrow this
// stack owner until shutdown; the measurement timer is stopped before close.
class CsvNative final {
public:
    struct Ready {
        CsvNative *owner{};
        void operator()(gf::Window &window, gf::ApplicationWindowHandle handle) const {
            (*owner).ready(window, handle);
        }
    };
    struct Tick {
        CsvNative *owner{};
        void operator()() const { (*owner).tick(); }
    };
    void run() {
        view_ = gf::make_control<notepad::CsvView>(gf::StableId("native.csv"));
        gf::ApplicationWindow entry{};
        entry.stable_id = "csv.native";
        entry.model = std::make_unique<gf::Window>(view_, gf::Size{800, 600});
        entry.options.title = "SwiftEdit CSV native regression";
        entry.options.initial_size = {800, 600};
        entry.options.ready = Ready{this};
        std::vector<gf::ApplicationWindow> windows{};
        windows.push_back(std::move(entry));
        const gf::ApplicationResult result = gf::Application::run(std::move(windows));
        subscription_ = {};
        timer_.reset();
        if (result.callback_exception)
            std::rethrow_exception(result.callback_exception);
        if (!result.accepted() || !passed_)
            throw std::runtime_error("Native CSV completion or settled-idle check failed.");
    }

private:
    void ready(gf::Window &window, gf::ApplicationWindowHandle handle) {
        handle_ = handle;
        window_ = &window;
        window.perform_layout();
        std::string source{};
        for (std::size_t row = 0; row < 40; ++row) {
            if (row)
                source += '\n';
            source += "=" + std::to_string(1000 + row);
        }
        (*view_).set_source(source);
        if (!(*view_).calculations_pending())
            throw std::runtime_error("Native fixture did not create deferred work.");
        timer_ = std::make_unique<gf::Timer>(window, std::chrono::milliseconds(50));
        subscription_ = (*timer_).tick().subscribe(Tick{this});
        (*timer_).start();
    }
    void tick() {
        ++ticks_;
        // No direct on_frame call: only the native scheduler may finish work.
        if (phase_ == 0 && !(*view_).calculations_pending()) {
            static_cast<void>((*window_).request_focus({}));
            (*timer_).set_interval(std::chrono::milliseconds(200));
            phase_ = 1;
        } else if (phase_ == 1) {
            (*window_).reset_activity_metrics();
            phase_ = 2;
        } else if (phase_ == 2) {
            const gf::MetricsSnapshot metrics = (*window_).metrics().snapshot();
            std::cout << "CSV settled idle metrics: " << metrics.to_json() << '\n';
            passed_ = !(*view_).calculations_pending() && metrics.frame_deadlines_fired == 0 &&
                      metrics.scheduled_frame_requests == 0;
            (*timer_).stop();
            static_cast<void>(handle_.request_close());
        } else if (ticks_ >= 40) {
            (*timer_).stop();
            static_cast<void>(handle_.request_close());
        }
    }
    std::shared_ptr<notepad::CsvView> view_{};
    gf::ApplicationWindowHandle handle_{};
    gf::Window *window_{};
    std::unique_ptr<gf::Timer> timer_{};
    gf::SubscriptionToken subscription_{};
    std::size_t ticks_{}, phase_{};
    bool passed_{};
};
int main() {
    try {
        CsvNative test{};
        test.run();
        std::cout << "Native CSV calculation frames completed and shut down cleanly.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
