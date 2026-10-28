// SPDX-License-Identifier: MIT
// One delete-then-type transaction: the barrier, the deadline and the keys that
// arrive in the middle of it.

#include "TestHarness.hpp"

#include "nexkey/Replacement.hpp"

#include <cstdint>
#include <string>
#include <string_view>

using nexkey::Burst;
using nexkey::CommitBatcher;
using nexkey::KeyAction;
using nexkey::ReplacementController;

namespace {

constexpr char32_t kT{0x74};

KeyAction text(ReplacementController& controller, char32_t code_point = kT)
{
    return controller.note_key(false, false, true, false, code_point);
}

KeyAction backspace(ReplacementController& controller)
{
    return controller.note_key(true, false, false, false, 0);
}

KeyAction barrier(ReplacementController& controller)
{
    return controller.note_key(false, true, false, false, 0);
}

KeyAction shortcut(ReplacementController& controller)
{
    return controller.note_key(false, false, false, true, 0);
}

} // namespace

TEST(replacement_appending_needs_no_burst)
{
    ReplacementController controller;
    controller.start(0, "kha", 1000);
    // Nothing to delete, so the text can be typed at once.
    EXPECT_TRUE(controller.state() == Burst::Complete);
    EXPECT_FALSE(controller.pending());
    EXPECT_EQ(controller.finish(), std::string("kha"));
    EXPECT_TRUE(controller.state() == Burst::Idle);
    // Only once: the second call has nothing to give.
    EXPECT_EQ(controller.finish(), std::string());
}

TEST(replacement_waits_for_the_backspaces)
{
    ReplacementController controller;
    controller.start(2, "kh\u1EA3", 1000);
    EXPECT_TRUE(controller.pending());
    EXPECT_EQ(controller.remaining(), std::size_t{2});

    // The first backspace of the burst belongs to the burst.
    EXPECT_TRUE(backspace(controller) == KeyAction::Swallow);
    EXPECT_TRUE(controller.pending());
    EXPECT_EQ(controller.remaining(), std::size_t{1});

    EXPECT_TRUE(backspace(controller) == KeyAction::Swallow);
    EXPECT_FALSE(controller.pending());
    EXPECT_TRUE(controller.state() == Burst::Complete);
    EXPECT_EQ(controller.finish(), std::string("kh\u1EA3"));
}

TEST(replacement_barrier_ends_the_wait_without_trusting_the_count)
{
    // A burst whose events were coalesced by a toolkit: the count never
    // arrives, but the barrier does, and the barrier is the one that matters.
    ReplacementController controller;
    controller.start(3, "ho\u00E0", 1000);
    EXPECT_TRUE(backspace(controller) == KeyAction::Swallow);
    EXPECT_TRUE(barrier(controller) == KeyAction::Swallow);
    EXPECT_TRUE(controller.state() == Burst::Complete);
    EXPECT_EQ(controller.remaining(), std::size_t{0});
    EXPECT_EQ(controller.finish(), std::string("ho\u00E0"));
}

TEST(replacement_says_when_the_barrier_is_still_expected)
{
    ReplacementController controller;
    EXPECT_FALSE(controller.barrier_expected());

    // A burst always ends with the barrier, so one is expected from the start
    // and until it arrives, even after the count closed the burst.
    controller.start(1, "a", 1000);
    EXPECT_TRUE(controller.barrier_expected());
    static_cast<void>(backspace(controller));
    EXPECT_TRUE(controller.state() == Burst::Complete);
    EXPECT_TRUE(controller.barrier_expected());
    static_cast<void>(barrier(controller));
    EXPECT_FALSE(controller.barrier_expected());

    // A transaction without deletions sends no keys and expects no barrier.
    controller.start(0, "b", 2000);
    EXPECT_FALSE(controller.barrier_expected());
}

TEST(replacement_swallows_the_barrier)
{
    // The barrier must never reach the application: it is a pause key that
    // exists only to tell the frontend the burst is over.
    ReplacementController controller;
    controller.start(1, "a", 1000);
    EXPECT_TRUE(barrier(controller) == KeyAction::Swallow);
    EXPECT_FALSE(controller.pending());
}

TEST(replacement_buffers_text_typed_during_the_burst)
{
    ReplacementController controller;
    controller.start(2, "c", 1000);

    // A fast typist keeps going while the deletions travel through the kernel.
    EXPECT_TRUE(text(controller, kT) == KeyAction::BufferText);
    EXPECT_TRUE(text(controller, char32_t{0x65}) == KeyAction::BufferText);
    EXPECT_EQ(controller.buffered(), std::u32string{U"te"});

    static_cast<void>(backspace(controller));
    static_cast<void>(backspace(controller));
    EXPECT_EQ(controller.finish(), std::string("c"));

    // The buffered keys are replayed afterwards, in the order they were typed.
    EXPECT_EQ(controller.take_buffered(), std::u32string{U"te"});
    EXPECT_TRUE(controller.buffered().empty());
}

