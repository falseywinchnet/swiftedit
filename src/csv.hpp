#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace swiftedit {
struct Cell {
    std::string value{};
    std::size_t begin{}, end{};
};
struct CellAddress {
    std::size_t row{}, column{};
    bool operator==(const CellAddress &other) const {
        const bool equal = row == other.row && column == other.column;
        return equal;
    }
};
[[nodiscard]] CellAddress cell_address(std::string_view);
[[nodiscard]] std::string cell_name(CellAddress);
class Csv {
public:
    explicit Csv(std::string_view source);
    const Cell &cell(CellAddress) const;
    const std::vector<std::vector<Cell>> &rows() const { return rows_; }
    [[nodiscard]] std::string set(CellAddress, std::string_view) const;
    [[nodiscard]] std::string clear(CellAddress first, CellAddress last) const;

private:
    std::string source_{};
    std::vector<std::vector<Cell>> rows_{};
};
struct Calculation {
    std::string result{};
    std::vector<CellAddress> references{};
};
// Exact bounded rational arithmetic: never binary floating point. Intermediate
// overflow and non-terminating decimals are errors, not approximate results.
[[nodiscard]] Calculation calculate(const Csv &, std::string_view expression);
} // namespace swiftedit
