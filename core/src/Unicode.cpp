// SPDX-License-Identifier: MIT
// NexKey - strict UTF-8 decoding, text measuring and the Unicode backend.
//
// The byte level is implemented here and nowhere else: decoding, encoding and
// the code point boundaries. Everything that needs the Unicode character
// database - normalization, grapheme clusters, case mapping - comes from ICU
// when the library is built with it. Without ICU a compact implementation that
// knows the Vietnamese alphabet is used, and text outside that alphabet is
// copied through unchanged. Both backends keep the same guarantees: the output
// is always well formed UTF-8 and no helper ever returns a position inside a
// code point.

#include "nexkey/Unicode.hpp"

#include "nexkey/Grapheme.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(NEXKEY_HAVE_ICU)
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#include <unicode/utext.h>
#include <unicode/utypes.h>
#include <unicode/uvernum.h>
#endif

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

// ---------------------------------------------------------------------------
// The Vietnamese alphabet: the fallback when ICU is not available.
//
// These tables cover exactly the letters of the alphabet. They are what keeps a
// build without ICU useful for Vietnamese text, and they are also what
// Grapheme.hpp uses to compose and decompose a single letter.
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

/// Canonical combining class of the marks above (Unicode character database).
///
/// UAX #15 sorts the marks of a decomposition by this number, and the canonical
/// decompositions of the Vietnamese letters depend on it: the horn (216) comes
/// before the dot below (220), which comes before every accent that sits above
/// the letter (230). Writing the dot of "o with a circumflex and a dot below"
/// after the circumflex produces a string that looks right but is not NFD, and
/// a client that normalises its document would then disagree with NexKey about
/// the text. ICU does this itself; the table is only needed for the fallback.
constexpr int combining_class(char32_t code_point) noexcept
{
    switch (code_point) {
    case kCombiningHorn:
        return 216;
    case kCombiningDot:
        return 220;
    default:
        return 230;
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

/// Decompose the Vietnamese letters, leave everything else as it is, and write
/// the marks in canonical order.
std::string encode_nfd_vietnamese(std::u32string_view text)
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
            append(out, code_point);
            continue;
        }
        const auto& letter = decomposed->letter;
        append(out, letter.upper ? letter.base - 32 : letter.base);
        const auto mark = combining_mark(letter.mark);
        const auto tone = combining_tone(decomposed->tone);
        if (mark && tone && combining_class(*tone) < combining_class(*mark)) {
            append(out, *tone);
            append(out, *mark);
        } else {
            if (mark) {
                append(out, *mark);
            }
            if (tone) {
                append(out, *tone);
            }
        }
    }
    return out;
}

/// Compose base letters and their marks back into Vietnamese letters. Marks may
/// arrive in any order, and text that is not Vietnamese stays as it is.
std::u32string decode_nfd_vietnamese(std::string_view bytes)
{
    const auto code_points = decode(bytes);
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

// ---------------------------------------------------------------------------
// Grapheme clusters: byte offset of the start of every cluster, plus one entry
// for the end of the string.
// ---------------------------------------------------------------------------

std::vector<std::size_t> cluster_starts_builtin(std::string_view text)
{
    std::vector<std::size_t> starts{0};
    std::size_t index = 0;
    while (index < text.size()) {
        const auto decoded = decode_at(text, index);
        if (index > 0 && !is_extending(decoded.code_point)) {
            starts.push_back(index);
        }
        index += decoded.length;
    }
    if (starts.back() != text.size()) {
        starts.push_back(text.size());
    }
    return starts;
}

#if defined(NEXKEY_HAVE_ICU)

std::u16string to_utf16(std::u32string_view text)
{
    std::u16string out;
    out.reserve(text.size());
    for (char32_t code_point : text) {
        if (!is_scalar_value(code_point)) {
            code_point = kReplacementCharacter;
        }
        if (code_point <= 0xFFFF) {
            out.push_back(static_cast<char16_t>(code_point));
        } else {
            code_point -= 0x10000;
            out.push_back(static_cast<char16_t>(0xD800U + (code_point >> 10U)));
            out.push_back(static_cast<char16_t>(0xDC00U + (code_point & 0x3FFU)));
        }
    }
    return out;
}

std::u32string from_utf16(std::u16string_view text)
{
    std::u32string out;
    out.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char16_t unit = text[index];
        if (unit >= 0xD800 && unit <= 0xDBFF && index + 1 < text.size() &&
            text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
            const char32_t code_point = 0x10000U + ((static_cast<char32_t>(unit) - 0xD800U) << 10U) +
                                        (static_cast<char32_t>(text[index + 1]) - 0xDC00U);
            out.push_back(code_point);
            ++index;
        } else {
            out.push_back(static_cast<char32_t>(unit));
        }
    }
    return out;
}

UChar* as_uchar(std::u16string& text) noexcept
{
    return reinterpret_cast<UChar*>(text.data());
}

const UChar* as_uchar(const std::u16string& text) noexcept
{
    return reinterpret_cast<const UChar*>(text.data());
}

