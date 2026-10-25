// SPDX-License-Identifier: MIT
// NexKey - Vietnamese syllable structure tables.

#include "Syllable.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace nexkey::detail {

namespace {

using namespace std::string_view_literals;

// Onsets are listed mostly by length so that the longest match wins.
constexpr std::u32string_view kOnsets[]{
    U""sv,   U"b"sv,  U"c"sv,   U"ch"sv,  U"d"sv,  U"\u0111"sv, U"g"sv,
    U"gh"sv, U"gi"sv, U"h"sv,   U"k"sv,   U"kh"sv, U"l"sv,      U"m"sv,
    U"n"sv,  U"ng"sv, U"ngh"sv, U"nh"sv,  U"p"sv,  U"ph"sv,     U"q"sv,
    U"qu"sv, U"r"sv,  U"s"sv,   U"t"sv,   U"th"sv, U"tr"sv,     U"v"sv,
    U"x"sv,
};

// Vowel clusters that can form the nucleus of a syllable. Marks are kept
// because they change the cluster: "ie" is not a Vietnamese nucleus while
// "i\u00EA" (i with circumflex) is.
constexpr std::u32string_view kNuclei[]{
    U"a"sv,        U"ai"sv,        U"ao"sv,        U"au"sv,
    U"ay"sv,       U"\u00E2"sv,    U"\u00E2u"sv,   U"\u00E2y"sv,
    U"\u0103"sv,   U"e"sv,         U"eo"sv,        U"\u00EA"sv,
    U"\u00EAu"sv,  U"i"sv,         U"ia"sv,        U"i\u00EA"sv,
    U"i\u00EAu"sv, U"iu"sv,        U"o"sv,         U"oa"sv,
    U"oai"sv,      U"oao"sv,       U"oay"sv,       U"o\u0103"sv,
    U"oe"sv,       U"oeo"sv,       U"oi"sv,        U"\u00F4"sv,
    U"\u00F4i"sv,  U"\u01A1"sv,    U"\u01A1i"sv,   U"u"sv,
    U"ua"sv,       U"u\u00E2"sv,   U"u\u00E2y"sv,  U"u\u00EA"sv,
    U"u\u00F4"sv,  U"u\u00F4i"sv,  U"ui"sv,        U"u\u01A1"sv,
    U"uy"sv,       U"uya"sv,       U"uy\u00EA"sv,  U"uyu"sv,
    U"\u01B0"sv,   U"\u01B0a"sv,   U"\u01B0i"sv,   U"\u01B0u"sv,
    U"\u01B0\u01A1"sv,
};

// Two more nuclei are appended here to keep the literal above readable.
constexpr std::u32string_view kNucleiTail[]{
    U"\u01B0\u01A1i"sv, U"\u01B0\u01A1u"sv, U"y"sv, U"y\u00EA"sv, U"y\u00EAu"sv,
};

constexpr std::u32string_view kCodas[]{
    U""sv, U"c"sv, U"ch"sv, U"m"sv, U"n"sv, U"ng"sv, U"nh"sv, U"p"sv, U"t"sv,
};

struct Tables {
    std::vector<std::u32string_view> onsets;
    std::vector<std::u32string_view> nuclei;
    std::vector<std::u32string_view> codas;

