#include "markdown_view.hpp"
#include "display.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace notepad {
namespace {
constexpr double ruler_height = 28, page_margin = 24;
std::string inert_text(std::string_view source) {
    std::string result{};
    std::size_t begin = 0;
    while (begin < source.size()) {
        std::size_t end = begin;
        while (end < source.size() && end - begin < 60000) {
            const std::size_t length = swiftedit::utf8_sequence_length(source, end);
            end += length ? length : 1;
        }
        const swiftedit::DisplayPage page(source.substr(begin, end - begin));
        result += page.text();
        begin = end;
    }
    return result;
}
} // namespace
MarkdownView::MarkdownView(gf::StableId id) : Control(std::move(id)) { set_focusable(true); }
bool MarkdownView::RunOrder::operator()(const Run &left, const Run &right) const {
    const bool before = left.bounds.y < right.bounds.y;
    return before;
}
void MarkdownView::ScrollListener::operator()(const double &) const {
    const std::shared_ptr<MarkdownView> self = owner.lock();
    if (self && (*self).is_alive()) {
        (*self).clear_hover();
        (*self).invalidate(gf::Dirty::paint);
    }
}
void MarkdownView::initialize_control_tree() {
    vertical_ = gf::make_control<gf::VScrollBar>(gf::StableId("markdown.vertical"));
    horizontal_ = gf::make_control<gf::HScrollBar>(gf::StableId("markdown.horizontal"));
    add_child(vertical_);
    add_child(horizontal_);
    const std::shared_ptr<MarkdownView> self =
        std::static_pointer_cast<MarkdownView>(shared_from_this());
    const std::weak_ptr<MarkdownView> observer = self;
    subscriptions_.push_back(
        (*vertical_).value_changed().subscribe(*this, ScrollListener{observer}));
    subscriptions_.push_back(
        (*horizontal_).value_changed().subscribe(*this, ScrollListener{observer}));
    (*vertical_).set_small_change(32);
    (*horizontal_).set_small_change(32);
}
void MarkdownView::on_dispose() noexcept {
    subscriptions_.clear();
    tooltip_.reset();
    Control::on_dispose();
}
void MarkdownView::set_source(std::string_view source) {
    if (source == source_)
        return;
    std::vector<swiftedit::MarkdownBlock> parsed = swiftedit::parse_markdown(source);
    std::string retained(source);
    blocks_ = std::move(parsed);
    source_ = std::move(retained);
    layout_dirty_ = true;
    layout_error_.clear();
    clear_hover();
    invalidate(gf::Dirty::paint);
}
void MarkdownView::arrange(gf::Rect bounds) {
    clear_hover();
    arrange_self(bounds);
    set_child_layout(vertical_, {std::max(0.0, bounds.width - 18), ruler_height, 18,
                                 std::max(0.0, bounds.height - ruler_height - 18)});
    set_child_layout(horizontal_,
                     {0, std::max(0.0, bounds.height - 18), std::max(0.0, bounds.width - 18), 18});
    (*vertical_).set_large_change(std::max(32.0, bounds.height - ruler_height - 18));
    (*horizontal_).set_large_change(std::max(32.0, bounds.width - 18));
    if (layout_width_ != bounds.width)
        layout_dirty_ = true;
    if (!layout_dirty_)
        update_scroll_ranges();
}
void MarkdownView::layout(gf::Painter &painter, double width) {
    std::vector<Run> next{};
    next.reserve(std::min<std::size_t>(source_.size() / 4 + 1, 250000));
    std::size_t retained_bytes = 0;
    constexpr std::size_t storage_limit = 32 * 1024 * 1024;
    double y = page_margin, maximum_x = width - 18, maximum_height = 32;
    for (const swiftedit::MarkdownBlock &block : blocks_) {
        if (next.size() >= 250000 || block.columns > 250000 - next.size())
            throw std::runtime_error("Markdown layout exceeds display run budget.");
        const double indent = static_cast<double>(std::min<std::size_t>(block.indent, 12)) * 20;
        const double start = page_margin + indent + (block.quoted ? 16 : 0);
        const double available = std::max(80.0, width - start - page_margin - 18);
        if (block.kind == swiftedit::MarkdownKind::rule) {
            Run rule{};
            rule.bounds = {start, y + 8, available, 1};
            rule.rule = true;
            next.push_back(std::move(rule));
            y += 24;
            continue;
        }
        const bool table = block.kind == swiftedit::MarkdownKind::table_row;
        const std::size_t columns = table ? std::max<std::size_t>(1, block.columns) : 1;
        const double column_width =
            table ? std::max(96.0, available / static_cast<double>(columns)) : available;
        std::vector<double> x(columns, 0), row_y(columns, y);
        std::vector<double> line_height(columns, 20);
        for (std::size_t column = 0; column < columns; ++column)
            x[column] = start + static_cast<double>(column) * column_width + (table ? 6 : 0);
        for (const swiftedit::MarkdownSpan &span : block.spans) {
            const std::size_t column = table ? std::min(span.column, columns - 1) : 0;
            const double left =
                start + static_cast<double>(column) * column_width + (table ? 6 : 0);
            const double right = left + column_width - (table ? 12 : 0);
            gf::FontSpec font{gf::FontRole::content, 14, 400, span.italic};
            if (span.bold || block.header || block.kind == swiftedit::MarkdownKind::heading)
                font.weight = 700;
            if (span.code || block.kind == swiftedit::MarkdownKind::code) {
                font.role = gf::FontRole::monospace;
                font.size = 13;
            }
            if (block.kind == swiftedit::MarkdownKind::heading)
                font.size = 30 - static_cast<double>(std::min<std::size_t>(block.level, 6)) * 2;
            const std::string safe = inert_text(span.text);
            for (std::size_t offset = 0; offset < safe.size();) {
                if (safe[offset] == '\n' || safe[offset] == '\r') {
                    x[column] = left;
                    row_y[column] += line_height[column];
                    line_height[column] = std::max(20.0, font.size * 1.5);
                    if (safe[offset] == '\r' && offset + 1 < safe.size() &&
                        safe[offset + 1] == '\n')
                        ++offset;
                    ++offset;
                    continue;
                }
                std::size_t end = offset + 1;
                if (safe[offset] != ' ' && safe[offset] != '\t') {
                    const std::size_t separator = safe.find_first_of(" \t\r\n", offset);
                    end = separator == safe.npos ? safe.size() : separator;
                }
                const std::string word =
                    safe[offset] == '\t' ? "    " : safe.substr(offset, end - offset);
                if (word.size() > storage_limit - retained_bytes)
                    throw std::runtime_error("Markdown layout exceeds display storage budget.");
                const std::size_t text_total = retained_bytes + word.size();
                if (span.url.size() > storage_limit - text_total)
                    throw std::runtime_error("Markdown layout exceeds display storage budget.");
                const gf::ResolvedTextLayout metrics = painter.resolve_text_layout_utf8(word, font);
                const gf::Size measured = metrics.logical_size;
                const double height = std::max(20.0, std::max(measured.height,
                    metrics.ascent + metrics.descent) + 4);
                if (block.kind != swiftedit::MarkdownKind::code && x[column] > left &&
                    x[column] + measured.width > right) {
                    x[column] = left;
                    row_y[column] += line_height[column];
                    line_height[column] = height;
                }
                line_height[column] = std::max(line_height[column], height);
                Run run{};
                run.bounds = {x[column], row_y[column], measured.width, height};
                run.font = font;
                run.baseline = 2 + metrics.ascent;
                run.underline = run.baseline + std::max(1.0, metrics.descent * 0.5);
                run.strike_y = run.baseline - metrics.ascent * 0.35;
                run.text = word;
                run.url = span.url;
                run.strike = span.strike;
                run.code = span.code || block.kind == swiftedit::MarkdownKind::code;
                next.push_back(std::move(run));
                retained_bytes = text_total + span.url.size();
                if (next.size() > 250000)
                    throw std::runtime_error("Markdown layout exceeds 250000 runs.");
                x[column] += measured.width;
                maximum_x = std::max(maximum_x, x[column] + page_margin);
                offset = end;
            }
        }
        double bottom = y + 20;
        for (std::size_t column = 0; column < columns; ++column)
            bottom = std::max(bottom, row_y[column] + line_height[column]);
        if (table) {
            for (std::size_t column = 0; column < columns; ++column) {
                Run border{};
                border.bounds = {start + static_cast<double>(column) * column_width, y,
                                 column_width, bottom - y + 4};
                border.border = true;
                next.push_back(std::move(border));
            }
            maximum_x = std::max(maximum_x,
                                 start + static_cast<double>(columns) * column_width + page_margin);
        }
        if (block.quoted) {
            Run quote{};
            quote.bounds = {start - 12, y, 2, bottom - y};
            quote.rule = true;
            next.push_back(std::move(quote));
        }
        y = bottom + (table ? 4 : 12);
    }
    for (const Run &run : next)
        maximum_height = std::max(maximum_height, run.bounds.height);
    std::stable_sort(next.begin(), next.end(), RunOrder{});
    const gf::FontSpec ruler_font{gf::FontRole::content, 11, 400, false};
    const gf::ResolvedTextLayout ruler_metrics = painter.resolve_text_layout_utf8("0", ruler_font);
    runs_ = std::move(next);
    ruler_baseline_ = 3 + ruler_metrics.ascent;
    content_height_ = y + page_margin;
    content_width_ = maximum_x;
    maximum_run_height_ = maximum_height;
    layout_width_ = width;
    layout_dirty_ = false;
    update_scroll_ranges();
}
void MarkdownView::update_scroll_ranges() {
    const gf::Rect bounds = arranged_bounds();
    const double vertical_extent =
        std::max(0.0, content_height_ - bounds.height + ruler_height + 18);
    const double horizontal_extent = std::max(0.0, content_width_ - bounds.width + 18);
    (*vertical_).set_range(0, std::max(1.0, vertical_extent));
    (*horizontal_).set_range(0, std::max(1.0, horizontal_extent));
    (*vertical_).set_enabled(vertical_extent > 0);
    (*horizontal_).set_enabled(horizontal_extent > 0);
    if (!vertical_extent)
        (*vertical_).set_value(0);
    if (!horizontal_extent)
        (*horizontal_).set_value(0);
}
void MarkdownView::on_paint(gf::Painter &painter, gf::Rect) {
    const gf::Rect bounds = arranged_bounds();
    const gf::BasicControlStyle &style = effective_theme().basic_style();
    painter.fill_rect({0, 0, bounds.width, bounds.height}, style.paper);
    if (layout_dirty_) {
        try {
            layout(painter, bounds.width);
            layout_error_.clear();
        } catch (const std::exception &failure) {
            layout_error_ = failure.what();
            layout_dirty_ = false;
        }
    }
    if (!layout_error_.empty()) {
        const gf::FontSpec font{gf::FontRole::content, 14, 400, false};
        painter.draw_text_utf8({24, 48}, layout_error_, font, style.text);
        return;
    }
    const double top = (*vertical_).value(), left = (*horizontal_).value();
    painter.save();
    painter.clip_rect({0, ruler_height, std::max(0.0, bounds.width - 18),
                       std::max(0.0, bounds.height - ruler_height - 18)});
    painter.translate({-left, ruler_height - top});
    Run beginning{};
    beginning.bounds.y = top - maximum_run_height_;
    std::vector<Run>::const_iterator run =
        std::lower_bound(runs_.begin(), runs_.end(), beginning, RunOrder{});
    for (; run != runs_.end() && (*run).bounds.y <= top + bounds.height; ++run) {
        const Run &item = *run;
        if (item.border) {
            painter.stroke_rect(item.bounds, style.border, 1);
            continue;
        }
        if (item.rule) {
            painter.fill_rect(item.bounds, style.border);
            continue;
        }
        if (item.code)
            painter.fill_rect(item.bounds, style.face_light);
        const gf::Color color = item.url.empty() ? style.text : style.link;
        painter.draw_text_utf8({item.bounds.x, item.bounds.y + item.baseline}, item.text, item.font, color);
        if (item.strike || !item.url.empty()) {
            const double baseline = item.bounds.y + (item.strike ? item.strike_y : item.underline);
            painter.draw_line({item.bounds.x, baseline},
                              {item.bounds.x + item.bounds.width, baseline}, color, 1);
        }
    }
    painter.restore();
    painter.fill_rect({0, 0, std::max(0.0, bounds.width - 18), ruler_height}, style.face_light);
    const gf::FontSpec ruler_font{gf::FontRole::content, 11, 400, false};
    const double first_tick = std::ceil(left / 48) * 48;
    for (double tick = first_tick; tick < left + bounds.width - 18; tick += 48) {
        const double position = tick - left;
        painter.draw_line({position, 20}, {position, ruler_height}, style.border, 1);
        const std::string label = std::to_string(static_cast<unsigned>(tick / 48));
        painter.draw_text_utf8({position + 3, ruler_baseline_}, label, ruler_font, style.disabled_text);
    }
}
void MarkdownView::clear_hover() {
    hovered_url_.clear();
    if (tooltip_)
        (*tooltip_).hide();
}
void MarkdownView::on_key(gf::KeyEvent &event) {
    if (event.phase != gf::EventPhase::target || event.action != gf::KeyAction::down ||
        layout_dirty_ || !layout_error_.empty())
        return;
    const bool plain = event.modifiers == gf::Modifier::none;
    const bool document = event.modifiers == gf::Modifier::control || event.modifiers == gf::Modifier::meta;
    const gf::Rect bounds = arranged_bounds();
    const double page = std::max(32.0, bounds.height - ruler_height - 18);
    if (plain && event.physical_key == gf::PhysicalKey::up) {
        if ((*vertical_).enabled()) (*vertical_).increment(-32);
    } else if (plain && event.physical_key == gf::PhysicalKey::down) {
        if ((*vertical_).enabled()) (*vertical_).increment(32);
    } else if (plain && event.physical_key == gf::PhysicalKey::page_up) {
        if ((*vertical_).enabled()) (*vertical_).increment(-page);
    } else if (plain && event.physical_key == gf::PhysicalKey::page_down) {
        if ((*vertical_).enabled()) (*vertical_).increment(page);
    } else if ((plain || document) && event.physical_key == gf::PhysicalKey::home) {
        (*vertical_).set_value(0);
    } else if ((plain || document) && event.physical_key == gf::PhysicalKey::end) {
        if ((*vertical_).enabled())
            (*vertical_).set_value(std::max(0.0, content_height_ - bounds.height + ruler_height + 18));
    } else if (plain && event.physical_key == gf::PhysicalKey::left) {
        if ((*horizontal_).enabled()) (*horizontal_).increment(-32);
    } else if (plain && event.physical_key == gf::PhysicalKey::right) {
        if ((*horizontal_).enabled()) (*horizontal_).increment(32);
    } else {
        return;
    }
    event.handled = true;
}
void MarkdownView::on_pointer(gf::PointerEvent &event) {
    if (event.phase != gf::EventPhase::target)
        return;
    if (event.action == gf::PointerAction::wheel) {
        const gf::Rect bounds = arranged_bounds();
        const double extent = content_height_ - bounds.height + ruler_height + 18;
        if (!layout_dirty_ && layout_error_.empty() && extent > 0 && event.wheel_delta.y != 0)
            (*vertical_).increment(event.wheel_delta.y < 0 ? 64 : -64);
        event.handled = true;
        return;
    }
    if (event.action == gf::PointerAction::down) {
        if (window()) (*window()).request_focus(shared_from_this());
        event.handled = true; // Links are intentionally inert, including file: URLs.
        return;
    }
    if (event.action != gf::PointerAction::move && event.action != gf::PointerAction::leave)
        return;
    const gf::Rect bounds = absolute_bounds();
    const gf::Rect content{bounds.x, bounds.y + ruler_height,
                           std::max(0.0, bounds.width - 18),
                           std::max(0.0, bounds.height - ruler_height - 18)};
    const gf::Point point{event.position.x - bounds.x + (*horizontal_).value(),
                          event.position.y - bounds.y - ruler_height + (*vertical_).value()};
    std::string url{};
    if (event.action == gf::PointerAction::move && content.contains(event.position) &&
        !layout_dirty_ && layout_error_.empty()) {
        Run beginning{};
        beginning.bounds.y = point.y - maximum_run_height_;
        std::vector<Run>::const_iterator run =
            std::lower_bound(runs_.begin(), runs_.end(), beginning, RunOrder{});
        for (; run != runs_.end() && (*run).bounds.y <= point.y; ++run)
            if (!(*run).url.empty() && (*run).bounds.contains(point)) {
                url = (*run).url;
                break;
            }
    }
    if (url == hovered_url_ || !window())
        return;
    hovered_url_ = url;
    if (!tooltip_)
        tooltip_ = std::make_unique<gf::ToolTip>(*window());
    (*tooltip_).set_tool_tip(shared_from_this(), url);
    if (url.empty())
        (*tooltip_).hide();
    else
        (*tooltip_).show(shared_from_this());
}
} // namespace notepad