TEST(replacement_buffer_does_not_grow_without_bound)
{
    ReplacementController controller;
    controller.start(2, "c", 1000);
    for (int index = 0; index < 40; ++index) {
        static_cast<void>(text(controller, char32_t{0x61}));
    }
    // A burst lasts tens of milliseconds, so this is far more than a person
    // can type; the buffer stops instead of swallowing the session.
    EXPECT_EQ(controller.buffered().size(), std::size_t{16});
}

TEST(replacement_ends_at_once_for_a_key_that_cannot_wait)
{
    ReplacementController controller;
    controller.start(3, "kha", 1000);
    EXPECT_TRUE(backspace(controller) == KeyAction::Swallow);

    // Enter, Tab, an arrow or a shortcut must not be held back while the burst
    // finishes: the caller takes the text and handles the key.
    EXPECT_TRUE(shortcut(controller) == KeyAction::FinishNow);
    EXPECT_EQ(controller.finish_early(), std::string("kha"));

    // Two deletions were already requested and will arrive. They are swallowed
    // so the application does not delete a character the user never asked for.
    EXPECT_TRUE(controller.draining());
    EXPECT_TRUE(backspace(controller) == KeyAction::Swallow);
    EXPECT_TRUE(backspace(controller) == KeyAction::Swallow);
    EXPECT_TRUE(controller.state() == Burst::Idle);

    // Nothing is left to type a second time.
    EXPECT_EQ(controller.finish(), std::string());
}

TEST(replacement_draining_ends_at_the_barrier_too)
{
    ReplacementController controller;
    controller.start(4, "mot", 1000);
    EXPECT_EQ(controller.finish_early(), std::string("mot"));
    EXPECT_TRUE(controller.draining());
    // The barrier also ends a drained burst: the count is a guess, the barrier
    // is not.
    EXPECT_TRUE(barrier(controller) == KeyAction::Swallow);
    EXPECT_TRUE(controller.state() == Burst::Idle);
}

TEST(replacement_draining_passes_ordinary_keys_through)
{
    ReplacementController controller;
    controller.start(2, "mot", 1000);
    static_cast<void>(controller.finish_early());
    // The user is typing again while the last deletions land: those keys are
    // not part of the burst and must reach the application.
    EXPECT_TRUE(text(controller) == KeyAction::Pass);
    EXPECT_TRUE(shortcut(controller) == KeyAction::Pass);
}

TEST(replacement_draining_gives_up_at_its_own_deadline)
{
    ReplacementController controller;
    controller.start(2, "mot", 1000, 100);
    static_cast<void>(controller.finish_early());
    // A drained burst waits for its keys, but not forever.
    EXPECT_TRUE(controller.poll(1200) == Burst::Draining);
    EXPECT_TRUE(controller.poll(1600) == Burst::Idle);
    EXPECT_FALSE(controller.expired());
}

TEST(replacement_swallows_a_barrier_that_arrives_after_the_count)
{
    ReplacementController controller;
    controller.start(1, "a", 1000);
    EXPECT_TRUE(backspace(controller) == KeyAction::Swallow);
    EXPECT_TRUE(controller.state() == Burst::Complete);
    EXPECT_EQ(controller.finish(), std::string("a"));

    // The barrier was already travelling through the kernel when the last
    // backspace arrived. It is still ours, and it still must not reach the
    // application.
    EXPECT_TRUE(barrier(controller) == KeyAction::Swallow);
    // Only that one: a second pause key is the user's, not ours.
    EXPECT_TRUE(barrier(controller) == KeyAction::Pass);
}

TEST(replacement_full_buffer_ends_the_burst_instead_of_losing_the_key)
{
    ReplacementController controller;
    controller.start(2, "c", 1000);
    for (std::size_t index = 0; index < ReplacementController::kBufferLimit; ++index) {
        EXPECT_TRUE(text(controller) == KeyAction::BufferText);
    }
    // The character over the limit is not swallowed: the burst ends, the caller
    // types what it holds and handles this key on the normal path.
    EXPECT_TRUE(text(controller) == KeyAction::FinishNow);
    EXPECT_EQ(controller.buffered().size(), ReplacementController::kBufferLimit);
}

TEST(replacement_restore_puts_keys_back_in_front)
{
    ReplacementController controller;
    controller.start(1, "c", 1000);
    static_cast<void>(text(controller, char32_t{0x61}));
    static_cast<void>(text(controller, char32_t{0x62}));
    const std::u32string taken = controller.take_buffered();
    EXPECT_EQ(taken, std::u32string{U"ab"});

    // The characters that were not replayed yet go back in front, in order.
    controller.restore(taken);
    static_cast<void>(text(controller, char32_t{0x63}));
    EXPECT_EQ(controller.take_buffered(), std::u32string{U"abc"});

    // Restoring nothing changes nothing.
    controller.restore(std::u32string_view{});
    EXPECT_TRUE(controller.buffered().empty());
}

