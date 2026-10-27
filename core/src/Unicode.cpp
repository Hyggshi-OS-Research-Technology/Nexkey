// SPDX-License-Identifier: MIT
// NexKey - strict UTF-8 decoding and text measuring.

#include "nexkey/Unicode.hpp"

namespace nexkey::unicode {

namespace {

constexpr bool is_continuation(unsigned char byte) noexcept
{
    return (byte & 0xC0U) == 0x80U;
}

/// Length announced by a lead byte, or zero for the bytes that can never start
/// a well formed sequence: continuation bytes, the overlong leads C0 and C1,
/// and F5..FF.
constexpr std::size_t sequence_length(unsigned char lead) noexcept
{
    if (lead < 0x80U) {
        return 1;
    }
    if (lead < 0xC2U) {
        return 0;
    }
    if (lead < 0xE0U) {
        return 2;
    }
    if (lead < 0xF0U) {
        return 3;
    }
    if (lead < 0xF5U) {
        return 4;
    }
    return 0;
}

/// Whether the second byte keeps the sequence in shortest form and inside the
/// Unicode range (Unicode 15.0, table 3-7). Every other lead only needs a plain
/// continuation byte.
constexpr bool second_byte_fits(unsigned char lead, unsigned char byte) noexcept
{
    switch (lead) {
    case 0xE0U: // overlong below U+0800
        return byte >= 0xA0U && byte <= 0xBFU;
    case 0xEDU: // surrogates U+D800..U+DFFF
        return byte >= 0x80U && byte <= 0x9FU;
    case 0xF0U: // overlong below U+10000
        return byte >= 0x90U && byte <= 0xBFU;
    case 0xF4U: // above U+10FFFF
        return byte >= 0x80U && byte <= 0x8FU;
    default:
        return is_continuation(byte);
    }
}

struct Decoded {
    char32_t code_point{kReplacementCharacter};
    std::size_t length{1};
    bool valid{false};
};

/// Decode the sequence that starts at `index`.
///
/// An invalid sequence reports the length of its maximal subpart, the bytes a
/// decoder may consume before it has to emit U+FFFD (Unicode 15.0, table 3-8).
/// The length is never zero, so every caller makes progress:
///   E1 80 41  ->  U+FFFD (E1 80) + 'A'
///   ED A0 80  ->  U+FFFD (ED) + U+FFFD (A0) + U+FFFD (80)
constexpr Decoded decode_at(std::string_view text, std::size_t index) noexcept
{
    const auto lead = static_cast<unsigned char>(text[index]);
    const std::size_t needed = sequence_length(lead);
    if (needed == 0) {
        return Decoded{kReplacementCharacter, 1, false};
    }
    if (needed == 1) {
        return Decoded{static_cast<char32_t>(lead), 1, true};
    }

    char32_t code_point = lead & (needed == 2 ? 0x1FU : needed == 3 ? 0x0FU : 0x07U);
    std::size_t offset = 1;
    for (; offset < needed; ++offset) {
        if (index + offset >= text.size()) {
            break; // truncated at the end of the string
        }
        const auto byte = static_cast<unsigned char>(text[index + offset]);
        if (offset == 1 ? !second_byte_fits(lead, byte) : !is_continuation(byte)) {
            break;
        }
        code_point = (code_point << 6U) | (byte & 0x3FU);
    }
    if (offset < needed) {
        return Decoded{kReplacementCharacter, offset, false};
    }
    return Decoded{code_point, needed, true};
}

/// Byte offset of the code point in front of `end`, or zero at the start.
/// Walking backwards only needs to skip continuation bytes, because the byte
/// that is not a continuation byte is the lead of the sequence.
constexpr std::size_t previous_code_point(std::string_view text, std::size_t end) noexcept
{
    std::size_t start = end - 1;
    while (start > 0 && is_continuation(static_cast<unsigned char>(text[start]))) {
        --start;
    }
    return start;
}

} // namespace

void append(std::string& out, char32_t code_point)
{
    if (!is_scalar_value(code_point)) {
        code_point = kReplacementCharacter;
    }
    if (code_point <= 0x7F) {
        out.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
        out.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else if (code_point <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    }
}

std::string encode(std::u32string_view text)
{
    std::string out;
    out.reserve(text.size() * 3);
    for (const char32_t code_point : text) {
        append(out, code_point);
    }
    return out;
}

std::u32string decode(std::string_view text)
{
    std::u32string out;
    out.reserve(text.size());

    std::size_t index = 0;
    while (index < text.size()) {
        const auto decoded = decode_at(text, index);
        out.push_back(decoded.code_point);
        index += decoded.length;
    }
    return out;
}

bool is_valid(std::string_view text) noexcept
{
    std::size_t index = 0;
    while (index < text.size()) {
        const auto decoded = decode_at(text, index);
        if (!decoded.valid) {
            return false;
        }
        index += decoded.length;
    }
    return true;
}

std::size_t count_code_points(std::string_view text) noexcept
{
    std::size_t count = 0;
    std::size_t index = 0;
    while (index < text.size()) {
        index += decode_at(text, index).length;
        ++count;
    }
    return count;
}

std::size_t prefix_length(std::string_view text, std::size_t count) noexcept
{
    std::size_t index = 0;
    std::size_t seen = 0;
    while (index < text.size() && seen < count) {
        index += decode_at(text, index).length;
        ++seen;
    }
    return index;
}

std::size_t count_graphemes(std::string_view text) noexcept
{
    std::size_t clusters = 0;
    std::size_t index = 0;
    while (index < text.size()) {
        const auto decoded = decode_at(text, index);
        // A leading combining mark is a cluster of its own, like UAX #29 says.
        if (clusters == 0 || !is_extending(decoded.code_point)) {
            ++clusters;
        }
        index += decoded.length;
    }
    return clusters;
}

std::size_t grapheme_boundary(std::string_view text, std::size_t clusters) noexcept
{
    if (clusters == 0) {
        return text.size();
    }

    std::size_t end = text.size();
    std::size_t seen = 0;
    while (end > 0 && seen < clusters) {
        const auto start = previous_code_point(text, end);
        if (!is_extending(decode_at(text, start).code_point)) {
            ++seen; // this code point starts the cluster that ends at `end`
        }
        end = start;
    }
    return end;
}

Prefix common_prefix(std::string_view a, std::string_view b) noexcept
{
    Prefix prefix;
    while (prefix.bytes < a.size() && prefix.bytes < b.size()) {
        const auto left = decode_at(a, prefix.bytes);
        const auto right = decode_at(b, prefix.bytes);
        if (left.length != right.length ||
            a.substr(prefix.bytes, left.length) != b.substr(prefix.bytes, right.length)) {
            break;
        }
        prefix.bytes += left.length;
        ++prefix.code_points;
        if (prefix.code_points == 1 || !is_extending(left.code_point)) {
            ++prefix.graphemes;
        }
    }
    return prefix;
}

} // namespace nexkey::unicode
