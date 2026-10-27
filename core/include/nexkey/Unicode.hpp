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
//
// Normalization, grapheme segmentation and case mapping are the parts that are
// too large to hand maintain: they need the Unicode character database. When
// the library is built with ICU (NEXKEY_USE_ICU, the default) those three come
// from ICU and cover every script. Without ICU a small implementation that
// knows the Vietnamese alphabet is compiled in, and everything outside that
// alphabet is copied through unchanged. has_full_unicode() reports which one is
// in use, and backend_name() is what --version prints.

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

/// Unicode normalization form.
///
/// The same Vietnamese letter has two valid spellings: one precomposed code
/// point (NFC: U+1EAF is a with a breve and an acute) or a base letter followed
/// by combining marks in canonical order (NFD: "a" + U+0306 + U+0301).
/// Applications and keyboard layouts disagree about which one they use, so the
/// frontends compare text after normalizing it, never by raw bytes.
enum class Normalization : std::uint8_t {
    Nfc, ///< One precomposed code point per letter, what most applications use.
    Nfd, ///< Base letter plus combining marks in canonical order.
};

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
/// This is the subset of the Unicode grapheme cluster rules the built-in
/// segmenter needs. With ICU the full extended grapheme cluster rules apply, so
/// `is_extending` only describes what the fallback can see.
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

/// True when the library was built against ICU.
///
/// Then normalization, grapheme segmentation and case mapping cover all of
/// Unicode. Otherwise only the Vietnamese alphabet is handled and text outside
/// it is copied through unchanged.
[[nodiscard]] bool has_full_unicode() noexcept;

/// Name of the Unicode backend, "icu 72.1" or "builtin". Shown by `--version`
/// and written to the diagnostics status so that a report always says which one
/// was running.
[[nodiscard]] std::string_view backend_name() noexcept;

/// Rewrite a UTF-8 string in the given normalization form.
///
/// The output is always well formed UTF-8. Malformed input is decoded the way
/// decode() decodes it, so it can only turn into U+FFFD, never into bytes that
/// a strict decoder would reject again.
[[nodiscard]] std::string normalize(std::string_view text, Normalization form);

/// Number of grapheme clusters in `text`, i.e. the number of characters a user
/// sees: `e` plus U+0302 plus U+0301 is one cluster, exactly like U+1EBF, and
/// with ICU an emoji sequence such as a family joined by zero width joiners is
/// one cluster as well.
[[nodiscard]] std::size_t count_graphemes(std::string_view text);

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
[[nodiscard]] std::size_t grapheme_boundary(std::string_view text, std::size_t clusters);

/// Lower case the ASCII letters of `text`, leaving every other byte alone.
///
/// This is the lowering used for configuration keys, macro shorthand and the
/// personal dictionary. It is deliberately not `std::tolower`, whose result
/// depends on the locale in effect: under `tr_TR.UTF-8` an uppercase I becomes
/// a dotless i, which would stop `INPUT` from matching `input`.
[[nodiscard]] std::string lower_ascii(std::string_view text);

/// Uppercase a UTF-8 string. With ICU this is the full Unicode case mapping
/// ("vi\u1EC7t nam" becomes "VI\u1EC6T NAM"); without it only ASCII letters change.
[[nodiscard]] std::string upper(std::string_view text);

/// Uppercase the first code point of a UTF-8 string and leave the rest alone.
[[nodiscard]] std::string upper_first(std::string_view text);

} // namespace nexkey::unicode
