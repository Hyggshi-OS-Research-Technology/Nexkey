// SPDX-License-Identifier: MIT
// NexKey - UTF-8 helpers.

#include "nexkey/Unicode.hpp"

namespace nexkey::unicode {

void append(std::string& out, char32_t code_point)
{
    if (code_point <= 0x7F) {
        out.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
        out.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else if (code_point <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else if (code_point <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    } else {
        append(out, kReplacementCharacter);
    }
}

std::string encode(std::u32string_view text)
{
    std::string out;
    out.reserve(text.size() * 3);
    for (const char32_t code_point : text) {
        append(out, code_point);
    }
    return out;
}

std::u32string decode(std::string_view text)
{
    std::u32string out;
    out.reserve(text.size());

    std::size_t index = 0;
    while (index < text.size()) {
        const auto lead = static_cast<unsigned char>(text[index]);
        std::size_t length = 0;
        char32_t code_point = 0;
        if (lead < 0x80U) {
            out.push_back(static_cast<char32_t>(lead));
            ++index;
            continue;
        }
        if ((lead & 0xE0U) == 0xC0U) {
            length = 2;
            code_point = lead & 0x1FU;
        } else if ((lead & 0xF0U) == 0xE0U) {
            length = 3;
            code_point = lead & 0x0FU;
        } else if ((lead & 0xF8U) == 0xF0U) {
            length = 4;
            code_point = lead & 0x07U;
        } else {
            out.push_back(kReplacementCharacter);
            ++index;
            continue;
        }

        if (index + length > text.size()) {
            out.push_back(kReplacementCharacter);
            break;
        }

        bool valid = true;
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto byte = static_cast<unsigned char>(text[index + offset]);
            if ((byte & 0xC0U) != 0x80U) {
                valid = false;
                break;
            }
            code_point = (code_point << 6U) | (byte & 0x3FU);
        }
        if (!valid) {
            out.push_back(kReplacementCharacter);
            ++index;
            continue;
        }
        out.push_back(code_point);
        index += length;
    }
    return out;
}

} // namespace nexkey::unicode
