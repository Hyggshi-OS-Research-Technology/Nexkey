// SPDX-License-Identifier: MIT
// NexKey - character set conversion.

#pragma once

#include "nexkey/Types.hpp"

#include <string>
#include <string_view>

namespace nexkey {

/// Conversion between the character sets that NexKey can produce.
class Converter {
public:
    /// Decode bytes in `encoding` into precomposed Unicode code points.
    [[nodiscard]] static std::u32string decode(Encoding encoding, std::string_view bytes);

    /// Encode precomposed Unicode code points into `encoding`.
    [[nodiscard]] static std::string encode(Encoding encoding, std::u32string_view text);

    /// Convert text from one character set to another.
    [[nodiscard]] static std::string convert(std::string_view input, Encoding from, Encoding to);

    /// True for the UTF-8 based encodings.
    [[nodiscard]] static constexpr bool is_unicode(Encoding encoding) noexcept
    {
        return encoding == Encoding::Unicode || encoding == Encoding::UnicodeDecomposed;
    }
};

} // namespace nexkey
