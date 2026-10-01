#include "csv_view.hpp"
#include "display.hpp"
#include <algorithm>

namespace notepad {
namespace {
constexpr double row_height = 28, column_width = 144, header_width = 56, grid_top = 68;
std::string cell_label(std::string_view value) {
    std::size_t end = 0;
    while (end < value.size() && end < 240) {
        const std::size_t length = swiftedit::utf8_sequence_length(value, end);
        end += length ? length : 1;
    }
    const swiftedit::DisplayPage display(value.substr(0, end));
    std::string result = display.text();
    for (char &character : result)
        if (character == '\r' || character == '\n' || character == '\t')
            character = ' ';
    if (end < value.size())
        result += "...";
    return result;
}
} // namespace
CsvView::CsvView(gf::StableId id) : Control(std::move(id)) { set_focusable(true); }
std::weak_ptr<CsvView> CsvView::observe() {
    const std::shared_ptr<CsvView> self = std::static_pointer_cast<CsvView>(shared_from_this());
    const std::weak_ptr<CsvView> observer = self;
    return observer;
}
void CsvView::CommitListener::operator()(const std::string &value) const {
    const std::shared_ptr<CsvView> self = owner.lock();
    if (self && (*self).is_alive())
        (*self).commit_cell(value);
}
void CsvView::ScrollListener::operator()(const double &) const {
    const std::shared_ptr<CsvView> self = owner.lock();
    if (self && (*self).is_alive())
        (*self).prepare_view();
}
void CsvView::initialize_control_tree() {
    entry_ = gf::make_control<gf::TextBox>(gf::StableId("csv.entry"));
    (*entry_).set_accessible_name("Cell source or formula; Enter to apply");
    (*entry_).set_maximum_length(4096);
    add_child(entry_);
    vertical_ = gf::make_control<gf::VScrollBar>(gf::StableId("csv.vertical"));
    horizontal_ = gf::make_control<gf::HScrollBar>(gf::StableId("csv.horizontal"));
    add_child(vertical_);
    add_child(horizontal_);
    subscriptions_.push_back((*entry_).committed().subscribe(*this, CommitListener{observe()}));
    subscriptions_.push_back(
        (*vertical_).value_changed().subscribe(*this, ScrollListener{observe()}));
    subscriptions_.push_back(
        (*horizontal_).value_changed().subscribe(*this, ScrollListener{observe()}));
    context_ = std::make_unique<gf::ContextMenu>("csv.context");
}
void CsvView::on_dispose() noexcept {
    subscriptions_.clear();
    tooltip_.reset();
    if (context_)
        (*context_).close();
    Control::on_dispose();
}
void CsvView::set_context_command(const std::shared_ptr<gf::Command> &command) {
    (*context_).set_items(
        {{"csv.convert-value", gf::MenuItemKind::command, command, "Convert to Value"}});
}
void CsvView::set_source(std::string_view source) {
    if (table_ && source == source_)
        return;
    std::unique_ptr<swiftedit::Csv> next = std::make_unique<swiftedit::Csv>(source);
    std::string retained(source);
    std::size_t columns = 0;
    for (const std::vector<swiftedit::Cell> &row : (*next).rows())
        columns = std::max(columns, row.size());
    table_ = std::move(next);
    source_ = std::move(retained);
    columns_ = columns;
    caret_.row = std::min(caret_.row, (*table_).rows().size() - 1);
    caret_.column = std::min(caret_.column, (*table_).rows()[caret_.row].size() - 1);
    anchor_ = caret_;
    (*vertical_)
        .set_range(0, static_cast<double>(std::max<std::size_t>(1, (*table_).rows().size() - 1)));
    (*horizontal_).set_range(0, static_cast<double>(std::max<std::size_t>(1, columns_ - 1)));
    (*vertical_).set_enabled((*table_).rows().size() > 1);
    (*horizontal_).set_enabled(columns_ > 1);
    if ((*table_).rows().size() == 1)
        (*vertical_).set_value(0);
    if (columns_ == 1)
        (*horizontal_).set_value(0);
    update_field();
    prepare_view();
}
void CsvView::arrange(gf::Rect bounds) {
    arrange_self(bounds);
    set_child_layout(entry_,
                     {header_width, 4, std::max(0.0, bounds.width - header_width - 20), 30});
    set_child_layout(vertical_, {std::max(0.0, bounds.width - 18), grid_top, 18,
                                 std::max(0.0, bounds.height - grid_top - 46)});
    set_child_layout(horizontal_, {header_width, std::max(0.0, bounds.height - 46),
                                   std::max(0.0, bounds.width - header_width - 18), 18});
    visible_rows_ = std::clamp(
        static_cast<std::size_t>(std::max(1.0, (bounds.height - grid_top - 46) / row_height)),
        std::size_t(1), std::size_t(128));
    visible_columns_ = std::clamp(
        static_cast<std::size_t>(std::max(1.0, (bounds.width - header_width - 18) / column_width)) +
            1,
        std::size_t(1), std::size_t(32));
    (*vertical_).set_large_change(static_cast<double>(visible_rows_));
    (*horizontal_).set_large_change(static_cast<double>(visible_columns_));
    prepare_view();
}
void CsvView::prepare_view() {
    if (!table_)
        return;
    top_ = static_cast<std::size_t>((*vertical_).value());
    left_ = static_cast<std::size_t>((*horizontal_).value());
    cells_.clear();
    hovered_.reset();
    const std::size_t row_end = std::min((*table_).rows().size(), top_ + visible_rows_);
    for (std::size_t row = top_; row < row_end; ++row) {
        const std::size_t column_end =
            std::min((*table_).rows()[row].size(), left_ + visible_columns_);
        for (std::size_t column = left_; column < column_end; ++column) {
            const swiftedit::CellAddress address{row, column};
            const swiftedit::Cell &cell = (*table_).cell(address);
            CellDisplay display{};
            display.formula = cell.value.starts_with('=');
            try {
                const swiftedit::Calculation value = swiftedit::calculate_cell(*table_, address);
                display.text = cell_label(value.result);
                display.detail = display.formula ? cell.value + " | References:" : "";
                std::size_t shown = 0;
                for (const swiftedit::CellAddress reference : value.references) {
                    if (reference.row >= top_ && reference.row < top_ + visible_rows_ &&
                        reference.column >= left_ && reference.column < left_ + visible_columns_)
                        display.visible_references.emplace(reference.row, reference.column);
                    if (shown < 16) {
                        const std::string name = swiftedit::cell_name(reference);
                        display.detail += " " + name;
                    } else if (shown == 16)
                        display.detail += " ...";
                    ++shown;
                }
            } catch (const std::exception &failure) {
                display.text = "#ERROR";
                display.detail = failure.what();
                display.error = true;
            }
            cells_.emplace(std::pair<std::size_t, std::size_t>{row, column}, std::move(display));
        }
    }
    invalidate(gf::Dirty::paint);
}
void CsvView::update_field() {
    if (!table_)
        return;
    (*entry_).set_text((*table_).cell(caret_).value);
}
void CsvView::select_cell(swiftedit::CellAddress address) {
    if (!table_)
        return;
    static_cast<void>((*table_).cell(address));
    caret_ = address;
    anchor_ = address;
    update_field();
    invalidate(gf::Dirty::paint);
}
void CsvView::publish(std::string source) { changed_.emit(source); }
void CsvView::commit_cell(std::string_view value) {
    if (!table_)
        return;
    try {
        std::string source = (*table_).set(caret_, value);
        publish(std::move(source));
        status_ = "Cell stored. Formula source is preserved; the grid displays its result.";
    } catch (const std::exception &failure) {
        status_ = failure.what();
    }
    invalidate(gf::Dirty::paint);
}
void CsvView::convert_to_value() {
    if (!table_)
        return;
    try {
        std::string source = swiftedit::convert_to_value(*table_, caret_);
        publish(std::move(source));
        status_ = "Converted formula to value. Undo restores the formula.";
    } catch (const std::exception &failure) {
        status_ = failure.what();
    }
    invalidate(gf::Dirty::paint);
}
void CsvView::clear_cells() {
    if (!table_)
        return;
    try {
        const swiftedit::CellAddress first{std::min(anchor_.row, caret_.row),
                                           std::min(anchor_.column, caret_.column)};
        const swiftedit::CellAddress last{std::max(anchor_.row, caret_.row),
                                          std::max(anchor_.column, caret_.column)};
        std::string source = (*table_).clear(first, last);
        publish(std::move(source));
        status_ = "Cell contents cleared; rows and columns remain in place.";
    } catch (const std::exception &failure) {
        status_ = failure.what();
    }
    invalidate(gf::Dirty::paint);
}
std::string CsvView::copy_cells() const {
    std::string result{};
    if (!table_)
        return result;
    const swiftedit::CellAddress first{std::min(anchor_.row, caret_.row),
                                       std::min(anchor_.column, caret_.column)};
    const swiftedit::CellAddress last{std::max(anchor_.row, caret_.row),
                                      std::max(anchor_.column, caret_.column)};
    for (std::size_t row = first.row; row <= last.row; ++row) {
        if (row != first.row)
            result += "\r\n";
        for (std::size_t column = first.column; column <= last.column; ++column) {
            if (column != first.column)
                result += '\t';
            const swiftedit::Cell &cell = (*table_).cell({row, column});
            result += cell.value;
        }
    }
    return result;
}
std::optional<swiftedit::CellAddress> CsvView::hit(gf::Point position) const {
    const gf::Rect bounds = absolute_bounds();
    const double x = position.x - bounds.x - header_width;
    const double y = position.y - bounds.y - grid_top;
    if (!table_ || x < 0 || y < 0 || x >= bounds.width - header_width - 18 ||
        y >= bounds.height - grid_top - 46)
        return std::nullopt;
    const swiftedit::CellAddress address{top_ + static_cast<std::size_t>(y / row_height),
                                         left_ + static_cast<std::size_t>(x / column_width)};
    if (address.row >= (*table_).rows().size() ||
        address.column >= (*table_).rows()[address.row].size())
        return std::nullopt;
    const std::optional<swiftedit::CellAddress> result = address;
    return result;
}
void CsvView::on_pointer(gf::PointerEvent &event) {
    if (event.phase != gf::EventPhase::target)
        return;
    if (event.action == gf::PointerAction::leave) {
        hovered_.reset();
        if (tooltip_)
            (*tooltip_).hide();
        invalidate(gf::Dirty::paint);
        return;
    }
    if (event.action == gf::PointerAction::wheel) {
        (*vertical_).increment(event.wheel_delta.y < 0 ? 3 : -3);
        event.handled = true;
        return;
    }
    if (event.action == gf::PointerAction::up) {
        dragging_ = false;
        set_pointer_capture(false);
    }
    const std::optional<swiftedit::CellAddress> cell = hit(event.position);
    if (!cell)
        return;
    if (event.action == gf::PointerAction::down) {
        select_cell(*cell);
        if (window())
            (*window()).request_focus(shared_from_this());
        if (event.button == gf::PointerButton::secondary)
            (*context_).show(shared_from_this(), event.position);
        else if (event.button == gf::PointerButton::primary) {
            dragging_ = true;
            set_pointer_capture(true);
            if (event.click_count == 2 && window())
                (*window()).request_focus(entry_);
        }
        event.handled = true;
    } else if (event.action == gf::PointerAction::move) {
        if (dragging_) {
            caret_ = *cell;
            update_field();
        }
        const std::map<std::pair<std::size_t, std::size_t>, CellDisplay>::const_iterator found =
            cells_.find({(*cell).row, (*cell).column});
        if (found != cells_.end()) {
            const std::pair<std::size_t, std::size_t> key{(*cell).row, (*cell).column};
            status_ = (*found).second.detail;
            if (hovered_ != key && window()) {
                if (!tooltip_)
                    tooltip_ = std::make_unique<gf::ToolTip>(*window());
                (*tooltip_).set_tool_tip(shared_from_this(), status_);
                if (status_.empty())
                    (*tooltip_).hide();
                else
                    (*tooltip_).show(shared_from_this());
            }
            hovered_ = key;
        }
        invalidate(gf::Dirty::paint);
    }
}
void CsvView::on_key(gf::KeyEvent &event) {
    if (event.phase != gf::EventPhase::target || event.action != gf::KeyAction::down || !table_)
        return;
    if (event.physical_key == gf::PhysicalKey::delete_forward) {
        clear_cells();
        event.handled = true;
    } else if (event.physical_key == gf::PhysicalKey::enter && window()) {
        (*window()).request_focus(entry_);
        (*entry_).select_all();
        event.handled = true;
    } else {
        swiftedit::CellAddress next = caret_;
        if (event.physical_key == gf::PhysicalKey::left && next.column)
            --next.column;
        else if (event.physical_key == gf::PhysicalKey::right &&
                 next.column + 1 < (*table_).rows()[next.row].size())
            ++next.column;
        else if (event.physical_key == gf::PhysicalKey::up && next.row)
            --next.row;
        else if (event.physical_key == gf::PhysicalKey::down &&
                 next.row + 1 < (*table_).rows().size())
            ++next.row;
        else
            return;
        next.column = std::min(next.column, (*table_).rows()[next.row].size() - 1);
        caret_ = next;
        if (!gf::has_modifier(event.modifiers, gf::Modifier::shift))
            anchor_ = next;
        if (next.row < top_)
            (*vertical_).set_value(static_cast<double>(next.row));
        else if (next.row >= top_ + visible_rows_)
            (*vertical_).set_value(static_cast<double>(next.row - visible_rows_ + 1));
        if (next.column < left_)
            (*horizontal_).set_value(static_cast<double>(next.column));
        else if (next.column >= left_ + visible_columns_)
            (*horizontal_).set_value(static_cast<double>(next.column - visible_columns_ + 1));
        update_field();
        invalidate(gf::Dirty::paint);
        event.handled = true;
    }
}
void CsvView::on_paint(gf::Painter &painter, gf::Rect) {
    const gf::Rect bounds = arranged_bounds();
    const gf::BasicControlStyle &style = effective_theme().basic_style();
    const gf::FontSpec font{gf::FontRole::content, 13, 400, false};
    painter.fill_rect({0, 0, bounds.width, bounds.height}, style.paper);
    const std::string address = swiftedit::cell_name(caret_);
    painter.draw_text_utf8({4, 10}, address, font, style.text);
    const CellDisplay *hover = nullptr;
    if (hovered_) {
        const std::map<std::pair<std::size_t, std::size_t>, CellDisplay>::const_iterator found =
            cells_.find(*hovered_);
        if (found != cells_.end())
            hover = &(*found).second;
    }
    for (const std::pair<const std::pair<std::size_t, std::size_t>, CellDisplay> &entry : cells_) {
        const std::size_t row = entry.first.first, column = entry.first.second;
        const gf::Rect rect{header_width + static_cast<double>(column - left_) * column_width,
                            grid_top + static_cast<double>(row - top_) * row_height, column_width,
                            row_height};
        const bool selected = row >= std::min(anchor_.row, caret_.row) &&
                              row <= std::max(anchor_.row, caret_.row) &&
                              column >= std::min(anchor_.column, caret_.column) &&
                              column <= std::max(anchor_.column, caret_.column);
        if (selected)
            painter.fill_rect(rect, style.accent_light);
        painter.stroke_rect(rect, style.border, 1);
        if (hover && (*hover).visible_references.contains(entry.first))
            painter.stroke_rect(rect, style.accent, 2);
        painter.save();
        painter.clip_rect(rect);
        painter.draw_text_utf8({rect.x + 5, rect.y + 6}, entry.second.text, font, style.text);
        if (entry.second.formula)
            for (int stripe = 0; stripe < 5; ++stripe)
                painter.draw_line({rect.x + rect.width - 6 + stripe, rect.y + 1 + stripe},
                                  {rect.x + rect.width - 1, rect.y + 1 + stripe}, style.accent, 1);
        painter.restore();
    }
    for (std::size_t index = 0; index < visible_columns_ && left_ + index < columns_; ++index) {
        std::string column = swiftedit::cell_name({0, left_ + index});
        column.pop_back();
        painter.draw_text_utf8({header_width + static_cast<double>(index) * column_width + 5, 46},
                               column, font, style.text);
    }
    if (table_)
        for (std::size_t index = 0; index < visible_rows_ && top_ + index < (*table_).rows().size();
             ++index) {
            const std::string row = std::to_string(top_ + index + 1);
            painter.draw_text_utf8({4, grid_top + static_cast<double>(index) * row_height + 6}, row,
                                   font, style.text);
        }
    painter.draw_text_utf8({4, std::max(0.0, bounds.height - 22)}, status_, font, style.text);
}
} // namespace notepad
