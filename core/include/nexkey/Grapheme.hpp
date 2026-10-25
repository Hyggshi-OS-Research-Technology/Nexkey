// SPDX-License-Identifier: MIT
// NexKey - Vietnamese letter composition and decomposition.

#pragma once

#include "nexkey/Types.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace nexkey {

/// A single letter of the Vietnamese alphabet, without its tone mark.
struct Letter {
    char32_t base{};           ///< Lowercase ASCII letter: a-z.
    Mark mark{Mark::None};     ///< Vowel modifier or the stroke of a d.
    bool upper{false};         ///< Rendered as uppercase.

    friend constexpr bool operator==(const Letter&, const Letter&) = default;
};

/// A letter together with the tone mark it carries.
struct Decomposed {
    Letter letter;
    Tone tone{Tone::None};

    friend constexpr bool operator==(const Decomposed&, const Decomposed&) = default;
};

/// True for a, e, i, o, u and y (lowercase).
[[nodiscard]] constexpr bool is_vowel(char32_t base) noexcept
{
    switch (base) {
    case U'a':
    case U'e':
    case U'i':
    case U'o':
    case U'u':
    case U'y':
        return true;
    default:
        return false;
    }
}

/// True when the base letter belongs to the Vietnamese alphabet.
[[nodiscard]] constexpr bool is_vietnamese_letter(char32_t base) noexcept
{
    switch (base) {
    case U'a':
    case U'b':
    case U'c':
    case U'd':
    case U'e':
    case U'g':
    case U'h':
    case U'i':
    case U'k':
    case U'l':
    case U'm':
    case U'n':
    case U'o':
    case U'p':
    case U'q':
    case U'r':
    case U's':
    case U't':
    case U'u':
    case U'v':
    case U'x':
    case U'y':
        return true;
    default:
        return false;
    }
}

/// Combine a letter and a tone mark into a Unicode code point.
///
/// Returns the plain base letter for combinations that do not exist in
/// Vietnamese (for example a consonant with a tone mark).
[[nodiscard]] char32_t compose(Letter letter, Tone tone) noexcept;

/// Split a Unicode code point into its base letter and tone mark.
[[nodiscard]] std::optional<Decomposed> decompose(char32_t code_point) noexcept;

/// Convert a code point to lowercase/uppercase ASCII when it is a plain letter.
[[nodiscard]] constexpr char32_t ascii_lower(char32_t code_point) noexcept
{
    return (code_point >= U'A' && code_point <= U'Z') ? code_point + 32 : code_point;
}

/// True when the code point is an uppercase ASCII letter.
[[nodiscard]] constexpr bool is_ascii_upper(char32_t code_point) noexcept
{
    return code_point >= U'A' && code_point <= U'Z';
}

} // namespace nexkey
