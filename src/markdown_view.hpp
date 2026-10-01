#pragma once
#include "markdown.hpp"
#include <gui_forms/gui_forms.hpp>

namespace notepad {
namespace gf = gui_forms;
class MarkdownView final : public gf::Control {
public:
    static constexpr bool initialize_tree_after_construction = true;
    explicit MarkdownView(gf::StableId);
    void initialize_control_tree();
    void set_source(std::string_view);
    void arrange(gf::Rect) override;
    void on_paint(gf::Painter &, gf::Rect) override;
    void on_pointer(gf::PointerEvent &) override;
    [[nodiscard]] std::size_t block_count() const {
        const std::size_t count = blocks_.size();
        return count;
    }

protected:
    void on_dispose() noexcept override;

private:
    struct Run {
        gf::Rect bounds{};
        gf::FontSpec font{};
        std::string text{}, url{};
        bool strike{}, code{}, border{}, rule{};
    };
    struct RunOrder {
        bool operator()(const Run &left, const Run &right) const;
    };
    struct ScrollListener {
        std::weak_ptr<MarkdownView> owner{};
        void operator()(const double &) const;
    };
    void layout(gf::Painter &, double width);
    std::string source_{};
    std::vector<swiftedit::MarkdownBlock> blocks_{};
    std::vector<Run> runs_{};
    std::shared_ptr<gf::VScrollBar> vertical_{};
    std::shared_ptr<gf::HScrollBar> horizontal_{};
    std::vector<gf::SubscriptionToken> subscriptions_{};
    std::unique_ptr<gf::ToolTip> tooltip_{};
    double layout_width_{}, content_height_{}, content_width_{}, maximum_run_height_{32};
    bool layout_dirty_{true};
    std::string hovered_url_{};
    std::string layout_error_{};
};
} // namespace notepad
