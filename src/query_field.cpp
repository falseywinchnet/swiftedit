#include "query_field.hpp"
#include "display.hpp"
#include <algorithm>

namespace notepad {
QueryField::QueryField(gf::StableId id) : Control(std::move(id)) {
    set_focusable(true);
    set_accessible_name("Find what; right-click or Control-question to toggle a wildcard");
}
void QueryField::initialize_control_tree() {
    edit_ = gf::make_control<gf::TextBox>(gf::StableId("query.editing"));
    (*edit_).set_maximum_length(4096);
    (*edit_).set_visible(false);
    add_child(edit_);
    const std::shared_ptr<QueryField> self =
        std::static_pointer_cast<QueryField>(shared_from_this());
    changed_ = (*edit_).text_changed().subscribe(*this, TextListener{self});
}
void QueryField::on_dispose() noexcept {
    changed_.disconnect();
    (*edit_).on_focus_changed(false);
    Control::on_dispose();
}
void QueryField::TextListener::operator()(const std::string &) const {
    const std::shared_ptr<QueryField> self = owner.lock();
    if (self && (*self).is_alive())
        (*self).synchronize();
}
void QueryField::set_text(std::string value) {
    if (value.size() > 4096)
        throw std::runtime_error("Search text exceeds 4096 bytes.");
    (*edit_).set_text(std::move(value));
}
std::string_view QueryField::text() const { return store_.utf8(); }
void QueryField::synchronize() {
    gf::TextStore next((*edit_).text());
    std::vector<swiftedit::SearchSlot> slots{};
    if (!(*edit_).text().empty()) {
        const swiftedit::SearchPattern parsed((*edit_).text());
        slots = parsed.slots();
    }
    std::size_t prefix = 0;
    while (prefix < slots.size() && prefix < slots_.size() &&
           slots[prefix].literal == slots_[prefix].literal) {
        slots[prefix].wildcard = slots_[prefix].wildcard;
        ++prefix;
    }
    std::size_t old_end = slots_.size(), new_end = slots.size();
    while (old_end > prefix && new_end > prefix &&
           slots[new_end - 1].literal == slots_[old_end - 1].literal) {
        slots[new_end - 1].wildcard = slots_[old_end - 1].wildcard;
        --old_end;
        --new_end;
    }
    store_ = std::move(next);
    slots_ = std::move(slots);
    hovered_.reset();
    layout_dirty_ = true;
    invalidate(gf::Dirty::paint);
}
swiftedit::SearchPattern QueryField::pattern() const {
    swiftedit::SearchPattern result(store_.utf8());
    for (std::size_t index = 0; index < slots_.size(); ++index)
        if (slots_[index].wildcard)
            result.toggle(index);
    return result;
}
void QueryField::select(gf::Utf8Offset anchor, gf::Utf8Offset caret) {
    (*edit_).select(anchor, caret);
    invalidate(gf::Dirty::paint);
}
void QueryField::toggle_slot(std::size_t slot) {
    if (slot >= slots_.size())
        return;
    slots_[slot].wildcard = !slots_[slot].wildcard;
    layout_dirty_ = true;
    invalidate(gf::Dirty::paint);
}
std::size_t QueryField::hit(double x) const {
    const double position = x - 6 + scroll_;
    const std::vector<double>::const_iterator edge =
        std::upper_bound(edges_.begin(), edges_.end(), position);
    const std::size_t index = static_cast<std::size_t>(edge - edges_.begin());
    const std::size_t slot = index ? index - 1 : 0;
    return std::min(slot, slots_.size());
}
void QueryField::on_paint(gf::Painter &painter, gf::Rect) {
    const gf::Rect bounds = arranged_bounds();
    const gf::BasicControlStyle &style = effective_theme().basic_style();
    const gf::FontSpec font{gf::FontRole::content, 14, 400, false};
    if (layout_dirty_) {
        std::vector<double> next{0};
        std::vector<std::string> labels{};
        next.reserve(slots_.size() + 1);
        labels.reserve(slots_.size());
        for (const swiftedit::SearchSlot &slot : slots_) {
            const swiftedit::DisplayPage display(slot.literal);
            std::string label = slot.wildcard ? "\xe2\x80\xa2" : display.text();
            const gf::Size size = painter.measure_text_utf8(label, font);
            next.push_back(next.back() + std::max(12.0, size.width + 4));
            labels.push_back(std::move(label));
        }
        edges_ = std::move(next);
        labels_ = std::move(labels);
        layout_dirty_ = false;
    }
    const gf::TextSelection selection = (*edit_).selection();
    const std::size_t caret = store_.grapheme_index(selection.caret).value();
    const double width = std::max(1.0, bounds.width - 12);
    if (edges_[caret] < scroll_)
        scroll_ = edges_[caret];
    if (edges_[caret] > scroll_ + width)
        scroll_ = edges_[caret] - width;
    scroll_ = std::min(scroll_, std::max(0.0, edges_.back() - width));
    painter.fill_rect({0, 0, bounds.width, bounds.height}, style.paper);
    painter.stroke_rect({0, 0, bounds.width, bounds.height}, style.border, 1);
    painter.save();
    painter.clip_rect({2, 2, std::max(0.0, bounds.width - 4), std::max(0.0, bounds.height - 4)});
    for (std::size_t index = hit(0); index < slots_.size(); ++index) {
        const double x = 6 + edges_[index] - scroll_;
        if (x > bounds.width)
            break;
        const gf::Utf8Range range = store_.grapheme_range(gf::GraphemeIndex(index));
        const gf::Rect box{x, 3, edges_[index + 1] - edges_[index], bounds.height - 6};
        if (range.start.value() >= selection.start().value() &&
            range.end.value() <= selection.end().value())
            painter.fill_rect(box, style.face_light);
        if (hovered_ == index)
            painter.stroke_rect(box, style.border, 1);
        painter.draw_text_utf8({x + 2, 6}, labels_[index], font,
                               slots_[index].wildcard ? style.link : style.text);
    }
    if (focused_) {
        const double x = 6 + edges_[caret] - scroll_;
        painter.draw_line({x, 4}, {x, bounds.height - 4}, style.text, 1);
    }
    painter.restore();
}
void QueryField::on_focus_changed(bool focused) {
    focused_ = focused;
    (*edit_).on_focus_changed(focused);
    invalidate(gf::Dirty::paint);
}
void QueryField::on_key(gf::KeyEvent &event) {
    if (event.phase != gf::EventPhase::target || event.action != gf::KeyAction::down)
        return;
    // USB HID slash/question key, as normalized by the public host contract.
    if (event.physical_key == 0x38 && gf::has_modifier(event.modifiers, gf::Modifier::control) &&
        gf::has_modifier(event.modifiers, gf::Modifier::shift)) {
        const std::size_t slot = store_.grapheme_index((*edit_).selection().caret).value();
        toggle_slot(slot);
        event.handled = true;
        return;
    }
    (*edit_).on_key(event);
    invalidate(gf::Dirty::paint);
}
void QueryField::on_text_input(gf::TextInputEvent &event) {
    (*edit_).on_text_input(event);
    invalidate(gf::Dirty::paint);
}
void QueryField::on_pointer(gf::PointerEvent &event) {
    if (event.phase != gf::EventPhase::target || layout_dirty_)
        return;
    const double x = event.position.x - absolute_bounds().x;
    const std::size_t slot = hit(x);
    const gf::Utf8Offset at = store_.utf8_offset(gf::GraphemeIndex(slot));
    if (event.action == gf::PointerAction::down) {
        if (window())
            (*window()).request_focus(shared_from_this());
        if (event.button == gf::PointerButton::secondary) {
            select(at, at);
            toggle_slot(slot);
        } else if (event.button == gf::PointerButton::primary) {
            const gf::Utf8Offset anchor = gf::has_modifier(event.modifiers, gf::Modifier::shift)
                                              ? (*edit_).selection().anchor
                                              : at;
            select(anchor, at);
            dragging_ = true;
            set_pointer_capture(true);
        }
        event.handled = true;
    } else if (event.action == gf::PointerAction::up) {
        dragging_ = false;
        set_pointer_capture(false);
    } else if (event.action == gf::PointerAction::move) {
        hovered_ = slot < slots_.size() ? std::optional<std::size_t>(slot) : std::nullopt;
        if (dragging_)
            select((*edit_).selection().anchor, at);
    } else if (event.action == gf::PointerAction::leave)
        hovered_.reset();
    invalidate(gf::Dirty::paint);
}
} // namespace notepad
