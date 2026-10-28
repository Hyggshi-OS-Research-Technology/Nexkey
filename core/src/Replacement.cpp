// SPDX-License-Identifier: MIT
// NexKey - the state machine of one delete-then-type transaction.

#include "nexkey/Replacement.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace nexkey {

namespace {

/// The burst keys have to be swallowed in every state that still expects them.
bool expects_keys(Burst state) noexcept
{
    return state == Burst::Deleting || state == Burst::Draining;
}

} // namespace

void ReplacementController::start(std::size_t backspaces, std::string text, std::uint64_t now_ms,
                                  std::uint32_t deadline_ms)
{
    // A caller that starts a second transaction without finishing the first has
    // left text in the document that nothing will ever type. Dropping the old
    // one is the only safe interpretation, and the new burst is self contained.
    m_buffered.clear();
    m_text = std::move(text);
    m_remaining = backspaces;
    m_barrier = false;
    // Every burst ends with a barrier, so one is due from now on until it
    // arrives.
    m_barrier_pending = backspaces > 0;
    m_expired = false;
    m_deadline_ms = now_ms + deadline_ms;

    // Nothing to delete means there is nothing to wait for: the text can be
    // typed at once, which is the common case of appending a letter.
    m_state = backspaces == 0 ? Burst::Complete : Burst::Deleting;
}

KeyAction ReplacementController::note_key(bool is_backspace, bool is_barrier, bool is_text,
                                          bool is_shortcut, char32_t code_point)
{
    // The barrier comes first and is handled even when the transaction already
    // ended: the count may have closed the burst a few microseconds earlier,
    // while the barrier was already travelling through the kernel. It must not
    // reach the application either way.
    if (is_barrier) {
        if (!m_barrier_pending && !expects_keys(m_state)) {
            return KeyAction::Pass;
        }
        m_barrier_pending = false;
        m_remaining = 0;
        if (m_state == Burst::Deleting) {
            m_barrier = true;
            m_state = Burst::Complete;
        } else if (m_state == Burst::Draining) {
            m_state = Burst::Idle;
        }
        // Complete, Idle and Expired keep their state: the text was handed out
        // already, or given up on.
        return KeyAction::Swallow;
    }

    if (!expects_keys(m_state)) {
        return KeyAction::Pass;
    }

    if (is_backspace && m_remaining > 0) {
        --m_remaining;
        if (m_remaining == 0) {
            m_state = m_state == Burst::Deleting ? Burst::Complete : Burst::Idle;
        }
        // The application still has to see it: the whole point of the burst is
        // that the application deletes the character itself.
        return KeyAction::Swallow;
    }

    if (m_state == Burst::Draining) {
        return KeyAction::Pass;
    }

    if (is_shortcut || !is_text) {
        return KeyAction::FinishNow;
    }

    // A character typed while the deletions are still travelling. Applying it
    // now would put it in the middle of the burst, so it waits its turn.
    if (m_buffered.size() >= kBufferLimit) {
        // A person cannot type this many characters in the tens of
        // milliseconds a burst takes, so something is wrong. Ending the burst
        // is better than dropping the key: the caller types what it has and
        // handles this key on the normal path.
        return KeyAction::FinishNow;
    }
    m_buffered.push_back(code_point);
    return KeyAction::BufferText;
}

Burst ReplacementController::poll(std::uint64_t now_ms)
{
    if (!expects_keys(m_state) || now_ms < m_deadline_ms) {
        return m_state;
    }

    if (m_state == Burst::Draining) {
        // Nothing is waiting for the text any more, so the burst simply ends.
        m_state = Burst::Idle;
        m_remaining = 0;
        return m_state;
    }

    // The helper never answered, or the barrier was lost. The deletions may or
    // may not have happened, so the transaction is not trusted: the caller
    // drops the word rather than typing text on top of an unknown document.
    // `expired()` says so, and the frontend stops using this path for a while.
    m_expired = true;
    m_state = Burst::Expired;
    m_remaining = 0;
    // After the deadline nothing that arrives can be ours to swallow.
    m_barrier_pending = false;
    m_text.clear();
    return m_state;
}

std::string ReplacementController::finish()
{
    std::string text;
    if (m_state == Burst::Complete) {
        text = std::move(m_text);
    }
    m_text.clear();
    m_state = Burst::Idle;
    m_remaining = 0;
    m_barrier = false;
    return text;
}

std::string ReplacementController::finish_early()
{
    if (m_state != Burst::Deleting) {
        return finish();
    }

    std::string text = std::move(m_text);
    m_text.clear();
    m_barrier = false;
    if (m_remaining > 0) {
        // The helper already pressed some of the keys, or is about to. They are
        // swallowed as they arrive so that the application never sees a burst
        // that belongs to a word nobody is waiting for any more.
        m_state = Burst::Draining;
        m_deadline_ms = m_deadline_ms + kDefaultDeadlineMs;
    } else {
        m_state = Burst::Idle;
    }
    return text;
}

void ReplacementController::abandon()
{
    // Whatever the user typed ahead belongs to a word that is being given up,
    // so it is dropped either way.
    m_buffered.clear();
    m_text.clear();
    m_barrier = false;

    if (m_state == Burst::Deleting && m_remaining > 0) {
        // Same reasoning as finish_early(): the keys are on their way and have
        // to be eaten even though nobody wants their effect any more.
        m_state = Burst::Draining;
        m_deadline_ms = m_deadline_ms + kDefaultDeadlineMs;
        return;
    }
    m_state = Burst::Idle;
    m_remaining = 0;
    m_expired = false;
}

std::u32string ReplacementController::take_buffered()
{
    std::u32string out = std::move(m_buffered);
    m_buffered.clear();
    return out;
}

void ReplacementController::restore(std::u32string_view text)
{
    if (text.empty()) {
        return;
    }
    m_buffered.insert(0, text.data(), text.size());
}

void CommitBatcher::append(std::string_view text, std::uint64_t now_ms)
{
    if (text.empty()) {
        return;
    }
    if (m_pending.empty()) {
        m_due_at = now_ms + kWindowMs;
    }
    m_pending += text;
}

bool CommitBatcher::due(std::uint64_t now_ms) const noexcept
{
    if (m_pending.empty()) {
        return false;
    }
    return now_ms >= m_due_at || m_pending.size() >= kMaxPendingBytes;
}

std::string CommitBatcher::take()
{
    std::string out = std::move(m_pending);
    m_pending.clear();
    m_due_at = 0;
    return out;
}

} // namespace nexkey
