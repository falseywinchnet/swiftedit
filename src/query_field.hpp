#pragma once
#include "search.hpp"
#include <gui_forms/gui_forms.hpp>

namespace notepad {
namespace gf = gui_forms;
// The private child supplies standard text editing/clipboard behavior. This
// control owns focus, hit testing and the visible, flagged-grapheme rendering.
class QueryField final : public gf::Control {
public:
    static constexpr bool initialize_tree_after_construction = true;
    explicit QueryField(gf::StableId);
    void initialize_control_tree();
    void set_text(std::string);
    std::string_view text() const;
    swiftedit::SearchPattern pattern() const;
    void select(gf::Utf8Offset, gf::Utf8Offset);
    void toggle_slot(std::size_t);
    void on_paint(gf::Painter &, gf::Rect) override;
    void on_pointer(gf::PointerEvent &) override;
    void on_key(gf::KeyEvent &) override;
    void on_text_input(gf::TextInputEvent &) override;
    void on_focus_changed(bool) override;

protected:
    void on_dispose() noexcept override;

private:
    struct TextListener {
        std::weak_ptr<QueryField> owner{};
        void operator()(const std::string &) const;
    };
    void synchronize();
    std::size_t hit(double) const;
    std::shared_ptr<gf::TextBox> edit_{};
    gf::TextStore store_{};
    std::vector<swiftedit::SearchSlot> slots_{};
    std::vector<double> edges_{};
    std::vector<std::string> labels_{};
    gf::SubscriptionToken changed_{};
    std::optional<std::size_t> hovered_{};
    double scroll_{};
    bool focused_{}, dragging_{}, layout_dirty_{true};
};
} // namespace notepad
