// SPDX-License-Identifier: MIT
// NexKey - keystroke engine.

#include "nexkey/Engine.hpp"

#include "Word.hpp"
#include "Syllable.hpp"
#include "nexkey/Converter.hpp"
#include "nexkey/Unicode.hpp"

namespace nexkey {

Engine::Engine(Options options)
    : m_options(options)
{
}

void Engine::set_options(const Options& options)
{
    m_options = options;
    recompute();
}

void Engine::recompute()
{
    m_preedit = render(m_raw);
}

void Engine::reset_state()
{
    m_raw.clear();
    m_preedit.clear();
    m_history.clear();
}

void Engine::discard()
{
    reset_state();
}

std::string Engine::render(std::string_view raw) const
{
    if (raw.empty()) {
        return {};
    }
    const auto word = detail::parse(m_options.method, raw, m_options);
    if (m_options.english_fallback && !word.escaped &&
        !detail::is_plausible(word.to_letters())) {
        return std::string(raw);
    }
    return Converter::encode(m_options.encoding, detail::compose_word(word, m_options));
}

bool Engine::plausible(std::string_view raw) const
{
    const auto word = detail::parse(m_options.method, raw, m_options);
    return detail::is_plausible(word.to_letters());
}

Step Engine::type(char32_t code_point)
{
    if (code_point == U'\b') {
        return backspace();
    }

    if (code_point < 0x20 || code_point == 0x7F) {
        // Control keys end the composition but stay available to the frontend.
        Step step;
        step.commit = m_preedit;
        step.handled = false;
        reset_state();
        return step;
    }

    if (code_point > 0x7F) {
        // Text that is not a plain key: commit it as it is.
        Step step;
        step.commit = m_preedit;
        unicode::append(step.commit, code_point);
        step.handled = true;
        reset_state();
        return step;
    }

    if (detail::is_word_boundary(code_point, m_options)) {
        Step step;
        step.commit = m_preedit;
        step.commit.push_back(static_cast<char>(code_point));
        step.handled = true;
        reset_state();
        return step;
    }

    m_history.push_back(m_raw);
    m_raw.push_back(static_cast<char>(code_point));
    recompute();
    return Step{std::string{}, m_preedit, true};
}

Step Engine::backspace()
{
    if (m_raw.empty()) {
        return Step{};
    }

    const auto previous_preedit = m_preedit;
    if (m_options.smart_backspace && !m_history.empty()) {
        m_raw = std::move(m_history.back());
        m_history.pop_back();
        recompute();
        // Keep deleting while the visible text does not change, so that
        // keystrokes without a visible effect never look like a no-op.
        while (!m_raw.empty() && m_preedit == previous_preedit) {
            if (m_history.empty()) {
                m_raw.pop_back();
            } else {
                m_raw = std::move(m_history.back());
                m_history.pop_back();
            }
            recompute();
        }
    } else {
        m_raw.pop_back();
        recompute();
    }
    return Step{std::string{}, m_preedit, true};
}

Step Engine::flush()
{
    if (m_raw.empty()) {
        return Step{};
    }
    Step step;
    step.commit = m_preedit;
    step.handled = !step.commit.empty();
    reset_state();
    return step;
}

} // namespace nexkey
