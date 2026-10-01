#pragma once
#include "csv.hpp"
#include <gui_forms/gui_forms.hpp>
#include <map>
#include <set>

namespace notepad {
namespace gf = gui_forms;
class CsvView final : public gf::Control {
public:
    static constexpr bool initialize_tree_after_construction = true;
    explicit CsvView(gf::StableId);
    void initialize_control_tree();
    void set_source(std::string_view);
    void set_context_command(const std::shared_ptr<gf::Command> &);
    void select_cell(swiftedit::CellAddress);
    void commit_cell(std::string_view);
    void convert_to_value();
    void clear_cells();
    [[nodiscard]] std::string copy_cells() const;
    gf::Event<const std::string &> &changed() { return changed_; }
    void arrange(gf::Rect) override;
    void on_paint(gf::Painter &, gf::Rect) override;
    void on_pointer(gf::PointerEvent &) override;
    void on_key(gf::KeyEvent &) override;
    const std::string &status() const { return status_; }
    swiftedit::CellAddress selected() const { return caret_; }
    gf::ContextMenu &context_menu() const { return *context_; }

protected:
    void on_dispose() noexcept override;

private:
    struct CommitListener {
        std::weak_ptr<CsvView> owner{};
        void operator()(const std::string &) const;
    };
    struct ScrollListener {
        std::weak_ptr<CsvView> owner{};
        void operator()(const double &) const;
    };
    struct CellDisplay {
        std::string text{}, detail{};
        std::set<std::pair<std::size_t, std::size_t>> visible_references{};
        bool formula{}, error{};
    };
    std::weak_ptr<CsvView> observe();
    void prepare_view();
    void update_field();
    void publish(std::string);
    std::optional<swiftedit::CellAddress> hit(gf::Point) const;
    std::unique_ptr<swiftedit::Csv> table_{};
    std::string source_{};
    std::shared_ptr<gf::TextBox> entry_{};
    std::shared_ptr<gf::VScrollBar> vertical_{};
    std::shared_ptr<gf::HScrollBar> horizontal_{};
    std::unique_ptr<gf::ContextMenu> context_{};
    std::unique_ptr<gf::ToolTip> tooltip_{};
    std::optional<std::pair<std::size_t, std::size_t>> hovered_{};
    std::vector<gf::SubscriptionToken> subscriptions_{};
    gf::Event<const std::string &> changed_{};
    std::map<std::pair<std::size_t, std::size_t>, CellDisplay> cells_{};
    swiftedit::CellAddress anchor_{}, caret_{};
    std::size_t columns_{}, top_{}, left_{}, visible_rows_{1}, visible_columns_{1};
    std::size_t cached_rows_{}, cached_columns_{};
    bool cells_dirty_{true};
    bool dragging_{};
    std::string status_{"Enter edits the selected cell. Delete clears the selected rectangle."};
};
} // namespace notepad
