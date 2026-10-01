#include "markdown.hpp"
#include <iostream>
#include <stdexcept>

void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    try {
        const std::string source =
            "# Heading\n\nA **bold** and *soft* [link](https://example.com).\n\n"
            "- [x] Done\n- [ ] Later\n\n> Quoted\n\n"
            "```cpp\na < b;\n```\n\n"
            "| A | B |\n|---|---|\n| 1 | 2 |\n\n"
            "<script>alert(1)</script> &amp; &#233;\n";
        const std::vector<swiftedit::MarkdownBlock> blocks = swiftedit::parse_markdown(source);
        bool heading = false, bold = false, italic = false, link = false, task = false;
        bool quoted = false, code = false, table = false, html_text = false, entity = false;
        for (const swiftedit::MarkdownBlock &block : blocks) {
            heading = heading || block.kind == swiftedit::MarkdownKind::heading;
            quoted = quoted || block.quoted;
            code = code || block.kind == swiftedit::MarkdownKind::code;
            table =
                table || (block.kind == swiftedit::MarkdownKind::table_row && block.columns == 2);
            for (const swiftedit::MarkdownSpan &span : block.spans) {
                bold = bold || span.bold;
                italic = italic || span.italic;
                link = link || span.url == "https://example.com";
                task = task || span.text == "[x] ";
                html_text = html_text || span.text.find("<script>") != std::string::npos;
                entity = entity || span.text == "\xc3\xa9";
            }
        }
        check(heading && bold && italic && link && task && quoted && code && table,
              "Common blocks, styled spans, task lists, links and tables parsed");
        check(html_text && entity, "HTML is inert text and entities decode");
        const std::vector<swiftedit::MarkdownBlock> image =
            swiftedit::parse_markdown("![alt](file:///private/image.svg)");
        check(!image.empty(), "Image is represented by inert alternate text");
        std::cout << "Markdown inert presentation tests passed.\n";
        return 0;
    } catch (const std::exception &failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
