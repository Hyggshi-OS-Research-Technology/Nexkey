// SPDX-License-Identifier: MIT
// NexKey - core types and enums.

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace nexkey {

inline constexpr std::string_view kProgramName{"NexKey"};
inline constexpr std::string_view kVersion{"2.0.0"};

/// Input method used to turn keystrokes into Vietnamese text.
enum class Method : std::uint8_t {
    Telex,      ///< Full Telex with double letters, w-shortcuts and brackets.
    SimpleTelex,///< Telex without the w-shortcuts and bracket shortcuts.
    Vni,        ///< VNI: digits 1-9 act as tone and vowel modifiers.
    Viqr,       ///< VIQR: pure ASCII notation with punctuation markers.
    Microsoft,  ///< Microsoft layout (Telex with bracket shortcuts).
};

/// Character set used for the composed output.
enum class Encoding : std::uint8_t {
    Unicode,           ///< Precomposed UTF-8 (the default).
    UnicodeDecomposed, ///< UTF-8 in Unicode NFD (base letters plus marks).
    Tcvn3,             ///< Legacy single byte TCVN3 (ABC) charset.
    Viqr,              ///< Pure ASCII VIQR notation.
};

/// Vietnamese tone marks.
enum class Tone : std::uint8_t { None, Acute, Grave, Hook, Tilde, Dot };

/// Diacritics that change the quality of a vowel, or the stroke of a d.
enum class Mark : std::uint8_t { None, Circumflex, Breve, Horn, Stroke };

/// Placement style for tones in open syllables such as "oa", "oe", "uy".
enum class ToneStyle : std::uint8_t {
    Traditional, ///< Tone on the first vowel: hoa with a grave accent on o.
    Modern,      ///< Tone on the second vowel: hoa with a grave accent on a.
};

/// Runtime configuration of the text engine.
struct Options {
    Method method{Method::Telex};
    Encoding encoding{Encoding::Unicode};
    ToneStyle tone_style{ToneStyle::Modern};
    /// Remove a whole composition step instead of a single keystroke.
    bool smart_backspace{true};
    /// Keep typing raw keystrokes while the word is not a Vietnamese syllable.
    bool english_fallback{true};
    /// Allow the Telex bracket shortcuts for the horned vowels.
    bool bracket_shortcuts{true};
    /// Offer the alternative interpretations of a word in a candidate window.
    bool candidates{true};
    /// Type directly into the document without preedit underline.
    bool direct_typing{true};
    /// Spell check with dictionary.
    bool spell_check{true};
    /// Enable macro shorthand expansion.
    bool macro_enabled{true};
    /// Capitalize macro replacements when abbreviation is capitalized.
    bool capitalize_macro{true};
    /// Enable custom personal dictionary.
    bool custom_dict{false};

    friend constexpr bool operator==(const Options&, const Options&) = default;
};

namespace literals {
inline constexpr std::string_view kMethodNames[]{"telex", "telex-simple", "vni", "viqr", "microsoft"};
inline constexpr std::string_view kEncodingNames[]{"unicode", "unicode-decomposed", "tcvn3", "viqr"};
inline constexpr std::string_view kToneStyleNames[]{"traditional", "modern"};
} // namespace literals

constexpr std::string_view to_string(Method method) noexcept
{
    const auto index = static_cast<std::size_t>(method);
    return index <= 4 ? literals::kMethodNames[index] : std::string_view{};
}

constexpr std::string_view to_string(Encoding encoding) noexcept
{
    const auto index = static_cast<std::size_t>(encoding);
    return index <= 3 ? literals::kEncodingNames[index] : std::string_view{};
}

constexpr std::string_view to_string(ToneStyle style) noexcept
{
    const auto index = static_cast<std::size_t>(style);
    return index <= 1 ? literals::kToneStyleNames[index] : std::string_view{};
}

constexpr std::optional<Method> method_from_string(std::string_view name) noexcept
{
    for (std::size_t i = 0; i < 5; ++i) {
        if (name == literals::kMethodNames[i]) {
            return static_cast<Method>(i);
        }
    }
    if (name == "telex_simple" || name == "simple-telex") {
        return Method::SimpleTelex;
    }
    if (name == "ms" || name == "windows") {
        return Method::Microsoft;
    }
    return std::nullopt;
}

constexpr std::optional<Encoding> encoding_from_string(std::string_view name) noexcept
{
    for (std::size_t i = 0; i < 4; ++i) {
        if (name == literals::kEncodingNames[i]) {
            return static_cast<Encoding>(i);
        }
    }
    if (name == "utf8" || name == "utf-8") {
        return Encoding::Unicode;
    }
    if (name == "nfd" || name == "unicode-nfd") {
        return Encoding::UnicodeDecomposed;
    }
    if (name == "tcvn" || name == "abc") {
        return Encoding::Tcvn3;
    }
    return std::nullopt;
}

constexpr std::optional<ToneStyle> tone_style_from_string(std::string_view name) noexcept
{
    for (std::size_t i = 0; i < 2; ++i) {
        if (name == literals::kToneStyleNames[i]) {
            return static_cast<ToneStyle>(i);
        }
    }
    if (name == "old") {
        return ToneStyle::Traditional;
    }
    if (name == "new") {
        return ToneStyle::Modern;
    }
    return std::nullopt;
}

} // namespace nexkey
