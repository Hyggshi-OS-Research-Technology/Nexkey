// SPDX-License-Identifier: MIT
// NexKey - character set conversion.

#pragma once

#include "nexkey/Types.hpp"
#include "nexkey/Unicode.hpp"

#include <string>
#include <string_view>

namespace nexkey {

/// Normalization form, see nexkey::unicode::Normalization.
using Normalization = unicode::Normalization;

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
    /// This is nexkey::unicode::normalize with the Unicode backend behind it:
    /// ICU when the library was built with it, the Vietnamese alphabet tables
    /// otherwise. The result is always well formed UTF-8; malformed input is
    /// decoded the way nexkey::unicode::decode decodes it.
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
