#include "vkcore/VKVni.hpp"

#include "VKCommon.hpp"

namespace vkcore {

std::string apply_vni_rules(const std::string& raw)
{
    using namespace detail;
    std::string text = raw;
    ToneMark tone = ToneMark::None;

    for (std::size_t i = text.size(); i > 0; --i) {
        if (auto t = vni_tone(text[i - 1]); t.has_value()) {
            tone = *t;
            text.erase(i - 1, 1);
            break;
        }
    }

    std::vector<VChar> chars;
    chars.reserve(text.size());
    for (char c : text) {
        chars.push_back({static_cast<char>(std::tolower(static_cast<unsigned char>(c))), VowelMod::None, ToneMark::None});
    }

    for (std::size_t i = 1; i < chars.size(); ++i) {
        const char marker = chars[i].base;
        if (marker == '9' && chars[i - 1].base == 'd') {
            chars[i - 1].mod = VowelMod::Hat;
        } else if (marker == '8' && chars[i - 1].base == 'a') {
            chars[i - 1].mod = VowelMod::Breve;
        } else if (marker == '7' && (chars[i - 1].base == 'o' || chars[i - 1].base == 'u')) {
            chars[i - 1].mod = VowelMod::Horn;
        } else if (marker == '6' && (chars[i - 1].base == 'a' || chars[i - 1].base == 'e' || chars[i - 1].base == 'o')) {
            chars[i - 1].mod = VowelMod::Hat;
        } else {
            continue;
        }
        text.erase(i, 1);
        chars.erase(chars.begin() + static_cast<std::ptrdiff_t>(i));
        --i;
    }

    return render(text, chars, tone);
}

} // namespace vkcore
