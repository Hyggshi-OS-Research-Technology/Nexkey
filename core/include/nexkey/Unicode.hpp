// SPDX-License-Identifier: MIT
// NexKey - UTF-8 / UTF-32 helpers.
//
// This header is the only place where NexKey turns bytes into text. It follows
// the well formedness rules of the Unicode Standard (chapter 3.9) instead of
// the looser "first byte tells the length" decoder: overlong forms, UTF-16
// surrogates and code points above U+10FFFF are not text and are reported as
// U+FFFD. That matters for the frontends, because they compare the document
// with what they wrote and a decoder that accepts anything would hide the
// corruption instead of showing it.
//
// The measuring helpers (code points, common prefix, grapheme clusters) are the
// primitives the commit path needs: a character set such as Unicode NFD stores
// one Vietnamese letter as up to three code points, so "delete one character"
// has to be counted in clusters, not in bytes and not in code points.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nexkey::unicode {

inline constexpr char32_t kReplacementCharacter = U'\uFFFD';

/// True for the code points that exist as text: everything up to U+10FFFF
/// except the surrogate range U+D800..U+DFFF.
[[nodiscard]] constexpr bool is_scalar_value(char32_t code_point) noexcept
{
    return code_point <= 0x10FFFF && (code_point < 0xD800 || code_point > 0xDFFF);
}

/// Decode UTF-8.
///
/// Every malformed sequence is replaced by exactly one U+FFFD per maximal
/// subpart, so `decode(text).size()` is the number of code points the text
/// stands for and is never larger than `text.size()`. A byte order mark is kept
/// as U+FEFF; callers that do not want it drop it themselves.
[[nodiscard]] std::u32string decode(std::string_view text);

/// True when `text` is well formed UTF-8.
[[nodiscard]] bool is_valid(std::string_view text) noexcept;

/// Encode a single code point as UTF-8 and append it to `out`.
///
/// Code points that UTF-8 cannot represent (surrogates and anything above
/// U+10FFFF) are written as U+FFFD, so `out` always stays valid UTF-8.
void append(std::string& out, char32_t code_point);

/// Encode a whole UTF-32 string as UTF-8.
[[nodiscard]] std::string encode(std::u32string_view text);

/// Number of bytes `append` writes for `code_point`, including the three bytes
/// of U+FFFD for code points that are not scalar values.
[[nodiscard]] constexpr std::size_t encoded_length(char32_t code_point) noexcept
{
    if (!is_scalar_value(code_point)) {
        return 3;
    }
    if (code_point <= 0x7F) {
        return 1;
    }
    if (code_point <= 0x7FF) {
        return 2;
    }
    if (code_point <= 0xFFFF) {
        return 3;
    }
    return 4;
}

/// True for ASCII digits, letters and symbols that an IME may consume.
[[nodiscard]] constexpr bool is_ascii(char32_t code_point) noexcept
{
    return code_point <= 0x7F;
}

/// True for the code points that attach to the character in front of them:
/// combining marks, variation selectors and emoji modifiers.
///
/// This is the subset of the Unicode grapheme cluster rules NexKey needs.
/// Sequences that need full tables (CR LF, the emoji ZWJ rules) are treated as
/// separate characters, which is why the helpers below talk about clusters and
/// not about "extended grapheme clusters".
[[nodiscard]] constexpr bool is_extending(char32_t code_point) noexcept
{
    return (code_point >= 0x0300 && code_point <= 0x036F) ||   // combining diacritics
           (code_point >= 0x0483 && code_point <= 0x0489) ||   // Cyrillic combining
           (code_point >= 0x1AB0 && code_point <= 0x1AFF) ||   // combining extended
           (code_point >= 0x1DC0 && code_point <= 0x1DFF) ||   // combining supplement
           (code_point >= 0x20D0 && code_point <= 0x20FF) ||   // combining for symbols
           (code_point >= 0xFE00 && code_point <= 0xFE0F) ||   // variation selectors
           (code_point >= 0xFE20 && code_point <= 0xFE2F) ||   // combining half marks
           (code_point >= 0x1F3FB && code_point <= 0x1F3FF) || // emoji skin tones
           (code_point >= 0xE0100 && code_point <= 0xE01EF);   // variation selectors, part 2
}

/// Number of code points in `text`. Each malformed subsequence counts as one,
/// matching `decode(text).size()`.
[[nodiscard]] std::size_t count_code_points(std::string_view text) noexcept;

/// Byte length of the first `count` code points of `text`, clamped to the end of
/// the string. The result is always a code point boundary.
[[nodiscard]] std::size_t prefix_length(std::string_view text, std::size_t count) noexcept;

/// Number of grapheme clusters in `text`, i.e. the number of characters a user
/// sees: `e` plus U+0302 plus U+0301 is one cluster, exactly like U+1EBF.
[[nodiscard]] std::size_t count_graphemes(std::string_view text) noexcept;

/// How much two strings share at their start.
///
/// `bytes` is a code point boundary in both strings, which makes it usable as
/// the cut between "delete" and "commit" when the direct typing path turns the
/// old text into the new one.
struct Prefix {
    std::size_t bytes{0};       ///< Shared bytes, equal in both strings.
    std::size_t code_points{0}; ///< Shared code points.
    std::size_t graphemes{0};   ///< Shared grapheme clusters.

    friend constexpr bool operator==(const Prefix&, const Prefix&) = default;
};

/// Compare two strings byte by byte, but never stop inside a code point.
[[nodiscard]] Prefix common_prefix(std::string_view a, std::string_view b) noexcept;

/// Byte offset of the position `clusters` grapheme clusters before the end of
/// `text`, clamped to the start of the string. `grapheme_boundary(text, 0)` is
/// `text.size()`, `grapheme_boundary(text, n)` for `n >= count_graphemes(text)`
/// is zero, so `text.substr(grapheme_boundary(text, n))` is the last `n`
/// characters of the string.
[[nodiscard]] std::size_t grapheme_boundary(std::string_view text, std::size_t clusters) noexcept;

} // namespace nexkey::unicode
