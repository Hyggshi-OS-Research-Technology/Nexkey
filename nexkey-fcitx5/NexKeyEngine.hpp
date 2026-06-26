#pragma once

#include "vkcore/VKBuffer.hpp"

#include <cstdint>

namespace nexkey {

class NexKeyEngine {
public:
    bool process_key(std::uint32_t keyval);
    void set_enabled(bool enabled);

private:
    vkcore::VKBuffer m_buffer;
    bool m_enabled{true};
};

} // namespace nexkey