    Tables()
    {
        for (const auto onset : kOnsets) {
            onsets.push_back(onset);
        }
        for (const auto nucleus : kNuclei) {
            nuclei.push_back(nucleus);
        }
        for (const auto nucleus : kNucleiTail) {
            nuclei.push_back(nucleus);
        }
        for (const auto coda : kCodas) {
            codas.push_back(coda);
        }
        std::ranges::sort(onsets, std::greater{}, &std::u32string_view::size);
        std::ranges::sort(nuclei, std::greater{}, &std::u32string_view::size);
    }
};

const Tables& tables() noexcept
{
    static const Tables instance;
    return instance;
}

bool is_prefix_of_any(std::u32string_view value,
                      const std::vector<std::u32string_view>& set) noexcept
{
    return std::ranges::any_of(set, [value](std::u32string_view candidate) {
        return candidate.starts_with(value);
    });
}

bool is_exact(std::u32string_view value,
              const std::vector<std::u32string_view>& set) noexcept
{
    return std::ranges::find(set, value) != set.end();
}

bool is_vowel_code_point(char32_t code_point) noexcept
{
    if (const auto decomposed = decompose(code_point)) {
        return is_vowel(decomposed->letter.base);
    }
    return is_vowel(ascii_lower(code_point));
}

std::u32string to_toneless(std::span<const Letter> letters)
{
    std::u32string text;
    text.reserve(letters.size());
    for (const auto& letter : letters) {
        text.push_back(compose(Letter{letter.base, letter.mark, false}, Tone::None));
    }
    return text;
}

/// Try to split `text` starting at the given onset.
std::optional<Structure> split_with_onset(const std::u32string& text, std::size_t onset,
                                          bool strict) noexcept
{
    const auto rest = std::u32string_view(text).substr(onset);
    std::size_t vowels = 0;
    while (vowels < rest.size() && is_vowel_code_point(rest[vowels])) {
        ++vowels;
    }

    const auto& table = tables();
    for (std::size_t take = vowels + 1; take-- > 0;) {
        const auto nucleus = rest.substr(0, take);
        const auto coda = rest.substr(take);
        const bool nucleus_ok = strict ? is_exact(nucleus, table.nuclei)
                                       : is_prefix_of_any(nucleus, table.nuclei);
        const bool coda_ok = strict ? is_exact(coda, table.codas)
                                    : is_prefix_of_any(coda, table.codas);
        if (nucleus_ok && coda_ok) {
            return Structure{onset, nucleus.size(), coda.size(), strict};
        }
    }
    return std::nullopt;
}

} // namespace

bool contains_vowel(std::span<const Letter> letters) noexcept
{
    return std::ranges::any_of(letters, [](const Letter& letter) {
        return is_vowel(letter.base);
    });
}

Structure analyse(std::span<const Letter> letters) noexcept
{
    if (letters.empty()) {
        return Structure{};
    }
    const auto text = to_toneless(letters);
    const auto& table = tables();

    // A word may only contain letters of the Vietnamese alphabet.
    const bool alphabet_ok = std::ranges::all_of(letters, [](const Letter& letter) {
        return is_vietnamese_letter(letter.base) ||
               (letter.base == U'd' && letter.mark == Mark::Stroke);
    });
    if (!alphabet_ok) {
        return Structure{};
    }

    for (const auto onset : table.onsets) {
        if (!std::u32string_view(text).starts_with(onset)) {
            continue;
        }
        if (auto structure = split_with_onset(text, onset.size(), true)) {
            return *structure;
        }
    }

    // Fall back to prefix matching so that partially typed words keep working.
    for (const auto onset : table.onsets) {
        if (!std::u32string_view(text).starts_with(onset)) {
            continue;
        }
        if (auto structure = split_with_onset(text, onset.size(), false)) {
            return *structure;
        }
    }

    return Structure{};
}

bool is_plausible(std::span<const Letter> letters) noexcept
{
    if (letters.empty()) {
        return true;
    }
    const auto structure = analyse(letters);
    return structure.size() == letters.size();
}

std::optional<std::size_t> tone_index(std::span<const Letter> letters,
                                      ToneStyle style) noexcept
{
    const auto structure = analyse(letters);
    if (structure.nucleus == 0) {
        // No usable structure: keep the tone on the last vowel instead of
        // silently dropping it.
        for (std::size_t index = letters.size(); index-- > 0;) {
            if (is_vowel(letters[index].base)) {
                return index;
            }
        }
        return std::nullopt;
    }

    const auto begin = structure.onset;
    const auto end = begin + structure.nucleus;

    for (std::size_t index = end; index-- > begin;) {
        if (letters[index].mark != Mark::None) {
            return index;
        }
    }

    if (structure.nucleus >= 3) {
        return begin + 1;
    }
    if (structure.nucleus == 2) {
        if (structure.coda > 0) {
            return begin + 1;
        }
        const auto first = letters[begin].base;
        const auto second = letters[begin + 1].base;
        if (style == ToneStyle::Modern) {
            if ((first == U'o' && (second == U'a' || second == U'e')) ||
                (first == U'u' && second == U'y')) {
                return begin + 1;
            }
        }
        return begin;
    }
    return begin;
}

} // namespace nexkey::detail
