#include "vkcore/VKTelex.hpp"

#include "VKCommon.hpp"

namespace vkcore {

std::string apply_telex_rules(const std::string& raw)
{
    using namespace detail;
    std::string text = raw;
    ToneMark tone = ToneMark::None;

    for (std::size_t i = text.size(); i > 0; --i) {
        if (auto t = telex_tone(text[i - 1]); t.has_value()) {
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
        const char prev = chars[i - 1].base;
        const char cur = chars[i].base;
        if (prev == 'd' && cur == 'd') {
            chars[i - 1].mod = VowelMod::Hat;
            text.erase(i, 1);
            chars.erase(chars.begin() + static_cast<std::ptrdiff_t>(i));
            --i;
        } else if (prev == cur && (cur == 'a' || cur == 'e' || cur == 'o')) {
            chars[i - 1].mod = VowelMod::Hat;
            text.erase(i, 1);
            chars.erase(chars.begin() + static_cast<std::ptrdiff_t>(i));
            --i;
        } else if (prev == 'a' && cur == 'w') {
            chars[i - 1].mod = VowelMod::Breve;
            text.erase(i, 1);
            chars.erase(chars.begin() + static_cast<std::ptrdiff_t>(i));
            --i;
        } else if ((prev == 'o' || prev == 'u') && (cur == 'w' || cur == '[' || cur == ']')) {
            chars[i - 1].mod = VowelMod::Horn;
            text.erase(i, 1);
            chars.erase(chars.begin() + static_cast<std::ptrdiff_t>(i));
            --i;
        }
    }

    return render(text, chars, tone);
}

} // namespace vkcore
