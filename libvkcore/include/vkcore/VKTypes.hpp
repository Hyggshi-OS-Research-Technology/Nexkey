#pragma once

#include <cstdint>

namespace vkcore {

enum class InputMethod : std::uint8_t { Telex, VNI, VIQR };
enum class Encoding : std::uint8_t { Unicode, TCVN3, VNIEncoding };
enum class ToneMark : std::uint8_t { None, Sac, Huyen, Hoi, Nga, Nang };
enum class VowelMod : std::uint8_t { None, Hat, Breve, Horn };

} // namespace vkcore
