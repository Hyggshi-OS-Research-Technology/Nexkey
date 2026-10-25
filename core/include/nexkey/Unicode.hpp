// SPDX-License-Identifier: MIT
// NexKey - small UTF-8 / UTF-32 helpers.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace nexkey::unicode {

inline constexpr char32_t kReplacementCharacter = U'\uFFFD';

/// Decode UTF-8. Invalid sequences are replaced with U+FFFD.
[[nodiscard]] std::u32string decode(std::string_view text);

/// Encode a single code point as UTF-8 and append it to `out`.
void append(std::string& out, char32_t code_point);

/// Encode a whole UTF-32 string as UTF-8.
[[nodiscard]] std::string encode(std::u32string_view text);

/// Number of bytes a single code point occupies in UTF-8.
[[nodiscard]] constexpr std::size_t encoded_length(char32_t code_point) noexcept
{
    if (code_point <= 0x7F) {
        return 1;
    }
    if (code_point <= 0x7FF) {
        return 2;
    }
    if (code_point <= 0xFFFF) {
        return 3;
    }
    return 4;
}

/// True for ASCII digits, letters and symbols that an IME may consume.
[[nodiscard]] constexpr bool is_ascii(char32_t code_point) noexcept
{
    return code_point <= 0x7F;
}

} // namespace nexkey::unicode
