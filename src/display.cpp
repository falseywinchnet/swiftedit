#include "display.hpp"
#include <algorithm>
#include <stdexcept>

namespace swiftedit {
namespace {
bool visible_control(char32_t scalar) {
    const bool control = scalar < 32 || (scalar >= 0x7f && scalar <= 0x9f) || scalar == 0xad ||
                         scalar == 0x61c || scalar == 0x200b || scalar == 0x200e ||
                         scalar == 0x200f || (scalar >= 0x202a && scalar <= 0x202e) ||
                         (scalar >= 0x2060 && scalar <= 0x206f) || scalar == 0xfeff;
    return control;
}
std::string hex_label(char32_t value, const char *prefix, std::size_t digits) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result(prefix);
    for (std::size_t index = digits; index > 0; --index) {
        const std::size_t shift = (index - 1) * 4;
        result += hex[(value >> shift) & 15];
    }
    result += ']';
    return result;
}
} // namespace
DisplayPage::DisplayPage(std::string_view source) : source_size_(source.size()) {
    if (source.size() > maximum_page)
        throw std::runtime_error("Display page exceeds 65536 source bytes.");
    text_.reserve(source.size());
    units_.reserve(source.size());
    for (std::size_t offset = 0; offset < source.size();) {
        std::size_t length = utf8_sequence_length(source, offset);
        const unsigned char first = static_cast<unsigned char>(source[offset]);
        DisplayUnit unit{{offset, length}, {text_.size(), 0}, DisplayKind::text};
        if (!length) {
            length = 1;
            unit.source.length = 1;
            unit.kind = DisplayKind::illegal_byte;
            const std::string label = hex_label(first, "[BYTE ", 2);
            text_.append(label);
        } else if (first == '\r' || first == '\n' || first == '\t') {
            // CRLF remains one selection unit, even though it has two bytes.
            if (first == '\r' && offset + 1 < source.size() && source[offset + 1] == '\n') {
                length = 2;
                unit.source.length = 2;
            }
            text_.append(source.substr(offset, length));
        } else {
            char32_t scalar = first;
            if (length > 1) {
                scalar = first & (length == 2 ? 31 : length == 3 ? 15 : 7);
                for (std::size_t index = 1; index < length; ++index) {
                    const unsigned char next = static_cast<unsigned char>(source[offset + index]);
                    scalar = (scalar << 6) | (next & 63);
                }
            }
            const bool control = visible_control(scalar);
            if (control) {
                unit.kind = DisplayKind::control;
                const std::string label = hex_label(scalar, "[U+", scalar > 0xffff ? 6 : 4);
                text_.append(label);
            } else
                text_.append(source.substr(offset, length));
        }
        unit.display.length = text_.size() - unit.display.offset;
        units_.push_back(unit);
        offset += length;
    }
}
SourceRange DisplayPage::source_range(SourceRange display) const {
    if (display.offset > text_.size() || display.length > text_.size() - display.offset)
        throw std::runtime_error("Display selection exceeds page.");
    const std::size_t end = display.offset + display.length;
    std::size_t first = source_size_, last = source_size_;
    bool found_first = display.offset == text_.size();
    bool found_last = end == text_.size();
    for (const DisplayUnit &unit : units_) {
        if (unit.display.offset == display.offset) {
            first = unit.source.offset;
            found_first = true;
        }
        if (unit.display.offset == end) {
            last = unit.source.offset;
            found_last = true;
        }
    }
    if (!found_first || !found_last)
        throw std::runtime_error("Select whole characters or control labels.");
    const SourceRange result{first, last - first};
    return result;
}
std::size_t DisplayPage::display_offset(std::size_t source_offset) const {
    if (source_offset == source_size_) {
        const std::size_t end = text_.size();
        return end;
    }
    for (const DisplayUnit &unit : units_)
        if (unit.source.offset == source_offset)
            return unit.display.offset;
    throw std::runtime_error("Source position splits a display unit.");
}
} // namespace swiftedit
