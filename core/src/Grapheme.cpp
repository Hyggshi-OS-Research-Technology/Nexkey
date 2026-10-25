// SPDX-License-Identifier: MIT
// NexKey - Vietnamese letter composition tables.
//
// Every table entry is written with Unicode escape sequences so that the
// source stays pure ASCII regardless of the editor locale.

#include "nexkey/Grapheme.hpp"

#include <algorithm>
#include <array>

namespace nexkey {

namespace {

struct Row {
    char32_t base;
    Mark mark;
    std::array<char32_t, 6> lower; ///< Tone::None, Acute, Grave, Hook, Tilde, Dot.
    std::array<char32_t, 6> upper;
    bool toned; ///< False for letters that cannot carry a tone (d with stroke).
};

// Tone order: none, acute, grave, hook, tilde, dot.
constexpr std::array<Row, 13> kRows{{
    {U'a', Mark::None,
     {U'a', U'\u00E1', U'\u00E0', U'\u1EA3', U'\u00E3', U'\u1EA1'},
     {U'A', U'\u00C1', U'\u00C0', U'\u1EA2', U'\u00C3', U'\u1EA0'}, true},
    {U'a', Mark::Breve,
     {U'\u0103', U'\u1EAF', U'\u1EB1', U'\u1EB3', U'\u1EB5', U'\u1EB7'},
     {U'\u0102', U'\u1EAE', U'\u1EB0', U'\u1EB2', U'\u1EB4', U'\u1EB6'}, true},
    {U'a', Mark::Circumflex,
     {U'\u00E2', U'\u1EA5', U'\u1EA7', U'\u1EA9', U'\u1EAB', U'\u1EAD'},
     {U'\u00C2', U'\u1EA4', U'\u1EA6', U'\u1EA8', U'\u1EAA', U'\u1EAC'}, true},
    {U'e', Mark::None,
     {U'e', U'\u00E9', U'\u00E8', U'\u1EBB', U'\u1EBD', U'\u1EB9'},
     {U'E', U'\u00C9', U'\u00C8', U'\u1EBA', U'\u1EBC', U'\u1EB8'}, true},
    {U'e', Mark::Circumflex,
     {U'\u00EA', U'\u1EBF', U'\u1EC1', U'\u1EC3', U'\u1EC5', U'\u1EC7'},
     {U'\u00CA', U'\u1EBE', U'\u1EC0', U'\u1EC2', U'\u1EC4', U'\u1EC6'}, true},
    {U'i', Mark::None,
     {U'i', U'\u00ED', U'\u00EC', U'\u1EC9', U'\u0129', U'\u1ECB'},
     {U'I', U'\u00CD', U'\u00CC', U'\u1EC8', U'\u0128', U'\u1ECA'}, true},
    {U'o', Mark::None,
     {U'o', U'\u00F3', U'\u00F2', U'\u1ECF', U'\u00F5', U'\u1ECD'},
     {U'O', U'\u00D3', U'\u00D2', U'\u1ECE', U'\u00D5', U'\u1ECC'}, true},
    {U'o', Mark::Circumflex,
     {U'\u00F4', U'\u1ED1', U'\u1ED3', U'\u1ED5', U'\u1ED7', U'\u1ED9'},
     {U'\u00D4', U'\u1ED0', U'\u1ED2', U'\u1ED4', U'\u1ED6', U'\u1ED8'}, true},
    {U'o', Mark::Horn,
     {U'\u01A1', U'\u1EDB', U'\u1EDD', U'\u1EDF', U'\u1EE1', U'\u1EE3'},
     {U'\u01A0', U'\u1EDA', U'\u1EDC', U'\u1EDE', U'\u1EE0', U'\u1EE2'}, true},
    {U'u', Mark::None,
     {U'u', U'\u00FA', U'\u00F9', U'\u1EE7', U'\u0169', U'\u1EE5'},
     {U'U', U'\u00DA', U'\u00D9', U'\u1EE6', U'\u0168', U'\u1EE4'}, true},
    {U'u', Mark::Horn,
     {U'\u01B0', U'\u1EE9', U'\u1EEB', U'\u1EED', U'\u1EEF', U'\u1EF1'},
     {U'\u01AF', U'\u1EE8', U'\u1EEA', U'\u1EEC', U'\u1EEE', U'\u1EF0'}, true},
    {U'y', Mark::None,
     {U'y', U'\u00FD', U'\u1EF3', U'\u1EF7', U'\u1EF9', U'\u1EF5'},
     {U'Y', U'\u00DD', U'\u1EF2', U'\u1EF6', U'\u1EF8', U'\u1EF4'}, true},
    {U'd', Mark::Stroke, {U'\u0111'}, {U'\u0110'}, false},
}};

struct Entry {
    char32_t code_point;
    Letter letter;
    Tone tone;
};

constexpr std::size_t kEntryCount = 12 * 12 + 2; // 12 toned rows plus d with stroke.

constexpr std::array<Entry, kEntryCount> build_entries()
{
    std::array<Entry, kEntryCount> entries{};
    std::size_t count = 0;
    for (const auto& row : kRows) {
        const std::size_t tones = row.toned ? 6 : 1;
        for (std::size_t tone = 0; tone < tones; ++tone) {
            entries[count++] = {row.lower[tone], Letter{row.base, row.mark, false},
                                static_cast<Tone>(tone)};
            entries[count++] = {row.upper[tone], Letter{row.base, row.mark, true},
                                static_cast<Tone>(tone)};
        }
    }

    // Insertion sort keeps the table constexpr friendly and stable.
    for (std::size_t i = 1; i < count; ++i) {
        const Entry key = entries[i];
        std::size_t j = i;
        while (j > 0 && entries[j - 1].code_point > key.code_point) {
            entries[j] = entries[j - 1];
            --j;
        }
        entries[j] = key;
    }
    return entries;
}

inline constexpr auto kEntries = build_entries();

} // namespace

char32_t compose(Letter letter, Tone tone) noexcept
{
    for (const auto& row : kRows) {
        if (row.base != letter.base || row.mark != letter.mark) {
            continue;
        }
        if (!row.toned) {
            return letter.upper ? row.upper[0] : row.lower[0];
        }
        const auto index = static_cast<std::size_t>(tone);
        return letter.upper ? row.upper[index] : row.lower[index];
    }
    if (letter.base == U'd' && letter.mark == Mark::Stroke) {
        return letter.upper ? U'\u0110' : U'\u0111';
    }
    const auto base = letter.base;
    return letter.upper && base >= U'a' && base <= U'z' ? base - 32 : base;
}

std::optional<Decomposed> decompose(char32_t code_point) noexcept
{
    const auto it = std::lower_bound(kEntries.begin(), kEntries.end(), code_point,
                                     [](const Entry& entry, char32_t value) {
                                         return entry.code_point < value;
                                     });
    if (it == kEntries.end() || it->code_point != code_point) {
        return std::nullopt;
    }
    return Decomposed{it->letter, it->tone};
}

} // namespace nexkey
