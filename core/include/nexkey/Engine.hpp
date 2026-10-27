// SPDX-License-Identifier: MIT
// NexKey - keystroke engine.

#pragma once

#include "nexkey/Types.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexkey {

/// Result of feeding one keystroke to the engine.
struct Step {
    /// Text the frontend must insert into the document right away.
    std::string commit;
    /// Current composition, already encoded for the configured character set.
    std::string preedit;
    /// False when the engine did not use the keystroke.
    bool handled{false};
};

/// One entry of the candidate window.
struct Candidate {
    /// Text that is committed when the entry is chosen, encoded like the
    /// preedit.
    std::string text;
    /// Short ASCII label shown next to the entry, may be empty.
    std::string comment;
};

/// Stateful keystroke engine: one instance per input context.
class Engine {
public:
    explicit Engine(Options options = {});

    [[nodiscard]] const Options& options() const noexcept { return m_options; }
    void set_options(const Options& options);

    /// Feed a keystroke given as a Unicode code point. Backspace (U+0008) is
    /// routed to backspace().
    [[nodiscard]] Step type(char32_t code_point);
    /// Convenience overload for a single byte keystroke; bytes above 0x7F are
    /// interpreted as Latin-1 code points.
    [[nodiscard]] Step type(char key)
    {
        return type(static_cast<char32_t>(static_cast<unsigned char>(key)));
    }

    /// Undo the last keystroke of the current composition.
    [[nodiscard]] Step backspace();

    /// Commit the current composition and start a new word.
    [[nodiscard]] Step flush();

    /// Drop the current composition without committing it.
    void discard();

    [[nodiscard]] bool composing() const noexcept { return !m_raw.empty(); }
    [[nodiscard]] std::string_view raw() const noexcept { return m_raw; }
    [[nodiscard]] std::string_view preedit() const noexcept { return m_preedit; }

    /// Render a full word of raw keystrokes without touching the state.
    [[nodiscard]] std::string render(std::string_view raw) const;

    /// True when the word typed so far is a plausible Vietnamese word.
    [[nodiscard]] bool plausible(std::string_view raw) const;

    /// Alternative interpretations of the current composition.
    ///
    /// The first entry is always the text shown as the preedit. The list is
    /// empty when the word is complete or when there is nothing to choose
    /// from.
    [[nodiscard]] std::vector<Candidate> candidates() const;

    /// Commit one specific candidate and start a new word.
    [[nodiscard]] Step commit_candidate(std::size_t index);

    /// Set macros (e.g. "vn" -> "Việt Nam", ":heart:" -> "❤️").
    void set_macros(std::unordered_map<std::string, std::string> macros)
    {
        m_macros = std::move(macros);
    }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& macros() const noexcept
    {
        return m_macros;
    }

private:
    void recompute();
    void reset_state();
    /// Macro lookup for the shorthand typed so far: exact match first, then the
    /// same text with ASCII letters lower cased. Returns nullptr when the
    /// shorthand is not a macro.
    [[nodiscard]] const std::string* find_macro(std::string_view raw) const;

    Options m_options;
    std::unordered_map<std::string, std::string> m_macros;
    std::string m_raw;
    std::string m_preedit;
    std::vector<std::string> m_history;
};

} // namespace nexkey
