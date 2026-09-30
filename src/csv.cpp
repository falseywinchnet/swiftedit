#include "csv.hpp"
#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace swiftedit {
CellAddress cell_address(std::string_view name) {
    std::size_t i = 0, col = 0, row = 0;
    while (i < name.size() && name[i] >= 'A' && name[i] <= 'Z') {
        if (col > 100000)
            throw std::runtime_error("Column address is too large.");
        col = col * 26 + name[i++] - 'A' + 1;
    }
    if (!col || i == name.size() || name[i] == '0')
        throw std::runtime_error("Expected a cell address such as A1.");
    while (i < name.size() && name[i] >= '0' && name[i] <= '9') {
        if (row > 100000)
            throw std::runtime_error("Row address is too large.");
        row = row * 10 + name[i++] - '0';
    }
    if (i != name.size() || !row)
        throw std::runtime_error("Invalid cell address.");
    return {row - 1, col - 1};
}
std::string cell_name(CellAddress p) {
    std::string s;
    auto n = p.column + 1;
    while (n) {
        s.insert(s.begin(), char('A' + (n - 1) % 26));
        n = (n - 1) / 26;
    }
    return s + std::to_string(p.row + 1);
}
Csv::Csv(std::string_view source) : source_(source) {
    std::size_t i = 0, count = 0;
    std::vector<Cell> row;
    for (;;) {
        if (++count > 100000)
            throw std::runtime_error("CSV table view is limited to 100000 cells.");
        Cell c;
        c.begin = i;
        if (i < source.size() && source[i] == '"') {
            ++i;
            bool closed = false;
            while (i < source.size()) {
                if (source[i] == '"') {
                    ++i;
                    if (i < source.size() && source[i] == '"') {
                        c.value += '"';
                        ++i;
                    } else {
                        closed = true;
                        break;
                    }
                } else
                    c.value += source[i++];
            }
            if (!closed)
                throw std::runtime_error("Unclosed quoted CSV field.");
            if (i < source.size() && source[i] != ',' && source[i] != '\r' && source[i] != '\n')
                throw std::runtime_error("Unexpected text after quoted CSV field.");
        } else {
            while (i < source.size() && source[i] != ',' && source[i] != '\r' &&
                   source[i] != '\n') {
                if (source[i] == '"')
                    throw std::runtime_error("Quote inside an unquoted CSV field.");
                c.value += source[i++];
            }
        }
        c.end = i;
        row.push_back(std::move(c));
        if (i == source.size()) {
            rows_.push_back(std::move(row));
            break;
        }
        if (source[i] == ',') {
            ++i;
            continue;
        }
        if (source[i++] == '\r' && i < source.size() && source[i] == '\n')
            ++i;
        rows_.push_back(std::move(row));
        row.clear();
        if (i == source.size())
            break;
    }
}
const Cell &Csv::cell(CellAddress p) const {
    if (p.row >= rows_.size() || p.column >= rows_[p.row].size())
        throw std::runtime_error("Cell " + cell_name(p) + " does not exist.");
    return rows_[p.row][p.column];
}
std::string Csv::set(CellAddress p, std::string_view value) const {
    auto &c = cell(p);
    std::string encoded;
    if (value.find_first_of(",\"\r\n") != value.npos) {
        encoded += '"';
        for (char ch : value) {
            encoded += ch;
            if (ch == '"')
                encoded += '"';
        }
        encoded += '"';
    } else
        encoded = value;
    auto result = source_;
    result.replace(c.begin, c.end - c.begin, encoded);
    return result;
}
std::string Csv::clear(CellAddress first, CellAddress last) const {
    if (first.row > last.row || first.column > last.column)
        throw std::runtime_error("Reversed CSV selection.");
    cell(first);
    cell(last);
    std::vector<Cell> targets;
    for (auto r = first.row; r <= last.row; ++r)
        for (auto c = first.column; c <= last.column; ++c)
            targets.push_back(cell({r, c}));
    auto result = source_;
    for (auto i = targets.rbegin(); i != targets.rend(); ++i)
        result.erase(i->begin, i->end - i->begin);
    return result;
}
namespace {
using Integer = std::int64_t;
constexpr Integer limit = std::numeric_limits<Integer>::max() / 10;
Integer magnitude(Integer n) { return n < 0 ? -n : n; }
Integer mul(Integer a, Integer b) {
    if (b && magnitude(a) > limit / magnitude(b))
        throw std::runtime_error("Exact arithmetic capacity exceeded.");
    return a * b;
}
Integer add(Integer a, Integer b) {
    if ((b > 0 && a > limit - b) || (b < 0 && a < -limit - b))
        throw std::runtime_error("Exact arithmetic capacity exceeded.");
    return a + b;
}
struct Number {
    Integer n{}, d{1};
    Number(Integer a = 0, Integer b = 1) : n(a), d(b) {
        if (!d)
            throw std::runtime_error("Division by zero.");
        if (d < 0) {
            n = -n;
            d = -d;
        }
        auto g = std::gcd(magnitude(n), d);
        n /= g;
        d /= g;
    }
};
Number plus(Number a, Number b) {
    auto g = std::gcd(a.d, b.d);
    return {add(mul(a.n, b.d / g), mul(b.n, a.d / g)), mul(a.d, b.d / g)};
}
Number times(Number a, Number b) {
    auto g = std::gcd(magnitude(a.n), b.d), h = std::gcd(magnitude(b.n), a.d);
    return {mul(a.n / g, b.n / h), mul(a.d / h, b.d / g)};
}
Number divide(Number a, Number b) {
    if (!b.n)
        throw std::runtime_error("Division by zero.");
    return times(a, {b.d, b.n});
}
bool less(Number a, Number b) { return plus(a, {-b.n, b.d}).n < 0; }
Number numeric(std::string_view s) {
    if (s.empty())
        throw std::runtime_error("Blank is not numeric.");
    bool negative = false;
    std::size_t i = 0;
    if (s[i] == '-' || s[i] == '+') {
        negative = s[i] == '-';
        ++i;
    }
    Integer n = 0, d = 1;
    bool point = false, digit = false;
    for (; i < s.size(); ++i) {
        auto c = s[i];
        if (c == '.' && !point) {
            point = true;
            continue;
        }
        if (c < '0' || c > '9')
            throw std::runtime_error("Non-numeric content; no coercion is allowed.");
        digit = true;
        n = add(mul(n, 10), c - '0');
        if (point)
            d = mul(d, 10);
    }
    if (!digit)
        throw std::runtime_error("Non-numeric content.");
    return {negative ? -n : n, d};
}
std::string decimal(Number a) {
    auto den = a.d;
    while (den % 2 == 0)
        den /= 2;
    while (den % 5 == 0)
        den /= 5;
    if (den != 1)
        throw std::runtime_error("Rounding required: use ROUND(expression, decimal_places).");
    auto n = magnitude(a.n);
    std::string out = (a.n < 0 ? "-" : "") + std::to_string(n / a.d);
    n %= a.d;
    if (n)
        out += '.';
    while (n) {
        n *= 10;
        out += char('0' + n / a.d);
        n %= a.d;
    }
    return out;
}
class Parser {
  public:
    Parser(const Csv &table, std::string_view expression) : table_(table), s_(expression) {
        if (s_.size() > 4096)
            throw std::runtime_error("Expression exceeds 4096 bytes.");
        if (s_.starts_with('='))
            ++at_;
    }
    Calculation run() {
        auto n = expression();
        space();
        if (at_ != s_.size())
            throw std::runtime_error("Unexpected formula text.");
        return {decimal(n), references_};
    }

