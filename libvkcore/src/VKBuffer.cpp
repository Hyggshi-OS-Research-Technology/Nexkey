#include "vkcore/VKBuffer.hpp"

#include "VKCommon.hpp"
#include "vkcore/VKEncoding.hpp"
#include "vkcore/VKTelex.hpp"
#include "vkcore/VKVni.hpp"
#include "vkcore/VKViqr.hpp"

namespace vkcore {

namespace {

bool is_printable_ascii(std::uint32_t keyval)
{
    return keyval >= 0x20 && keyval <= 0x7e;
}

} // namespace

VKBuffer::VKBuffer(InputMethod method, Encoding enc)
    : m_method(method)
    , m_encoding(enc)
{
}

ProcessResult VKBuffer::process_key(std::uint32_t keyval, bool is_backspace)
{
    if (is_backspace || keyval == 0xff08) {
        if (!m_buffer.empty()) {
            if (m_smartBackspace && !m_history.empty()) {
                m_buffer = m_history.back();
                m_history.pop_back();
            } else {
                m_buffer.pop_back();
            }
            m_output = apply(m_buffer);
            return {"", encode(m_output), true};
        }
        return {"", "", false};
    }

    if (is_word_boundary(keyval)) {
        const char boundary = keyval == '\r' ? '\n' : static_cast<char>(keyval);
        const std::string committed = encode(apply(m_buffer)) + boundary;
        reset();
        return {committed, "", true};
    }

    if (!is_printable_ascii(keyval)) {
        return {"", "", false};
    }

    m_history.push_back(m_buffer);
    m_buffer.push_back(static_cast<char>(keyval));
    m_output = apply(m_buffer);
    return {"", encode(m_output), true};
}

void VKBuffer::reset()
{
    m_buffer.clear();
    m_output.clear();
    m_history.clear();
}

std::string VKBuffer::flush()
{
    const auto out = encode(apply(m_buffer));
    reset();
    return out;
}

void VKBuffer::set_method(InputMethod m)
{
    m_method = m;
    m_output = apply(m_buffer);
}

void VKBuffer::set_encoding(Encoding e)
{
    m_encoding = e;
}

void VKBuffer::set_smart_backspace(bool enabled)
{
    m_smartBackspace = enabled;
}

InputMethod VKBuffer::method() const
{
    return m_method;
}

Encoding VKBuffer::encoding() const
{
    return m_encoding;
}

bool VKBuffer::smart_backspace() const
{
    return m_smartBackspace;
}

std::string VKBuffer::apply(const std::string& raw) const
{
    switch (m_method) {
    case InputMethod::Telex:
        return apply_telex_rules(raw);
    case InputMethod::VNI:
        return apply_vni_rules(raw);
    case InputMethod::VIQR:
        return apply_viqr_rules(raw);
    }
    return raw;
}

std::string VKBuffer::encode(const std::string& utf8) const
{
    switch (m_encoding) {
    case Encoding::Unicode:
        return utf8;
    case Encoding::TCVN3:
        return VKEncoding::to_tcvn3(utf8);
    case Encoding::VNIEncoding:
        return VKEncoding::to_vni(utf8);
    }
    return utf8;
}

bool is_word_boundary(std::uint32_t keyval)
{
    if (keyval == ' ' || keyval == '\n' || keyval == '\r' || keyval == '\t') {
        return true;
    }
    switch (keyval) {
    case '.':
    case ',':
    case ';':
    case ':':
    case '!':
    case '?':
    case ')':
    case ']':
    case '}':
    case '"':
        return true;
    default:
        return false;
    }
}

} // namespace vkcore
