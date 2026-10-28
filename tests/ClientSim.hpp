// SPDX-License-Identifier: MIT
// A client and an input method, small enough to test the whole direct typing
// path: the client owns a document and applies the edits it is given, the input
// method is the sequence of steps the Fcitx5 addon performs for one keystroke.
//
// The addon itself cannot be built without Fcitx5, so its decisions were never
// covered by a test before this file existed. The steps below are the ones
// NexKeyEngine::keyEvent performs, in the same order, and they are the part
// where a character can be lost: the engine produces text, the session turns it
// into an edit, the client applies the edit. Keeping that chain here means a
// regression in any of the three shows up as a wrong document.

#pragma once

#include "nexkey/Delivery.hpp"
#include "nexkey/Engine.hpp"
#include "nexkey/Unicode.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sim {

/// Drop the last `count` characters of a UTF-8 string.
inline void erase_last(std::string& text, std::size_t count)
{
    const std::size_t total = nexkey::unicode::count_code_points(text);
    if (count >= total) {
        text.clear();
        return;
    }
    text.resize(nexkey::unicode::prefix_length(text, total - count));
}

inline void append(std::string& text, char32_t code_point)
{
    nexkey::unicode::append(text, code_point);
}

/// How well the client treats the requests an input method can make.
enum class Behaviour {
    /// Deletes what it is asked to delete and reports its text accurately.
    Honest,
    /// Accepts deleteSurroundingText and does nothing with it. This is what
    /// Chromium, some Electron applications and every client that only ever
    /// gets forwarded keys look like from the outside.
    IgnoresDeletion,
    /// Reports the text as it was before the last edit, one round trip late.
    StaleReport,
    /// Reports nothing at all, which is what a client without surrounding text
    /// support does.
    NoSurroundingText,
};

/// The application window: a document and a cursor at its end.
class Client {
public:
    explicit Client(Behaviour behaviour = Behaviour::Honest, std::string initial = {})
        : m_behaviour(behaviour)
        , m_document(std::move(initial))
    {
    }

    /// Everything before the cursor, which is what a frontend reports.
    [[nodiscard]] std::string before_cursor() const
    {
        if (m_behaviour == Behaviour::NoSurroundingText) {
            return {};
        }
        if (m_behaviour == Behaviour::StaleReport && m_stale.size() <= m_document.size()) {
            return m_stale;
        }
        return m_document;
    }

    /// True when the report can be used to check an edit at all.
    [[nodiscard]] bool reports_text() const
    {
        return m_behaviour != Behaviour::NoSurroundingText;
    }

    /// Apply one edit the way a toolkit would: delete, then insert.
    void apply(const nexkey::Edit& edit)
    {
        const std::string previous = m_document;
        if (edit.delete_code_points > 0 && m_behaviour != Behaviour::IgnoresDeletion) {
            erase_last(m_document, edit.delete_code_points);
        }
        m_document += edit.commit;
        if (m_behaviour == Behaviour::StaleReport) {
            m_stale = previous;
        }
    }

    /// The client handled a BackSpace key itself: the document loses one
    /// character. This is the path that always works, in every toolkit.
    void forwarded_backspace()
    {
        const std::string previous = m_document;
        erase_last(m_document, 1);
        if (m_behaviour == Behaviour::StaleReport) {
            m_stale = previous;
        }
    }

    /// Catch up with the last edit, as a client does a frame later.
    void settle()
    {
        m_stale = m_document;
    }

    [[nodiscard]] const std::string& document() const { return m_document; }
    [[nodiscard]] std::size_t size() const
    {
        return nexkey::unicode::count_code_points(m_document);
    }

private:
    Behaviour m_behaviour;
    std::string m_document;
    /// The report a StaleReport client gives until it settles.
    std::string m_stale;
};

/// The steps NexKeyEngine::keyEvent takes in direct typing mode.
class InputMethod {
public:
    InputMethod() = default;
    explicit InputMethod(nexkey::Options options)
        : m_engine(std::move(options))
    {
    }

    /// Number of characters the last edit deleted, for the tests that care
    /// about what the client was asked to do.
    [[nodiscard]] int last_deletes() const { return m_last_deletes; }
    [[nodiscard]] std::string_view last_commit() const { return m_last_commit; }
    /// Repairs applied after a client was caught ignoring a deletion.
    [[nodiscard]] int repairs() const { return m_repairs; }
    /// True when the input method decided the document was not its own.
    [[nodiscard]] bool dropped() const { return m_dropped; }

    [[nodiscard]] const nexkey::Engine& engine() const { return m_engine; }

