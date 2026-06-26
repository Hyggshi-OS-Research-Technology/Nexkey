#pragma once

#include "vkcore/VKTypes.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace vkcore::detail {

struct VChar {
    char base{};
    VowelMod mod{VowelMod::None};
    ToneMark tone{ToneMark::None};
};

inline bool is_ascii_vowel(char c)
{
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
}

inline bool is_consonant_or_vowel(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

inline std::string utf8(char32_t cp)
{
    std::string out;
    if (cp <= 0x7f) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else if (cp <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    }
    return out;
}

inline std::string compose(VChar v, bool upper)
{
    struct Key {
        char base;
        VowelMod mod;
        ToneMark tone;
        bool operator<(const Key& other) const
        {
            return std::tie(base, mod, tone) < std::tie(other.base, other.mod, other.tone);
        }
    };

    static const std::map<Key, std::array<char32_t, 2>> table = {
        {{'a', VowelMod::None, ToneMark::None}, {U'a', U'A'}},
        {{'a', VowelMod::None, ToneMark::Sac}, {U'á', U'Á'}},
        {{'a', VowelMod::None, ToneMark::Huyen}, {U'à', U'À'}},
        {{'a', VowelMod::None, ToneMark::Hoi}, {U'ả', U'Ả'}},
        {{'a', VowelMod::None, ToneMark::Nga}, {U'ã', U'Ã'}},
        {{'a', VowelMod::None, ToneMark::Nang}, {U'ạ', U'Ạ'}},
        {{'a', VowelMod::Hat, ToneMark::None}, {U'â', U'Â'}},
        {{'a', VowelMod::Hat, ToneMark::Sac}, {U'ấ', U'Ấ'}},
        {{'a', VowelMod::Hat, ToneMark::Huyen}, {U'ầ', U'Ầ'}},
        {{'a', VowelMod::Hat, ToneMark::Hoi}, {U'ẩ', U'Ẩ'}},
        {{'a', VowelMod::Hat, ToneMark::Nga}, {U'ẫ', U'Ẫ'}},
        {{'a', VowelMod::Hat, ToneMark::Nang}, {U'ậ', U'Ậ'}},
        {{'a', VowelMod::Breve, ToneMark::None}, {U'ă', U'Ă'}},
        {{'a', VowelMod::Breve, ToneMark::Sac}, {U'ắ', U'Ắ'}},
        {{'a', VowelMod::Breve, ToneMark::Huyen}, {U'ằ', U'Ằ'}},
        {{'a', VowelMod::Breve, ToneMark::Hoi}, {U'ẳ', U'Ẳ'}},
        {{'a', VowelMod::Breve, ToneMark::Nga}, {U'ẵ', U'Ẵ'}},
        {{'a', VowelMod::Breve, ToneMark::Nang}, {U'ặ', U'Ặ'}},
        {{'e', VowelMod::None, ToneMark::None}, {U'e', U'E'}},
        {{'e', VowelMod::None, ToneMark::Sac}, {U'é', U'É'}},
        {{'e', VowelMod::None, ToneMark::Huyen}, {U'è', U'È'}},
        {{'e', VowelMod::None, ToneMark::Hoi}, {U'ẻ', U'Ẻ'}},
        {{'e', VowelMod::None, ToneMark::Nga}, {U'ẽ', U'Ẽ'}},
        {{'e', VowelMod::None, ToneMark::Nang}, {U'ẹ', U'Ẹ'}},
        {{'e', VowelMod::Hat, ToneMark::None}, {U'ê', U'Ê'}},
        {{'e', VowelMod::Hat, ToneMark::Sac}, {U'ế', U'Ế'}},
        {{'e', VowelMod::Hat, ToneMark::Huyen}, {U'ề', U'Ề'}},
        {{'e', VowelMod::Hat, ToneMark::Hoi}, {U'ể', U'Ể'}},
        {{'e', VowelMod::Hat, ToneMark::Nga}, {U'ễ', U'Ễ'}},
        {{'e', VowelMod::Hat, ToneMark::Nang}, {U'ệ', U'Ệ'}},
        {{'i', VowelMod::None, ToneMark::None}, {U'i', U'I'}},
        {{'i', VowelMod::None, ToneMark::Sac}, {U'í', U'Í'}},
        {{'i', VowelMod::None, ToneMark::Huyen}, {U'ì', U'Ì'}},
        {{'i', VowelMod::None, ToneMark::Hoi}, {U'ỉ', U'Ỉ'}},
        {{'i', VowelMod::None, ToneMark::Nga}, {U'ĩ', U'Ĩ'}},
        {{'i', VowelMod::None, ToneMark::Nang}, {U'ị', U'Ị'}},
        {{'o', VowelMod::None, ToneMark::None}, {U'o', U'O'}},
        {{'o', VowelMod::None, ToneMark::Sac}, {U'ó', U'Ó'}},
        {{'o', VowelMod::None, ToneMark::Huyen}, {U'ò', U'Ò'}},
        {{'o', VowelMod::None, ToneMark::Hoi}, {U'ỏ', U'Ỏ'}},
        {{'o', VowelMod::None, ToneMark::Nga}, {U'õ', U'Õ'}},
        {{'o', VowelMod::None, ToneMark::Nang}, {U'ọ', U'Ọ'}},
        {{'o', VowelMod::Hat, ToneMark::None}, {U'ô', U'Ô'}},
        {{'o', VowelMod::Hat, ToneMark::Sac}, {U'ố', U'Ố'}},
        {{'o', VowelMod::Hat, ToneMark::Huyen}, {U'ồ', U'Ồ'}},
        {{'o', VowelMod::Hat, ToneMark::Hoi}, {U'ổ', U'Ổ'}},
        {{'o', VowelMod::Hat, ToneMark::Nga}, {U'ỗ', U'Ỗ'}},
        {{'o', VowelMod::Hat, ToneMark::Nang}, {U'ộ', U'Ộ'}},
        {{'o', VowelMod::Horn, ToneMark::None}, {U'ơ', U'Ơ'}},
        {{'o', VowelMod::Horn, ToneMark::Sac}, {U'ớ', U'Ớ'}},
        {{'o', VowelMod::Horn, ToneMark::Huyen}, {U'ờ', U'Ờ'}},
        {{'o', VowelMod::Horn, ToneMark::Hoi}, {U'ở', U'Ở'}},
        {{'o', VowelMod::Horn, ToneMark::Nga}, {U'ỡ', U'Ỡ'}},
        {{'o', VowelMod::Horn, ToneMark::Nang}, {U'ợ', U'Ợ'}},
        {{'u', VowelMod::None, ToneMark::None}, {U'u', U'U'}},
        {{'u', VowelMod::None, ToneMark::Sac}, {U'ú', U'Ú'}},
        {{'u', VowelMod::None, ToneMark::Huyen}, {U'ù', U'Ù'}},
        {{'u', VowelMod::None, ToneMark::Hoi}, {U'ủ', U'Ủ'}},
        {{'u', VowelMod::None, ToneMark::Nga}, {U'ũ', U'Ũ'}},
        {{'u', VowelMod::None, ToneMark::Nang}, {U'ụ', U'Ụ'}},
        {{'u', VowelMod::Horn, ToneMark::None}, {U'ư', U'Ư'}},
        {{'u', VowelMod::Horn, ToneMark::Sac}, {U'ứ', U'Ứ'}},
        {{'u', VowelMod::Horn, ToneMark::Huyen}, {U'ừ', U'Ừ'}},
        {{'u', VowelMod::Horn, ToneMark::Hoi}, {U'ử', U'Ử'}},
        {{'u', VowelMod::Horn, ToneMark::Nga}, {U'ữ', U'Ữ'}},
        {{'u', VowelMod::Horn, ToneMark::Nang}, {U'ự', U'Ự'}},
        {{'y', VowelMod::None, ToneMark::None}, {U'y', U'Y'}},
        {{'y', VowelMod::None, ToneMark::Sac}, {U'ý', U'Ý'}},
        {{'y', VowelMod::None, ToneMark::Huyen}, {U'ỳ', U'Ỳ'}},
        {{'y', VowelMod::None, ToneMark::Hoi}, {U'ỷ', U'Ỷ'}},
        {{'y', VowelMod::None, ToneMark::Nga}, {U'ỹ', U'Ỹ'}},
        {{'y', VowelMod::None, ToneMark::Nang}, {U'ỵ', U'Ỵ'}},
    };

    const auto it = table.find({v.base, v.mod, v.tone});
    if (it == table.end()) {
        return std::string(1, upper ? static_cast<char>(std::toupper(v.base)) : v.base);
    }
    return utf8(it->second[upper ? 1 : 0]);
}

inline std::size_t tone_position(const std::vector<VChar>& chars)
{
    std::vector<std::size_t> vowels;
    for (std::size_t i = 0; i < chars.size(); ++i) {
        if (is_ascii_vowel(chars[i].base)) {
            vowels.push_back(i);
        }
    }
    if (vowels.empty()) {
        return std::string::npos;
    }
    for (auto i : vowels) {
        if (chars[i].mod == VowelMod::Horn) {
            return i;
        }
    }
    for (auto i : vowels) {
        if (chars[i].mod == VowelMod::Hat) {
            return i;
        }
    }
    for (std::size_t n = 1; n < vowels.size(); ++n) {
        const char prev = chars[vowels[n - 1]].base;
        const char cur = chars[vowels[n]].base;
        if ((prev == 'i' || prev == 'y') && cur == 'e') {
            return vowels[n];
        }
        if (prev == 'u' && cur == 'o') {
            return vowels[n];
        }
    }
    return vowels.back();
}

inline std::string render(std::string_view original, std::vector<VChar> chars, ToneMark tone)
{
    const auto pos = tone_position(chars);
    if (pos != std::string::npos) {
        chars[pos].tone = tone;
    }

    std::string out;
    out.reserve(original.size() * 3);
    for (std::size_t i = 0; i < chars.size(); ++i) {
        const auto raw = original[i];
        if (chars[i].base == 'd' && chars[i].mod == VowelMod::Hat) {
            out += (std::isupper(static_cast<unsigned char>(raw)) ? utf8(U'Đ') : utf8(U'đ'));
        } else if (is_ascii_vowel(chars[i].base)) {
            out += compose(chars[i], std::isupper(static_cast<unsigned char>(raw)) != 0);
        } else {
            out.push_back(raw);
        }
    }
    return out;
}

inline std::optional<ToneMark> telex_tone(char c)
{
    switch (static_cast<char>(std::tolower(static_cast<unsigned char>(c)))) {
    case 's': return ToneMark::Sac;
    case 'f': return ToneMark::Huyen;
    case 'r': return ToneMark::Hoi;
    case 'x': return ToneMark::Nga;
    case 'j': return ToneMark::Nang;
    case 'z': return ToneMark::None;
    default: return std::nullopt;
    }
}

inline std::optional<ToneMark> vni_tone(char c)
{
    switch (c) {
    case '1': return ToneMark::Sac;
    case '2': return ToneMark::Huyen;
    case '3': return ToneMark::Hoi;
    case '4': return ToneMark::Nga;
    case '5': return ToneMark::Nang;
    case '0': return ToneMark::None;
    default: return std::nullopt;
    }
}

} // namespace vkcore::detail
