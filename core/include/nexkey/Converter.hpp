// SPDX-License-Identifier: MIT
// NexKey - character set conversion.

#pragma once

#include "nexkey/Types.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace nexkey {

/// Normalization form of the Vietnamese alphabet.
///
/// The same Vietnamese letter has two valid spellings: one precomposed code
/// point (NFC: U+1EAF is a with a breve and an acute) or a base letter followed
/// by combining marks in canonical order (NFD: "a" + U+0306 + U+0301).
/// Applications and keyboard layouts disagree about which one they use, so the
/// frontends compare text after normalizing it, never by raw bytes.
enum class Normalization : std::uint8_t {
    Nfc, ///< One code point per letter, the form most applications use.
    Nfd, ///< Base letter plus combining marks in canonical order.
};

/// Conversion between the character sets that NexKey can produce.
class Converter {
public:
    /// Decode bytes in `encoding` into precomposed Unicode code points.
    [[nodiscard]] static std::u32string decode(Encoding encoding, std::string_view bytes);

    /// Encode precomposed Unicode code points into `encoding`.
    [[nodiscard]] static std::string encode(Encoding encoding, std::u32string_view text);

    /// Convert text from one character set to another.
    [[nodiscard]] static std::string convert(std::string_view input, Encoding from, Encoding to);

    /// Rewrite a UTF-8 string in the given normalization form.
    ///
    /// Only the letters of the Vietnamese alphabet are decomposed and composed
    /// again, so a string without Vietnamese letters comes back unchanged, and
    /// mixed input such as a precomposed a with a breve followed by a combining
    /// acute is turned into a single letter. The result is always well formed UTF-8;
    /// malformed input is decoded the way nexkey::unicode::decode decodes it.
    [[nodiscard]] static std::string normalize(std::string_view utf8, Normalization form);

    /// True when `a` and `b` are the same Vietnamese text, whichever
    /// normalization form the two use. Text outside the Vietnamese alphabet is
    /// compared byte for byte.
    [[nodiscard]] static bool equal_normalized(std::string_view a, std::string_view b);

    /// True for the UTF-8 based encodings.
    [[nodiscard]] static constexpr bool is_unicode(Encoding encoding) noexcept
    {
        return encoding == Encoding::Unicode || encoding == Encoding::UnicodeDecomposed;
    }
};

} // namespace nexkey
