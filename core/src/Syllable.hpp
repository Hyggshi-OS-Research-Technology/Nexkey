// SPDX-License-Identifier: MIT
// NexKey - Vietnamese syllable structure, tone placement and plausibility.

#pragma once

#include "nexkey/Grapheme.hpp"
#include "nexkey/Types.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace nexkey::detail {

/// Onset, nucleus and coda lengths of a word, measured in letters.
struct Structure {
    std::size_t onset{0};
    std::size_t nucleus{0};
    std::size_t coda{0};
    /// True when nucleus and coda are complete Vietnamese phonemes, false when
    /// the word is only a plausible prefix of one (still being typed).
    bool complete{false};

    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return onset + nucleus + coda;
    }
};

/// Analyse a word of toneless letters.
///
/// The returned lengths always describe the whole word. When no split matches
/// the syllable tables the structure is zero sized and `complete` is false.
[[nodiscard]] Structure analyse(std::span<const Letter> letters) noexcept;

/// True when the word could still become a Vietnamese syllable.
[[nodiscard]] bool is_plausible(std::span<const Letter> letters) noexcept;

/// Index of the letter that carries the tone mark, if the word has a vowel.
[[nodiscard]] std::optional<std::size_t> tone_index(std::span<const Letter> letters,
                                                    ToneStyle style) noexcept;

/// True when the word already contains a vowel.
[[nodiscard]] bool contains_vowel(std::span<const Letter> letters) noexcept;

} // namespace nexkey::detail
