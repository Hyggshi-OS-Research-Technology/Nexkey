// SPDX-License-Identifier: MIT
// NexKey - one "delete then type" transaction, and the keys around it.
//
// Replacing the end of a word is two operations that travel through different
// queues: the helper presses BACKSPACE through the kernel, and the input method
// types the new text through the toolkit. Nothing guarantees their order, so
// the frontend must not type before it knows the deletions landed.
//
// ReplacementController is that bookkeeping, free of any toolkit:
//
//   1. start() with the number of backspaces and the text to type afterwards.
//   2. Every key event that arrives is offered to note_key(). The backspaces of
//      the burst are counted, a key press that is neither text nor a known
//      shortcut ends the wait early, and the barrier key finishes the burst.
//   3. finish() returns the text to type, or an empty string when the word has
//      to be abandoned.
//   4. If neither the count nor the barrier arrives in time, expire() says what
//      to do: never wait forever for a helper that died.
//
// Keys the user types during the ~50 ms of a burst are buffered instead of
// being applied in the middle of it, which would interleave two different texts
// in the document.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nexkey {

/// Where the wait for a burst stands.
enum class Burst : std::uint8_t {
    Idle,      ///< Nothing pending.
    Deleting,  ///< Waiting for the backspaces and the barrier.
    Draining,  ///< The text is already gone; the remaining burst keys are still
               ///< swallowed, because they will arrive whether we like it or not.
    Complete,  ///< The burst finished; the text may be typed.
    Expired,   ///< The deadline passed before it finished.
};

/// What a frontend should do with one key event while a burst is pending.
enum class KeyAction : std::uint8_t {
    Pass,       ///< Nothing pending: handle the key as usual.
    Swallow,    ///< It belongs to the burst: the input method must not process it
                ///< as typing. A burst BACKSPACE still has to reach the
                ///< application, because pressing it is what deletes the
                ///< character; the barrier must never reach it.
    BufferText, ///< Text typed during the wait: keep it for after the commit.
    FinishNow,  ///< A key that cannot wait: finish the burst, then handle it.
};

class ReplacementController {
public:
    /// How long a burst may take before it is considered lost. A burst of a few
    /// keys is well under 100 ms, so this is generous.
    static constexpr std::uint32_t kDefaultDeadlineMs = 400;

    /// How many characters typed during a burst are kept and replayed. More
    /// than a person can type in tens of milliseconds; the frontend ends the
    /// burst instead of dropping the extra key.
    static constexpr std::size_t kBufferLimit = 16;

    /// Begin a transaction. `backspaces` deletions are on their way, after which
    /// `text` is typed. Starting again while one is pending is a programming
    /// error and is treated as an immediate finish.
    void start(std::size_t backspaces, std::string text, std::uint64_t now_ms,
               std::uint32_t deadline_ms = kDefaultDeadlineMs);

    /// Offer a key event.
    ///
    /// `is_backspace` is true for the key the burst presses, `is_barrier` for
    /// the key that closes it, and `is_text` for a key that produces the
    /// character `code_point`. Everything else (Return, Tab, arrows, shortcuts)
    /// ends the wait at once, because a word must not be left half replaced
    /// while the user presses Enter.
    KeyAction note_key(bool is_backspace, bool is_barrier, bool is_text, bool is_shortcut,
                       char32_t code_point);

    /// The deadline check, to be called from a timer.
    Burst poll(std::uint64_t now_ms);

    /// Take the text to type and end the transaction. Empty when there is
    /// nothing to type, when the burst was abandoned, or when it expired.
    std::string finish();

    /// Take the text before the burst finished, for a key that cannot wait
    /// (Return, Tab, a shortcut). The remaining burst keys are still swallowed
    /// until they arrive: they were already pressed and cannot be recalled, and
    /// letting them reach the application unfiltered would delete text that has
    /// nothing to do with this word.
    std::string finish_early();

    /// Give up on the transaction without typing anything.
    void abandon();

    [[nodiscard]] Burst state() const noexcept { return m_state; }
    [[nodiscard]] bool pending() const noexcept
    {
        return m_state == Burst::Deleting || m_state == Burst::Draining;
    }
    /// True while keys of a submitted burst still have to be swallowed.
    [[nodiscard]] bool draining() const noexcept { return m_state == Burst::Draining; }
    /// True while the barrier key of a burst is still expected. The frontend
    /// uses it to swallow the release of that key as well.
    [[nodiscard]] bool barrier_expected() const noexcept { return m_barrier_pending; }
    /// Backspaces still expected.
    [[nodiscard]] std::size_t remaining() const noexcept { return m_remaining; }
    /// The text that will be typed when the burst finishes.
    [[nodiscard]] std::string_view text() const noexcept { return m_text; }
    /// True when the burst finished because the deadline passed rather than
    /// because the keys arrived.
    [[nodiscard]] bool expired() const noexcept { return m_expired; }
    /// Keys the user typed during the burst, in order.
    [[nodiscard]] const std::u32string& buffered() const noexcept { return m_buffered; }
    [[nodiscard]] std::u32string take_buffered();

    /// Put characters back in front of the buffer, in order. A frontend uses
    /// this when replaying the buffer had to stop because the replay itself
    /// started another burst.
    void restore(std::u32string_view text);

private:
    Burst m_state{Burst::Idle};
    std::size_t m_remaining{0};
    std::string m_text;
    std::u32string m_buffered;
    std::uint64_t m_deadline_ms{0};
    bool m_expired{false};
    /// The barrier closed the burst: the text may be typed even if the count
    /// says otherwise, because the count is a guess and the barrier is not.
    bool m_barrier{false};
    /// A barrier key is still expected: it has to be swallowed even after the
    /// count closed the burst, because it is already on its way through the
    /// kernel when the last backspace arrives.
    bool m_barrier_pending{false};
};

/// Coalesces the text of several commits into one.
///
/// Typing a word ends with one commit per keystroke in some paths, and every
/// commit is a round trip to the application. Merging the commits of one key
/// event turns a burst of them into one, which is what slow clients (a browser
/// tab, a remote desktop) need. `kWindowMs` bounds how long a frontend that
/// cannot tell when a burst ends may wait for more; a frontend that flushes at
/// the end of every key event, as the Fcitx5 addon does, never waits at all.
class CommitBatcher {
public:
    /// Flush no later than this after the first pending commit.
    static constexpr std::uint64_t kWindowMs = 12;
    /// ...or sooner than that when the batch is already this large.
    static constexpr std::size_t kMaxPendingBytes = 256;

    void append(std::string_view text, std::uint64_t now_ms);

    /// True when the pending batch must be sent now.
    [[nodiscard]] bool due(std::uint64_t now_ms) const noexcept;

    /// Take the pending text.
    std::string take();

    [[nodiscard]] bool empty() const noexcept { return m_pending.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return m_pending.size(); }
    /// The moment the batch becomes due, for a timer to aim at.
    [[nodiscard]] std::uint64_t due_at() const noexcept { return m_due_at; }

private:
    std::string m_pending;
    std::uint64_t m_due_at{0};
};

} // namespace nexkey
