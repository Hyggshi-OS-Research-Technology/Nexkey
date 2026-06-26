#include "NexKeyEngine.hpp"

namespace nexkey {

bool NexKeyEngine::process_key(std::uint32_t keyval)
{
    if (!m_enabled) {
        return false;
    }
    return m_buffer.process_key(keyval).consumed;
}

void NexKeyEngine::set_enabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        m_buffer.reset();
    }
}

} // namespace nexkey

extern "C" void* fcitx_module_init(void*)
{
    return new nexkey::NexKeyEngine();
}

extern "C" void fcitx_module_exit(void* engine)
{
    delete static_cast<nexkey::NexKeyEngine*>(engine);
}