TEST(replacement_expires_instead_of_hanging)
{
    ReplacementController controller;
    controller.start(2, "kh\u1EA3", 1000, 400);
    EXPECT_TRUE(controller.poll(1200) == Burst::Deleting);
    EXPECT_TRUE(controller.poll(1400) == Burst::Expired);
    EXPECT_TRUE(controller.expired());

    // The burst may have deleted nothing, one character or both, so the text
    // cannot be typed on top of an unknown document.
    EXPECT_EQ(controller.finish(), std::string());
    EXPECT_TRUE(controller.state() == Burst::Idle);
}

TEST(replacement_poll_keeps_a_finished_burst_finished)
{
    ReplacementController controller;
    controller.start(1, "a", 1000, 100);
    static_cast<void>(barrier(controller));
    // A late timer must not turn a finished burst into an expired one.
    EXPECT_TRUE(controller.poll(5000) == Burst::Complete);
    EXPECT_FALSE(controller.expired());
    EXPECT_EQ(controller.finish(), std::string("a"));
}

TEST(replacement_abandon_clears_everything)
{
    ReplacementController controller;
    controller.start(2, "abc", 1000);
    static_cast<void>(text(controller));
    controller.abandon();
    EXPECT_TRUE(controller.state() == Burst::Draining);
    // The two deletions were already asked for, so the keys are still eaten...
    EXPECT_TRUE(backspace(controller) == KeyAction::Swallow);
    EXPECT_TRUE(backspace(controller) == KeyAction::Swallow);
    // ... and once they are gone nothing is left over.
    EXPECT_TRUE(controller.state() == Burst::Idle);
    EXPECT_TRUE(controller.buffered().empty());
    EXPECT_EQ(controller.finish(), std::string());
    EXPECT_EQ(controller.remaining(), std::size_t{0});
}

TEST(replacement_abandon_before_any_key_is_immediate)
{
    ReplacementController controller;
    controller.start(0, "abc", 1000);
    controller.abandon();
    EXPECT_TRUE(controller.state() == Burst::Idle);
    EXPECT_TRUE(controller.buffered().empty());
    EXPECT_EQ(controller.finish(), std::string());
}

TEST(replacement_keys_outside_a_burst_are_passed_through)
{
    ReplacementController controller;
    EXPECT_TRUE(backspace(controller) == KeyAction::Pass);
    EXPECT_TRUE(barrier(controller) == KeyAction::Pass);
    EXPECT_TRUE(text(controller) == KeyAction::Pass);
    EXPECT_TRUE(shortcut(controller) == KeyAction::Pass);
    EXPECT_EQ(controller.poll(10000), Burst::Idle);
}

TEST(replacement_starting_again_drops_the_older_text)
{
    // The caller is expected to finish one transaction before starting the
    // next, but a wrong caller must not leave text that nothing will type.
    ReplacementController controller;
    controller.start(2, "first", 1000);
    controller.start(1, "second", 1100);
    EXPECT_TRUE(controller.pending());
    EXPECT_EQ(controller.remaining(), std::size_t{1});
    static_cast<void>(backspace(controller));
    EXPECT_EQ(controller.finish(), std::string("second"));
}

// ---------------------------------------------------------------------------
// Batching
// ---------------------------------------------------------------------------

TEST(commit_batcher_merges_a_burst_of_commits)
{
    CommitBatcher batcher;
    EXPECT_TRUE(batcher.empty());
    EXPECT_FALSE(batcher.due(1000));

    batcher.append("t", 1000);
    EXPECT_EQ(batcher.size(), std::size_t{1});
    // The window starts with the first commit, so nothing is held back longer
    // than that even if more keep coming.
    EXPECT_EQ(batcher.due_at(), 1000u + CommitBatcher::kWindowMs);
    EXPECT_FALSE(batcher.due(1005));
    EXPECT_TRUE(batcher.due(1000 + CommitBatcher::kWindowMs));

    batcher.append("hu", 1002);
    EXPECT_EQ(batcher.take(), std::string("thu"));
    EXPECT_TRUE(batcher.empty());
    EXPECT_FALSE(batcher.due(2000));
}

TEST(commit_batcher_flushes_a_large_batch_early)
{
    CommitBatcher batcher;
    std::string lots(CommitBatcher::kMaxPendingBytes, 'a');
    batcher.append(lots, 1000);
    EXPECT_TRUE(batcher.due(1000));
}

TEST(commit_batcher_ignores_empty_text)
{
    CommitBatcher batcher;
    batcher.append("", 1000);
    EXPECT_TRUE(batcher.empty());
    EXPECT_EQ(batcher.due_at(), 0u);
}
