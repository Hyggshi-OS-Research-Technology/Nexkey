#pragma once

#include "VKTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace vkcore {

struct ProcessResult {
    std::string committed;
    std::string preedit;
    bool consumed{false};
};

class VKBuffer {
public:
    explicit VKBuffer(InputMethod method = InputMethod::Telex,
                      Encoding enc = Encoding::Unicode);

    ProcessResult process_key(std::uint32_t keyval, bool is_backspace = false);
    void reset();
    std::string flush();

    void set_method(InputMethod m);
    void set_encoding(Encoding e);
    void set_smart_backspace(bool enabled);
    InputMethod method() const;
    Encoding encoding() const;
    bool smart_backspace() const;

private:
    std::string m_buffer;
    std::string m_output;
    InputMethod m_method;
    Encoding m_encoding;
    bool m_smartBackspace{true};
    std::vector<std::string> m_history;

    std::string apply(const std::string& raw) const;
    std::string encode(const std::string& utf8) const;
};

bool is_word_boundary(std::uint32_t keyval);

} // namespace vkcore
