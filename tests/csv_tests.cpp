#include "csv.hpp"
#include <iostream>
#include <stdexcept>
void check(bool b, const char *s) {
    if (!b)
        throw std::runtime_error(s);
}
template <class F> void refuses(F f, std::string_view fragment = "") {
    try {
        f();
    } catch (const std::exception &e) {
        check(std::string_view(e.what()).find(fragment) != std::string_view::npos,
              "Wrong refusal reason");
        return;
    }
    throw std::runtime_error("Expected refusal");
}
int main() {
    try {
        using namespace swiftedit;
        Csv csv("\"a,b\",\"quoted \"\"word\"\"\",\"line\r\nbreak\"\r\n1,2,3\n4,,6");
        check(csv.rows().size() == 3 && csv.cell({0, 2}).value == "line\r\nbreak",
              "embedded newlines");
        check(csv.cell({0, 1}).value == "quoted \"word\"", "escaped quotes");
        auto changed = csv.set({1, 1}, "x,y");
        check(changed == "\"a,b\",\"quoted \"\"word\"\"\",\"line\r\nbreak\"\r\n1,\"x,y\",3\n4,,6",
              "localized serialization preserves unaffected source");
        auto cleared = Csv("1,2,3\r\n4,5,6\n").clear({0, 1}, {1, 2});
        check(cleared == "1,,\r\n4,,\n", "clear preserves delimiters rows and line endings");
        check(Csv("a,b,").rows()[0].size() == 3, "trailing empty field");
        check(Csv("").rows()[0].size() == 1, "empty document cell");
        refuses([] { Csv("\"bad"); });
        refuses([] { Csv("\"bad\"tail"); });
        refuses([] { Csv("b\"ad"); });
        check(cell_address("AA12") == CellAddress{11, 26} && cell_name({11, 26}) == "AA12",
              "addresses");
        refuses([] { cell_address("A0"); });
        Csv nums("0.1,0.2,3\r\n4,5,6\r\ntext,,=SUM(A1:B1)");
        auto calc = [&](std::string_view s) { return calculate(nums, s).result; };
        check(calc("=A1+B1") == "0.3", "decimal exactness");
        check(calc("10/4") == "2.5", "finite division");
        refuses([&] { calc("10/3"); }, "Rounding required");
        check(calc("ROUND(10/3,2)") == "3.33", "explicit rounding");
        check(calc("ROUND(-1.235,2)") == "-1.24", "half away from zero");
        check(calc("SUM(A1:B2,C1)") == "12.3", "rectangle and comma list");
        check(calc("SUM(A1 : B1)") == "0.3", "spaced range");
        check(calc("AVERAGE(A2:C2)") == "5", "average");
        check(calc("MIN(A2:C2)") == "4" && calc("MAX(A2:C2)") == "6", "min max");
        check(calc("COUNT(A1:C2)") == "6", "strict numeric count");
        check(calc("ABS(-3.5)") == "3.5", "absolute");
        check(calc("FLOOR(-1.2)") == "-2" && calc("CEILING(-1.2)") == "-1",
              "negative rounding directions");
        check(calc("MOD(-5,3)") == "1", "floor-based remainder");
        refuses([&] { calc("SUM(A1:A3)"); }, "A3");
        refuses([&] { calc("SUM(B1:B3)"); }, "B3");
        refuses([&] { calc("C3"); }, "C3");
        refuses([&] { calc("1/0"); }, "zero");
        refuses([&] { calc("999999999999999999*99"); }, "capacity");
        refuses([&] { calc("RAND()"); });
        refuses([&] { calc("SUM(C2:A1)"); }, "Reversed");
        auto refs = calculate(nums, "SUM(A1:B1,C2)").references;
        check(refs == std::vector<CellAddress>{{0, 0}, {0, 1}, {1, 2}},
              "references available for hover highlighting");
        std::cout << "CSV tests passed: lossless local edits, quoting, clear rectangle, exact "
                     "decimal arithmetic, strict errors, rounding and reference metadata.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
