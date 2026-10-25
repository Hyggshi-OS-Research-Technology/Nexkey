// SPDX-License-Identifier: MIT
// Helpers shared by the test translation units.

#pragma once

#include "nexkey/Engine.hpp"
#include "nexkey/Grapheme.hpp"
#include "nexkey/Types.hpp"
#include "nexkey/Unicode.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace testing {

/// Type every byte of `keys` as a separate keystroke and return the text that
/// the engine produced, including the final commit.
inline std::string type_text(std::string_view keys, nexkey::Options options = {})
{
    nexkey::Engine engine(options);
    std::string out;
    for (const char key : keys) {
        out += engine.type(key).commit;
    }
    out += engine.flush().commit;
    return out;
}

/// Render a single word without committing it.
inline std::string render_word(std::string_view keys, nexkey::Options options = {})
{
    const nexkey::Engine engine(options);
    return engine.render(keys);
}

inline nexkey::Options telex_options(nexkey::ToneStyle style = nexkey::ToneStyle::Modern,
                                     bool english_fallback = true)
{
    nexkey::Options options;
    options.method = nexkey::Method::Telex;
    options.tone_style = style;
    options.english_fallback = english_fallback;
    return options;
}

inline nexkey::Options method_options(nexkey::Method method)
{
    nexkey::Options options;
    options.method = method;
    return options;
}

/// Split a UTF-8 string into toneless letters for the syllable helpers.
inline std::vector<nexkey::Letter> letters_of(std::string_view utf8)
{
    std::vector<nexkey::Letter> letters;
    for (const char32_t code_point : nexkey::unicode::decode(utf8)) {
        if (const auto decomposed = nexkey::decompose(code_point)) {
            letters.push_back(decomposed->letter);
        } else {
            letters.push_back(nexkey::Letter{code_point, nexkey::Mark::None, false});
        }
    }
    return letters;
}

} // namespace testing
