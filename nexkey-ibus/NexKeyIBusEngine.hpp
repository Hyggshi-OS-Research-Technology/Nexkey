#pragma once

#include "vkcore/VKBuffer.hpp"

#include <cstdint>

namespace nexkey {

class NexKeyIBusEngine {
public:
    bool process_key_event(std::uint32_t keyval, std::uint32_t state);
    void toggle_mode();
    bool vietnamese_enabled() const;

private:
    vkcore::VKBuffer m_buffer;
    bool m_vietnameseEnabled{true};
};

} // namespace nexkey
