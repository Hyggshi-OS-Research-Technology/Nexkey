#pragma once

#include <string>

namespace vkcore::VKEncoding {

std::string to_tcvn3(const std::string& utf8);
std::string to_vni(const std::string& utf8);
std::string to_utf8_from_vni(const std::string& text);

} // namespace vkcore::VKEncoding