    /// One printable key, exactly as keyEvent handles it.
    void type(char32_t code_point, Client& client)
    {
        m_last_deletes = 0;
        m_last_commit.clear();
        verify(client);

        const auto step = m_engine.type(code_point);
        if (!step.handled) {
            // The keystroke is not for us: whatever was composed is committed
            // and the key goes to the application.
            commit_all(client);
            return;
        }
        if (!step.commit.empty()) {
            deliver(client, m_session.replace(step.commit));
            m_session.reset();
        }
        update(client);
    }

    /// One BackSpace, exactly as keyEvent handles it.
    void backspace(Client& client)
    {
        m_last_deletes = 0;
        m_last_commit.clear();
        verify(client);

        const auto step = m_engine.backspace();
        if (!step.handled) {
            commit_all(client);
            client.forwarded_backspace();
            return;
        }

        // A deletion at the end of the word lets the application do it. It is
        // the one deletion every toolkit gets right, and it needs no edit.
        const std::string next(m_engine.preedit());
        const std::string_view previous = m_session.text();
        if (next.size() < previous.size() &&
            previous.compare(0, next.size(), next) == 0 &&
            nexkey::unicode::count_code_points(previous) -
                    nexkey::unicode::count_code_points(next) ==
                1) {
            m_session.adopt(next);
            client.forwarded_backspace();
            return;
        }
        update(client);
    }

    /// A key that ends the word: space, Return, a candidate.
    void commit_text(std::string_view text, Client& client)
    {
        m_last_deletes = 0;
        m_last_commit.clear();
        verify(client);
        deliver(client, m_session.replace(text));
        m_session.reset();
        m_engine.discard();
        // The next word starts on the normal path again.
        m_prefer_keys = false;
    }

    /// Everything that is still composed, which in direct typing is nothing:
    /// the word is already in the document. The last edit of the word is
    /// checked first, because afterwards the mirror forgets what it asked for.
    void commit_all(Client& client)
    {
        verify(client);
        m_engine.discard();
        m_session.reset();
        m_prefer_keys = false;
    }

    /// Throw the word away without typing it.
    void discard(Client& client)
    {
        m_dropped = true;
        m_engine.discard();
        m_session.reset();
        client.settle();
    }

private:
    /// Check the last edit against the document, and repair it when the client
    /// verifiably ignored the deletion.
    void verify(Client& client)
    {
        if (!client.reports_text() || !m_session.has_pending()) {
            return;
        }
        const auto verification = m_session.verify(client.before_cursor());
        if (!verification.has_value()) {
            return;
        }
        switch (verification->outcome) {
        case nexkey::EditOutcome::Landed:
        case nexkey::EditOutcome::Unverifiable:
            return;
        case nexkey::EditOutcome::Unrelated:
            discard(client);
            return;
        case nexkey::EditOutcome::DeletionIgnored:
            // The request that failed cannot fix the word, so the repair goes
            // through forwarded keys, which every client handles.
            ++m_repairs;
            for (std::size_t index = 0; index < verification->repair.delete_code_points;
                 ++index) {
                client.forwarded_backspace();
            }
            m_last_commit = verification->repair.commit;
            if (!verification->repair.commit.empty()) {
                client.apply(nexkey::Edit{0, verification->repair.commit});
            }
            // ... and the same failure must not happen again for this word.
            m_prefer_keys = true;
            return;
        }
    }

    /// Apply one planned edit, with the mechanism this session uses.
    void deliver(Client& client, nexkey::Edit edit)
    {
        m_last_deletes = static_cast<int>(edit.delete_code_points);
        m_last_commit = edit.commit;
        if (m_prefer_keys) {
            // After a deletion the client ignored, the rest of the word is
            // typed the way every client gets right: as key presses.
            for (std::size_t index = 0; index < edit.delete_code_points; ++index) {
                client.forwarded_backspace();
            }
            if (!edit.commit.empty()) {
                client.apply(nexkey::Edit{0, edit.commit});
            }
            return;
        }
        client.apply(edit);
    }

    /// The preedit of the engine is the text the word should be; anything else
    /// is one more replacement.
    void update(Client& client)
    {
        const std::string next(m_engine.preedit());
        if (next != m_session.text()) {
            deliver(client, m_session.replace(next));
        }
    }

    nexkey::Engine m_engine;
    nexkey::DirectSession m_session;
    bool m_prefer_keys{false};
    int m_last_deletes{0};
    int m_repairs{0};
    bool m_dropped{false};
    std::string m_last_commit;
};

/// Type a sequence of ASCII keystrokes.
inline void type_all(InputMethod& method, Client& client, std::string_view keys)
{
    for (const char key : keys) {
        method.type(static_cast<unsigned char>(key), client);
    }
}

} // namespace sim
