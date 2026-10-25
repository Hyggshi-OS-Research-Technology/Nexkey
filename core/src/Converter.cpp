// SPDX-License-Identifier: MIT
// NexKey - TCVN3, VIQR and Unicode NFD conversion.

#include "nexkey/Converter.hpp"

#include "nexkey/Grapheme.hpp"
#include "nexkey/Unicode.hpp"

#include <algorithm>
#include <array>
#include <optional>

namespace nexkey {

namespace {

constexpr char to_lower(char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

constexpr bool is_alpha(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

bool mark_fits(Mark mark, char32_t base) noexcept
{
    switch (mark) {
    case Mark::Circumflex:
        return base == U'a' || base == U'e' || base == U'o';
    case Mark::Breve:
        return base == U'a';
    case Mark::Horn:
        return base == U'o' || base == U'u';
    case Mark::Stroke:
        return base == U'd';
    case Mark::None:
        break;
    }
    return false;
}

// ---------------------------------------------------------------------------
// TCVN3 (ABC), a legacy single byte character set.
// ---------------------------------------------------------------------------

struct TcvnRow {
    char32_t base;
    Mark mark;
    std::array<unsigned char, 6> bytes; ///< None, acute, grave, hook, tilde, dot.
};

constexpr std::array<TcvnRow, 13> kTcvnRows{{
    {U'a', Mark::None, {0x61, 0xB8, 0xB5, 0xB6, 0xB7, 0xB9}},
    {U'a', Mark::Breve, {0xA8, 0xBE, 0xBB, 0xBC, 0xBD, 0xC6}},
    {U'a', Mark::Circumflex, {0xA9, 0xCA, 0xC7, 0xC8, 0xC9, 0xCB}},
    {U'e', Mark::None, {0x65, 0xD0, 0xCC, 0xCE, 0xCF, 0xD1}},
    {U'e', Mark::Circumflex, {0xAA, 0xD5, 0xD2, 0xD3, 0xD4, 0xD6}},
    {U'i', Mark::None, {0x69, 0xDD, 0xD7, 0xD8, 0xDC, 0xDE}},
    {U'o', Mark::None, {0x6F, 0xE3, 0xDF, 0xE1, 0xE2, 0xE4}},
    {U'o', Mark::Circumflex, {0xAB, 0xE8, 0xE5, 0xE6, 0xE7, 0xE9}},
    {U'o', Mark::Horn, {0xAC, 0xED, 0xEA, 0xEB, 0xEC, 0xEE}},
    {U'u', Mark::None, {0x75, 0xF3, 0xEF, 0xF1, 0xF2, 0xF4}},
    {U'u', Mark::Horn, {0xAD, 0xF8, 0xF5, 0xF6, 0xF7, 0xF9}},
    {U'y', Mark::None, {0x79, 0xFD, 0xFA, 0xFB, 0xFC, 0xFE}},
    {U'd', Mark::Stroke, {0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE}},
}};

std::string encode_tcvn3(std::u32string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const auto code_point : text) {
        if (code_point <= 0x7F) {
            out.push_back(static_cast<char>(code_point));
            continue;
        }
        const auto decomposed = decompose(code_point);
        if (!decomposed.has_value()) {
            unicode::append(out, code_point);
            continue;
        }
        const auto& letter = decomposed->letter;
        const auto row = std::ranges::find_if(kTcvnRows, [&letter](const TcvnRow& candidate) {
            return candidate.base == letter.base && candidate.mark == letter.mark;
        });
        if (row == kTcvnRows.end()) {
            unicode::append(out, code_point);
            continue;
        }
        const auto index = decomposed->tone == Tone::None
                               ? std::size_t{0}
                               : static_cast<std::size_t>(decomposed->tone);
        out.push_back(static_cast<char>((*row).bytes[index]));
    }
    return out;
}

std::u32string decode_tcvn3(std::string_view bytes)
{
    std::u32string out;
    out.reserve(bytes.size());
    for (const char raw : bytes) {
        const auto byte = static_cast<unsigned char>(raw);
        if (byte < 0x80) {
            out.push_back(static_cast<char32_t>(byte));
            continue;
        }
        std::optional<char32_t> decoded;
        for (const auto& row : kTcvnRows) {
            for (std::size_t index = 0; index < row.bytes.size(); ++index) {
                if (row.bytes[index] != byte) {
                    continue;
                }
                decoded = compose(Letter{row.base, row.mark, false}, static_cast<Tone>(index));
                break;
            }
            if (decoded.has_value()) {
                break;
            }
        }
        out.push_back(decoded.value_or(static_cast<char32_t>(byte)));
    }
    return out;
}

// ---------------------------------------------------------------------------
// VIQR, a pure ASCII notation.
// ---------------------------------------------------------------------------

constexpr char mark_char(Mark mark) noexcept
{
    switch (mark) {
    case Mark::Circumflex:
        return '^';
    case Mark::Breve:
        return '(';
    case Mark::Horn:
        return '+';
    case Mark::Stroke:
        return '-';
    case Mark::None:
        break;
    }
    return '\0';
}

constexpr char tone_char(Tone tone) noexcept
{
    switch (tone) {
    case Tone::Acute:
        return '\'';
    case Tone::Grave:
        return '`';
    case Tone::Hook:
        return '?';
    case Tone::Tilde:
        return '~';
    case Tone::Dot:
        return '.';
    case Tone::None:
        break;
    }
    return '\0';
}

constexpr std::optional<Mark> viqr_mark(char value) noexcept
{
    switch (value) {
    case '^':
        return Mark::Circumflex;
    case '(':
        return Mark::Breve;
    case '+':
        return Mark::Horn;
    case '-':
        return Mark::Stroke;
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Tone> viqr_tone(char value) noexcept
{
    switch (value) {
    case '\'':
        return Tone::Acute;
    case '`':
        return Tone::Grave;
    case '?':
        return Tone::Hook;
    case '~':
        return Tone::Tilde;
    case '.':
        return Tone::Dot;
    default:
        return std::nullopt;
    }
}

std::string encode_viqr(std::u32string_view text)
{
    std::string out;
    out.reserve(text.size() * 2);
    for (const auto code_point : text) {
        if (code_point <= 0x7F) {
            out.push_back(static_cast<char>(code_point));
            continue;
        }
        const auto decomposed = decompose(code_point);
        if (!decomposed.has_value()) {
            unicode::append(out, code_point);
            continue;
        }
        const auto& letter = decomposed->letter;
        out.push_back(static_cast<char>(letter.upper ? letter.base - 32 : letter.base));
        if (const auto mark = mark_char(letter.mark); mark != '\0') {
            out.push_back(mark);
        }
        if (const auto tone = tone_char(decomposed->tone); tone != '\0') {
            out.push_back(tone);
        }
    }
    return out;
}

std::u32string decode_viqr(std::string_view bytes)
{
    std::u32string out;
    out.reserve(bytes.size());

    std::size_t index = 0;
    while (index < bytes.size()) {
        const char value = bytes[index];
        if (!is_alpha(value)) {
            out.push_back(static_cast<char32_t>(static_cast<unsigned char>(value)));
            ++index;
            continue;
        }

        const char32_t base = static_cast<char32_t>(to_lower(value));
        const bool upper = value >= 'A' && value <= 'Z';
        Mark mark = Mark::None;
        Tone tone = Tone::None;

        std::size_t next = index + 1;
        bool progress = true;
        while (next < bytes.size() && progress) {
            progress = false;
            if (mark == Mark::None) {
                if (const auto candidate = viqr_mark(bytes[next]);
                    candidate.has_value() && mark_fits(*candidate, base)) {
                    mark = *candidate;
                    ++next;
                    progress = true;
                }
            }
            if (tone == Tone::None && is_vowel(base)) {
                if (const auto candidate = viqr_tone(bytes[next])) {
                    tone = *candidate;
                    ++next;
                    progress = true;
                }
            }
        }

        out.push_back(compose(Letter{base, mark, upper}, tone));
        index = next;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Unicode NFD.
// ---------------------------------------------------------------------------

constexpr char32_t kCombiningCircumflex = U'\u0302';
constexpr char32_t kCombiningBreve = U'\u0306';
constexpr char32_t kCombiningHorn = U'\u031B';
constexpr char32_t kCombiningAcute = U'\u0301';
constexpr char32_t kCombiningGrave = U'\u0300';
constexpr char32_t kCombiningHook = U'\u0309';
constexpr char32_t kCombiningTilde = U'\u0303';
constexpr char32_t kCombiningDot = U'\u0323';

constexpr std::optional<char32_t> combining_mark(Mark mark) noexcept
{
    switch (mark) {
    case Mark::Circumflex:
        return kCombiningCircumflex;
    case Mark::Breve:
        return kCombiningBreve;
    case Mark::Horn:
        return kCombiningHorn;
    default:
        return std::nullopt;
    }
}

constexpr std::optional<char32_t> combining_tone(Tone tone) noexcept
{
    switch (tone) {
    case Tone::Acute:
        return kCombiningAcute;
    case Tone::Grave:
        return kCombiningGrave;
    case Tone::Hook:
        return kCombiningHook;
    case Tone::Tilde:
        return kCombiningTilde;
    case Tone::Dot:
        return kCombiningDot;
    case Tone::None:
        return std::nullopt;
    }
    return std::nullopt;
}

constexpr std::optional<Mark> mark_of_combining(char32_t code_point) noexcept
{
    switch (code_point) {
    case kCombiningCircumflex:
        return Mark::Circumflex;
    case kCombiningBreve:
        return Mark::Breve;
    case kCombiningHorn:
        return Mark::Horn;
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Tone> tone_of_combining(char32_t code_point) noexcept
{
    switch (code_point) {
    case kCombiningAcute:
        return Tone::Acute;
    case kCombiningGrave:
        return Tone::Grave;
    case kCombiningHook:
        return Tone::Hook;
    case kCombiningTilde:
        return Tone::Tilde;
    case kCombiningDot:
        return Tone::Dot;
    default:
        return std::nullopt;
    }
}

std::string encode_nfd(std::u32string_view text)
{
    std::string out;
    out.reserve(text.size() * 3);
    for (const auto code_point : text) {
        if (code_point <= 0x7F) {
            out.push_back(static_cast<char>(code_point));
            continue;
        }
        const auto decomposed = decompose(code_point);
        if (!decomposed.has_value() || decomposed->letter.mark == Mark::Stroke) {
            unicode::append(out, code_point);
            continue;
        }
        const auto& letter = decomposed->letter;
        unicode::append(out, letter.upper ? letter.base - 32 : letter.base);
        if (const auto mark = combining_mark(letter.mark)) {
            unicode::append(out, *mark);
        }
        if (const auto tone = combining_tone(decomposed->tone)) {
            unicode::append(out, *tone);
        }
    }
    return out;
}

std::u32string decode_nfd(std::string_view bytes)
{
    const auto code_points = unicode::decode(bytes);
    std::u32string out;
    out.reserve(code_points.size());

    std::size_t index = 0;
    while (index < code_points.size()) {
        const auto code_point = code_points[index];
        const auto decomposed = decompose(code_point);
        if (!decomposed.has_value()) {
            out.push_back(code_point);
            ++index;
            continue;
        }

        Letter letter = decomposed->letter;
        Tone tone = decomposed->tone;
        std::size_t next = index + 1;
        while (next < code_points.size()) {
            if (const auto mark = mark_of_combining(code_points[next]);
                mark.has_value() && letter.mark == Mark::None) {
                letter.mark = *mark;
                ++next;
                continue;
            }
            if (const auto combining = tone_of_combining(code_points[next]);
                combining.has_value() && tone == Tone::None) {
                tone = *combining;
                ++next;
                continue;
            }
            break;
        }
        out.push_back(compose(letter, tone));
        index = next;
    }
    return out;
}

} // namespace

std::u32string Converter::decode(Encoding encoding, std::string_view bytes)
{
    switch (encoding) {
    case Encoding::Unicode:
        return unicode::decode(bytes);
    case Encoding::UnicodeDecomposed:
        return decode_nfd(bytes);
    case Encoding::Tcvn3:
        return decode_tcvn3(bytes);
    case Encoding::Viqr:
        return decode_viqr(bytes);
    }
    return unicode::decode(bytes);
}

std::string Converter::encode(Encoding encoding, std::u32string_view text)
{
    switch (encoding) {
    case Encoding::Unicode:
        return unicode::encode(text);
    case Encoding::UnicodeDecomposed:
        return encode_nfd(text);
    case Encoding::Tcvn3:
        return encode_tcvn3(text);
    case Encoding::Viqr:
        return encode_viqr(text);
    }
    return unicode::encode(text);
}

std::string Converter::convert(std::string_view input, Encoding from, Encoding to)
{
    if (from == to) {
        return std::string(input);
    }
    return encode(to, decode(from, input));
}

} // namespace nexkey
