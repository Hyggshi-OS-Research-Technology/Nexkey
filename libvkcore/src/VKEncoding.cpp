#include "vkcore/VKEncoding.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace vkcore::VKEncoding {

namespace {

std::string replace_all(std::string text, const std::vector<std::pair<std::string_view, std::string_view>>& rules)
{
    for (const auto& [from, to] : rules) {
        std::size_t pos = 0;
        while ((pos = text.find(from.data(), pos, from.size())) != std::string::npos) {
            text.replace(pos, from.size(), to.data(), to.size());
            pos += to.size();
        }
    }
    return text;
}

const std::vector<std::pair<std::string_view, std::string_view>>& vni_rules()
{
    static const std::vector<std::pair<std::string_view, std::string_view>> rules = {
        {"ắ", "aé"}, {"ằ", "aè"}, {"ẳ", "aú"}, {"ẵ", "aü"}, {"ặ", "aë"}, {"ă", "aê"},
        {"ấ", "aá"}, {"ầ", "aà"}, {"ẩ", "aå"}, {"ẫ", "aã"}, {"ậ", "aä"}, {"â", "aâ"},
        {"á", "aù"}, {"à", "aø"}, {"ả", "aû"}, {"ã", "aõ"}, {"ạ", "aï"},
        {"ế", "eá"}, {"ề", "eà"}, {"ể", "eå"}, {"ễ", "eã"}, {"ệ", "eä"}, {"ê", "eâ"},
        {"é", "eù"}, {"è", "eø"}, {"ẻ", "eû"}, {"ẽ", "eõ"}, {"ẹ", "eï"},
        {"í", "í"}, {"ì", "ì"}, {"ỉ", "æ"}, {"ĩ", "ó"}, {"ị", "ò"},
        {"ố", "oá"}, {"ồ", "oà"}, {"ổ", "oå"}, {"ỗ", "oã"}, {"ộ", "oä"}, {"ô", "oâ"},
        {"ớ", "ôù"}, {"ờ", "ôø"}, {"ở", "ôû"}, {"ỡ", "ôõ"}, {"ợ", "ôï"}, {"ơ", "ô"},
        {"ó", "où"}, {"ò", "oø"}, {"ỏ", "oû"}, {"õ", "oõ"}, {"ọ", "oï"},
        {"ứ", "öù"}, {"ừ", "öø"}, {"ử", "öû"}, {"ữ", "öõ"}, {"ự", "öï"}, {"ư", "ö"},
        {"ú", "uù"}, {"ù", "uø"}, {"ủ", "uû"}, {"ũ", "uõ"}, {"ụ", "uï"},
        {"ý", "yù"}, {"ỳ", "yø"}, {"ỷ", "yû"}, {"ỹ", "yõ"}, {"ỵ", "î"}, {"đ", "ñ"},
    };
    return rules;
}

} // namespace

std::string to_tcvn3(const std::string& utf8)
{
    static const std::vector<std::pair<std::string_view, std::string_view>> rules = {
        {"ă", "¨"}, {"â", "©"}, {"đ", "®"}, {"ê", "ª"}, {"ô", "«"}, {"ơ", "¬"}, {"ư", "­"},
        {"á", "¸"}, {"à", "µ"}, {"ả", "¶"}, {"ã", "·"}, {"ạ", "¹"},
        {"ắ", "¾"}, {"ằ", "»"}, {"ẳ", "¼"}, {"ẵ", "½"}, {"ặ", "Æ"},
        {"ấ", "Ê"}, {"ầ", "Ç"}, {"ẩ", "È"}, {"ẫ", "É"}, {"ậ", "Ë"},
        {"é", "Ð"}, {"è", "Ì"}, {"ẻ", "Î"}, {"ẽ", "Ï"}, {"ẹ", "Ñ"},
        {"ế", "Õ"}, {"ề", "Ò"}, {"ể", "Ó"}, {"ễ", "Ô"}, {"ệ", "Ö"},
        {"í", "Ý"}, {"ì", "×"}, {"ỉ", "Ø"}, {"ĩ", "Ü"}, {"ị", "Þ"},
        {"ó", "ã"}, {"ò", "ß"}, {"ỏ", "á"}, {"õ", "â"}, {"ọ", "ä"},
        {"ố", "è"}, {"ồ", "å"}, {"ổ", "æ"}, {"ỗ", "ç"}, {"ộ", "é"},
        {"ớ", "í"}, {"ờ", "ê"}, {"ở", "ë"}, {"ỡ", "ì"}, {"ợ", "î"},
        {"ú", "ó"}, {"ù", "ï"}, {"ủ", "ñ"}, {"ũ", "ò"}, {"ụ", "ô"},
        {"ứ", "ø"}, {"ừ", "õ"}, {"ử", "ö"}, {"ữ", "÷"}, {"ự", "ù"},
        {"ý", "ý"}, {"ỳ", "ú"}, {"ỷ", "û"}, {"ỹ", "ü"}, {"ỵ", "þ"},
    };
    return replace_all(utf8, rules);
}

std::string to_vni(const std::string& utf8)
{
    return replace_all(utf8, vni_rules());
}

std::string to_utf8_from_vni(const std::string& text)
{
    auto rules = vni_rules();
    for (auto& rule : rules) {
        std::swap(rule.first, rule.second);
    }
    return replace_all(text, rules);
}

} // namespace vkcore::VKEncoding
