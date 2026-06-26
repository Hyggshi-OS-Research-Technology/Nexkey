#include "NexKeyIBusEngine.hpp"

namespace nexkey {

bool NexKeyIBusEngine::process_key_event(std::uint32_t keyval, std::uint32_t state)
{
    constexpr std::uint32_t controlMask = 1u << 2u;
    constexpr std::uint32_t shiftMask = 1u;
    if ((state & controlMask) != 0 && (state & shiftMask) != 0) {
        toggle_mode();
        return true;
    }
    if (!m_vietnameseEnabled) {
        return false;
    }
    return m_buffer.process_key(keyval).consumed;
}

void NexKeyIBusEngine::toggle_mode()
{
    m_vietnameseEnabled = !m_vietnameseEnabled;
    m_buffer.reset();
}

bool NexKeyIBusEngine::vietnamese_enabled() const
{
    return m_vietnameseEnabled;
}

} // namespace nexkey