/// Normalize with ICU. Returns false when ICU refuses, so that the caller can
/// fall back instead of returning half converted text.
bool normalize_icu(std::string_view text, Normalization form, std::string& out)
{
    const std::u16string input = to_utf16(decode(text));
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer =
        form == Normalization::Nfc ? unorm2_getNFCInstance(&status) : unorm2_getNFDInstance(&status);
    if (U_FAILURE(status) || normalizer == nullptr) {
        return false;
    }

    const auto* source = as_uchar(input);
    const auto length = static_cast<int32_t>(input.size());
    status = U_ZERO_ERROR;
    const int32_t needed = unorm2_normalize(normalizer, source, length, nullptr, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        return false;
    }
    std::u16string normalized(static_cast<std::size_t>(needed), u'\0');
    status = U_ZERO_ERROR;
    unorm2_normalize(normalizer, source, length, as_uchar(normalized), needed, &status);
    if (U_FAILURE(status)) {
        return false;
    }
    out = encode(from_utf16(normalized));
    return true;
}

std::string upper_icu(std::string_view text)
{
    const std::u16string input = to_utf16(decode(text));
    UErrorCode status = U_ZERO_ERROR;
    // The root locale: Vietnamese has no case rules of its own, and a locale
    // that depends on the user environment would make the result unpredictable.
    const int32_t needed =
        u_strToUpper(nullptr, 0, as_uchar(input), static_cast<int32_t>(input.size()), "", &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        return {};
    }
    std::u16string upper(static_cast<std::size_t>(needed), u'\0');
    status = U_ZERO_ERROR;
    u_strToUpper(as_uchar(upper), needed, as_uchar(input), static_cast<int32_t>(input.size()), "",
                 &status);
    if (U_FAILURE(status)) {
        return {};
    }
    return encode(from_utf16(upper));
}

/// Cluster starts from the ICU character break iterator, which implements the
/// extended grapheme cluster rules. UText wraps the UTF-8 buffer directly, so
/// the positions it reports are byte offsets and no conversion is needed.
std::vector<std::size_t> cluster_starts_icu(std::string_view text)
{
    std::vector<std::size_t> starts;
    UErrorCode status = U_ZERO_ERROR;
    UText utf8 = UTEXT_INITIALIZER;
    utext_openUTF8(&utf8, text.data(), static_cast<int64_t>(text.size()), &status);
    if (U_FAILURE(status)) {
        return {};
    }

    UBreakIterator* iterator = ubrk_open(UBRK_CHARACTER, "", nullptr, 0, &status);
    if (U_FAILURE(status) || iterator == nullptr) {
        utext_close(&utf8);
        return {};
    }

    ubrk_setUText(iterator, &utf8, &status);
    if (U_SUCCESS(status)) {
        starts.push_back(0);
        for (int32_t position = ubrk_first(iterator); position != UBRK_DONE;
             position = ubrk_next(iterator)) {
            if (position > 0 && static_cast<std::size_t>(position) != starts.back()) {
                starts.push_back(static_cast<std::size_t>(position));
            }
        }
        if (starts.back() != text.size()) {
            starts.push_back(text.size());
        }
    }

    ubrk_close(iterator);
    utext_close(&utf8);
    if (starts.empty()) {
        return {};
    }
    return starts;
}

#endif // NEXKEY_HAVE_ICU

std::vector<std::size_t> cluster_starts(std::string_view text)
{
    if (text.empty()) {
        return {0};
    }
#if defined(NEXKEY_HAVE_ICU)
    auto starts = cluster_starts_icu(text);
    if (!starts.empty()) {
        return starts;
    }
#endif
    return cluster_starts_builtin(text);
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

bool has_full_unicode() noexcept
{
#if defined(NEXKEY_HAVE_ICU)
    return true;
#else
    return false;
#endif
}

std::string_view backend_name() noexcept
{
#if defined(NEXKEY_HAVE_ICU)
    return "icu " U_ICU_VERSION;
#else
    return "builtin";
#endif
}

std::string normalize(std::string_view text, Normalization form)
{
#if defined(NEXKEY_HAVE_ICU)
    std::string normalized;
    if (normalize_icu(text, form, normalized)) {
        return normalized;
    }
#endif
    // Fallback: compose what the alphabet table knows, then write it back in
    // the requested form. Text outside the alphabet keeps its bytes.
    if (form == Normalization::Nfc) {
        return encode(decode_nfd_vietnamese(text));
    }
    return encode_nfd_vietnamese(decode_nfd_vietnamese(text));
}

std::size_t count_graphemes(std::string_view text)
{
    return cluster_starts(text).size() - 1;
}

std::size_t grapheme_boundary(std::string_view text, std::size_t clusters)
{
    const auto starts = cluster_starts(text);
    const std::size_t count = starts.size() - 1;
    if (clusters >= count) {
        return 0;
    }
    return starts[count - clusters];
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

std::string upper(std::string_view text)
{
    if (text.empty()) {
        return {};
    }
#if defined(NEXKEY_HAVE_ICU)
    if (auto result = upper_icu(text); !result.empty()) {
        return result;
    }
#endif
    std::string out(text);
    for (char& character : out) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(character - 'a' + 'A');
        }
    }
    return out;
}

std::string upper_first(std::string_view text)
{
    if (text.empty()) {
        return {};
    }
    auto code_points = decode(text);
    const char32_t first = code_points.front();
#if defined(NEXKEY_HAVE_ICU)
    code_points.front() = static_cast<char32_t>(u_toupper(static_cast<UChar32>(first)));
#else
    if (first >= U'a' && first <= U'z') {
        code_points.front() = first - 32;
    }
#endif
    return encode(code_points);
}

} // namespace nexkey::unicode
