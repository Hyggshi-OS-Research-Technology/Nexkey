// SPDX-License-Identifier: MIT
// NexKey - keystroke parsing and word composition.

#pragma once

#include "nexkey/Grapheme.hpp"
#include "nexkey/Types.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace nexkey::detail {

/// A letter produced by the keystroke parser.
struct ParsedLetter {
    char32_t base{};
    Mark mark{Mark::None};
    bool upper{false};
    /// Keystroke that applied `mark`, used to detect a repeated key.
    char trigger{' '};
};

/// A word as understood from a sequence of keystrokes.
struct ParsedWord {
    std::vector<ParsedLetter> letters;
    Tone tone{Tone::None};
    /// Keystroke that applied `tone`.
    char tone_trigger{' '};
    /// True when the last keystroke took back a diacritic (double key press),
    /// which means the word must be shown exactly as parsed.
    bool escaped{false};

    [[nodiscard]] std::vector<Letter> to_letters() const;
    [[nodiscard]] bool empty() const noexcept { return letters.empty(); }
};

/// Interpret raw keystrokes with the rules of `method`.
[[nodiscard]] ParsedWord parse(Method method, std::string_view raw, const Options& options);

/// Compose the parsed word into precomposed Unicode, applying tone placement.
[[nodiscard]] std::u32string compose_word(const ParsedWord& word, const Options& options);

/// True when the code point ends a word for the given method.
[[nodiscard]] bool is_word_boundary(char32_t code_point, const Options& options) noexcept;

} // namespace nexkey::detail