  private:
    const Csv &table_;
    std::string_view s_;
    std::size_t at_{}, depth_{};
    std::vector<CellAddress> references_;
    void space() {
        while (at_ < s_.size() && (s_[at_] == ' ' || s_[at_] == '\t'))
            ++at_;
    }
    bool take(char c) {
        space();
        if (at_ < s_.size() && s_[at_] == c) {
            ++at_;
            return true;
        }
        return false;
    }
    void need(char c) {
        if (!take(c))
            throw std::runtime_error(std::string("Expected '") + c + "'.");
    }
    Number expression() {
        auto n = term();
        while (true) {
            if (take('+'))
                n = plus(n, term());
            else if (take('-')) {
                auto rhs = term();
                n = plus(n, {-rhs.n, rhs.d});
            } else
                return n;
        }
    }
    Number term() {
        auto n = atom();
        while (true) {
            if (take('*'))
                n = times(n, atom());
            else if (take('/'))
                n = divide(n, atom());
            else
                return n;
        }
    }
    Number reference(CellAddress p) {
        if (references_.size() >= 100000)
            throw std::runtime_error("Too many referenced cells.");
        references_.push_back(p);
        try {
            return numeric(table_.cell(p).value);
        } catch (const std::exception &e) {
            throw std::runtime_error(cell_name(p) + ": " + e.what());
        }
    }
    std::string name() {
        space();
        auto start = at_;
        while (at_ < s_.size() && s_[at_] >= 'A' && s_[at_] <= 'Z')
            ++at_;
        return std::string(s_.substr(start, at_ - start));
    }
    CellAddress address() {
        space();
        auto start = at_;
        name();
        while (at_ < s_.size() && s_[at_] >= '0' && s_[at_] <= '9')
            ++at_;
        return cell_address(s_.substr(start, at_ - start));
    }
    std::vector<Number> arguments() {
        std::vector<Number> values;
        if (take(')'))
            return values;
        do {
            space();
            auto saved = at_;
            bool range = false;
            if (at_ < s_.size() && s_[at_] >= 'A' && s_[at_] <= 'Z') {
                name();
                auto digits = at_;
                while (at_ < s_.size() && s_[at_] >= '0' && s_[at_] <= '9')
                    ++at_;
                if (at_ > digits && take(':')) {
                    at_ = saved;
                    auto first = address();
                    need(':');
                    auto last = address();
                    if (first.row > last.row || first.column > last.column)
                        throw std::runtime_error("Reversed cell range.");
                    table_.cell(first);
                    table_.cell(last);
                    for (auto r = first.row; r <= last.row; ++r)
                        for (auto c = first.column; c <= last.column; ++c)
                            values.push_back(reference({r, c}));
                    range = true;
                }
            }
            if (!range) {
                at_ = saved;
                values.push_back(expression());
            }
        } while (take(','));
        need(')');
        return values;
    }
    Number function(const std::string &name, std::vector<Number> v) {
        auto count = [&](std::size_t n) {
            if (v.size() != n)
                throw std::runtime_error(name + ": wrong argument count.");
        };
        if (name == "ROUND") {
            count(2);
            if (v[1].d != 1 || v[1].n < 0 || v[1].n > 15)
                throw std::runtime_error("ROUND places must be an integer from 0 to 15.");
            Integer scale = 1;
            for (Integer i = 0; i < v[1].n; ++i)
                scale = mul(scale, 10);
            auto a = times(v[0], {scale});
            auto n = magnitude(a.n), q = n / a.d, r = n % a.d;
            if (r >= a.d - r)
                q = add(q, 1);
            return {a.n < 0 ? -q : q, scale};
        }
        if (name == "ABS") {
            count(1);
            return {magnitude(v[0].n), v[0].d};
        }
        if (name == "FLOOR" || name == "CEILING") {
            count(1);
            auto a = v[0];
            auto n = a.n / a.d;
            if (a.n % a.d) {
                if (name == "FLOOR" && a.n < 0)
                    --n;
                if (name == "CEILING" && a.n > 0)
                    ++n;
            }
            return {n};
        }
        if (name == "MOD") {
            count(2);
            auto ratio = divide(v[0], v[1]);
            auto q = ratio.n / ratio.d;
            if (ratio.n < 0 && ratio.n % ratio.d)
                --q;
            auto p = times({q}, v[1]);
            return plus(v[0], {-p.n, p.d});
        }
        if (v.empty())
            throw std::runtime_error(name + ": expected numeric operands.");
        if (name == "COUNT")
            return {static_cast<Integer>(v.size())};
        if (name == "SUM" || name == "AVERAGE") {
            Number n;
            for (auto a : v)
                n = plus(n, a);
            return name == "AVERAGE" ? divide(n, {static_cast<Integer>(v.size())}) : n;
        }
        if (name == "MIN" || name == "MAX") {
            auto n = v.front();
            for (auto a : v)
                if (name == "MIN" ? less(a, n) : less(n, a))
                    n = a;
            return n;
        }
        throw std::runtime_error("Unsupported function: " + name);
    }
    Number atom() {
        if (++depth_ > 32)
            throw std::runtime_error("Formula nesting exceeds 32 levels.");
        struct Depth {
            std::size_t &d;
            ~Depth() { --d; }
        } depth{depth_};
        space();
        if (take('+'))
            return atom();
        if (take('-')) {
            auto n = atom();
            return {-n.n, n.d};
        }
        if (take('(')) {
            auto n = expression();
            need(')');
            return n;
        }
        auto start = at_;
        if (at_ < s_.size() && s_[at_] >= 'A' && s_[at_] <= 'Z') {
            auto id = name();
            if (take('('))
                return function(id, arguments());
            at_ = start;
            return reference(address());
        }
        while (at_ < s_.size() && ((s_[at_] >= '0' && s_[at_] <= '9') || s_[at_] == '.'))
            ++at_;
        return numeric(s_.substr(start, at_ - start));
    }
};
} // namespace
Calculation calculate(const Csv &table, std::string_view expression) {
    return Parser(table, expression).run();
}
} // namespace swiftedit
