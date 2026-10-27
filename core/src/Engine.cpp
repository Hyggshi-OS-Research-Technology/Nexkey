// SPDX-License-Identifier: MIT
// NexKey - keystroke engine.

#include "nexkey/Engine.hpp"

#include "Word.hpp"
#include "Syllable.hpp"
#include "nexkey/Converter.hpp"
#include "nexkey/Unicode.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>

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

const std::string* Engine::find_macro(std::string_view raw) const
{
    if (auto it = m_macros.find(std::string(raw)); it != m_macros.end()) {
        return &it->second;
    }
    // Shorthand matching is ASCII, so it must not depend on the locale: under a
    // Turkish locale std::tolower('I') is a dotted i and "IBUS" would stop
    // matching "ibus". Lowering the ASCII range by hand is what the
    // configuration file means.
    std::string lowered(raw);
    for (char& c : lowered) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    if (auto it = m_macros.find(lowered); it != m_macros.end()) {
        return &it->second;
    }
    return nullptr;
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

namespace {

/// Render a word of raw keystrokes with an explicit option set.
std::string render_with(std::string_view raw, const Options& options)
{
    if (raw.empty()) {
        return {};
    }
    if (raw.front() == ':' || raw.front() == '<' || raw.front() == '(') {
        return std::string(raw);
    }
    const auto word = detail::parse(options.method, raw, options);
    if (options.english_fallback && !word.escaped &&
        !detail::is_plausible(word.to_letters())) {
        return std::string(raw);
    }
    return Converter::encode(options.encoding, detail::compose_word(word, options));
}

bool contains(const std::vector<Candidate>& candidates, const std::string& text)
{
    return std::ranges::any_of(candidates, [&text](const Candidate& candidate) {
        return candidate.text == text;
    });
}

std::string expand_macro_text(std::string text, std::string_view raw, const Options& options)
{
    if (text.empty()) {
        return text;
    }

    if (text.find("$TIME") != std::string::npos || text.find("$DATE") != std::string::npos) {
        const auto now = std::chrono::system_clock::now();
        const auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&time_t, &tm);

        char time_buf[32];
        std::strftime(time_buf, sizeof(time_buf), "%H:%M", &tm);

        char date_buf[32];
        std::strftime(date_buf, sizeof(date_buf), "%d/%m/%Y", &tm);

        std::size_t pos = 0;
        while ((pos = text.find("$TIME", pos)) != std::string::npos) {
            text.replace(pos, 5, time_buf);
            pos += std::strlen(time_buf);
        }
        pos = 0;
        while ((pos = text.find("$DATE", pos)) != std::string::npos) {
            text.replace(pos, 5, date_buf);
            pos += std::strlen(date_buf);
        }
    }

    if (options.capitalize_macro && !raw.empty()) {
        bool all_upper = true;
        for (char c : raw) {
            if (std::isalpha(static_cast<unsigned char>(c)) && !std::isupper(static_cast<unsigned char>(c))) {
                all_upper = false;
                break;
            }
        }
        // Case mapping goes through the Unicode backend: uppercasing the bytes
        // of a macro only works for ASCII, so a Vietnamese macro would come out
        // with its accented letters unchanged.
        if (all_upper && raw.size() >= 2) {
            text = unicode::upper(text);
        } else if (std::isupper(static_cast<unsigned char>(raw.front())) && !text.empty()) {
            text = unicode::upper_first(text);
        }
    }
    return text;
}

} // namespace

std::string Engine::render(std::string_view raw) const
{
    return render_with(raw, m_options);
}

std::vector<Candidate> Engine::candidates() const
{
    std::vector<Candidate> result;
    if (m_raw.empty() || !m_options.candidates) {
        return result;
    }

    const auto word = detail::parse(m_options.method, m_raw, m_options);
    const bool plausible =
        word.escaped || detail::is_plausible(word.to_letters());

    result.push_back(Candidate{m_preedit, {}});

    if (plausible) {
        Options alternative = m_options;
        alternative.tone_style = m_options.tone_style == ToneStyle::Modern
                                     ? ToneStyle::Traditional
                                     : ToneStyle::Modern;
        if (const auto text = render_with(m_raw, alternative); !contains(result, text)) {
            result.push_back(Candidate{text, std::string(to_string(alternative.tone_style))});
        }
    }

    // The raw keystrokes are the last resort when the rules mangle a word.
    if (!contains(result, m_raw)) {
        result.push_back(Candidate{m_raw, std::string("raw")});
    }

    if (result.size() < 2) {
        return {};
    }
    return result;
}

Step Engine::commit_candidate(std::size_t index)
{
    const auto options = candidates();
    if (options.empty() || index >= options.size()) {
        return Step{};
    }

    Step step;
    step.commit = options[index].text;
    step.handled = !step.commit.empty();
    reset_state();
    return step;
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

    if (m_raw.empty() && (code_point == ':' || code_point == '<' || code_point == '(')) {
        m_history.push_back(m_raw);
        m_raw.push_back(static_cast<char>(code_point));
        m_preedit = m_raw;
        return Step{std::string{}, m_preedit, true};
    }

    if (!m_raw.empty() && m_raw.front() == ':') {
        if (code_point == ':') {
            m_raw.push_back(':');
            Step step;
            if (const std::string* macro = find_macro(m_raw); macro != nullptr) {
                step.commit = *macro;
            } else {
                step.commit = m_raw;
            }
            step.handled = true;
            reset_state();
            return step;
        }
        if (code_point == ' ' || code_point == '\t') {
            Step step;
            if (const std::string* macro = find_macro(m_raw); macro != nullptr) {
                step.commit = *macro;
            } else {
                step.commit = m_raw;
            }
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

    if (!m_raw.empty() && (m_raw.front() == '<' || m_raw.front() == '(')) {
        if (code_point == ' ' || code_point == '\t') {
            Step step;
            if (auto it = m_macros.find(m_raw); it != m_macros.end()) {
                step.commit = it->second;
            } else {
                step.commit = m_raw;
            }
            step.commit.push_back(static_cast<char>(code_point));
            step.handled = true;
            reset_state();
            return step;
        }
        if (m_raw.size() < 6) {
            m_history.push_back(m_raw);
            m_raw.push_back(static_cast<char>(code_point));
            recompute();
            return Step{std::string{}, m_preedit, true};
        }
    }

    if (detail::is_word_boundary(code_point, m_options)) {
        Step step;
        std::string word = m_preedit;
        if (m_options.macro_enabled && !m_macros.empty()) {
            if (auto it = m_macros.find(m_raw); it != m_macros.end()) {
                word = expand_macro_text(it->second, m_raw, m_options);
            } else if (auto it_pre = m_macros.find(m_preedit); it_pre != m_macros.end()) {
                word = expand_macro_text(it_pre->second, m_raw, m_options);
            } else if (const std::string* macro = find_macro(m_raw); macro != nullptr) {
                word = expand_macro_text(*macro, m_raw, m_options);
            }
        }
        step.commit = std::move(word);
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
