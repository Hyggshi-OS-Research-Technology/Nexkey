// SPDX-License-Identifier: MIT
// NexKey - how the text of a word reaches the application window.
//
// There are two ways to type a word into a document, and applications differ
// wildly in how well they support each one:
//
//   Direct   type the letters straight into the document and delete the ones
//            the word no longer needs. No underline, but every replacement
//            depends on the application really deleting what it was asked to.
//   Preedit  let the client (or the input method panel) show the word with an
//            underline and type it once, when the word is finished. Slower to
//            look at, but it only ever appends text and works everywhere.
//
// NexKey tries the direct path and steps down when the application proves that
// it drops the deletions, so the user does not have to know which program is
// which. This header holds the policy and the bookkeeping of one direct typing
// session; both are free of toolkit code, which makes them testable.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace nexkey {

/// How a frontend puts the word being typed into the document.
enum class DeliveryMode : std::uint8_t {
    Direct,  ///< Type into the document, no underline, replace while typing.
    Preedit, ///< Show the word in the preedit area, commit it once.
};

/// How a direct replacement removes the characters it does not need any more.
enum class DeleteStrategy : std::uint8_t {
    /// Press BackSpace on a virtual keyboard device, through the kernel. The
    /// application receives a genuine key press from a genuine device, so this
    /// works even where every other request is ignored. It needs the helper,
    /// and it is asynchronous: the frontend waits for a barrier key before it
    /// types the replacement.
    InjectedKey,
    /// Ask the client to delete a number of characters before the cursor.
    /// Exact and fast, but the request is silently dropped by some toolkits.
    SurroundingText,
    /// Forward one BackSpace key per character. The application handles the
    /// key itself, so it behaves like the user pressing it, which almost every
    /// toolkit does correctly.
    ForwardedKey,
};

/// Why a mode was chosen. The frontends turn this into a sentence for the
/// diagnostics page.
enum class DeliverySource : std::uint8_t {
    Default,          ///< Nothing special: direct typing where it can work.
    Setting,          ///< Direct typing is turned off in the settings.
    AppRule,          ///< The [app_rules] section names this application.
    NoSurroundingText, ///< The client does not report its text, see the comment.
    NoHelper          ///< The virtual keyboard helper is not running.
};

/// What a frontend should do with the text of one word.
struct Decision {
    DeliveryMode mode{DeliveryMode::Direct};
    /// Only meaningful for DeliveryMode::Direct.
    DeleteStrategy strategy{DeleteStrategy::SurroundingText};
    DeliverySource source{DeliverySource::Default};

    friend constexpr bool operator==(const Decision&, const Decision&) = default;
};

/// One value of the [app_rules] section.
///
/// A value can name an input method, a delivery mode, or both separated by '+'
/// or ',' ("code=telex+preedit"), and "off" bypasses the program. The two parts
/// are independent: a learned delivery rule must not throw away the input
/// method the user chose.
struct AppRule {
    std::string method;   ///< Input method name, empty when not given.
    std::string delivery; ///< Delivery mode, empty when not given.
    bool off{false};      ///< The program gets the keystrokes untouched.
    bool valid{false};    ///< Every part was understood and there was at least one.
};

/// Parse an [app_rules] value. Case, spaces and the order of the parts do not
/// matter.
[[nodiscard]] AppRule parse_app_rule(std::string_view value);

/// The value that keeps the method of `rule` and replaces its delivery mode.
[[nodiscard]] std::string with_delivery(const AppRule& rule, std::string_view delivery);

/// What one input context can do, as far as delivering text is concerned.
struct ClientInfo {
    /// Name of the program that owns the input context, as the desktop reports
    /// it. Empty when the frontend does not know.
    std::string_view program;
    /// The client declares that it can report the text around the cursor.
    bool surrounding_text{false};
    /// ...and it actually delivers it, with a cursor position.
    bool surrounding_valid{false};
    /// The virtual keyboard helper answered a probe recently.
    bool helper{false};
};

/// Decide between the direct and the preedit path.
///
/// The rule is deliberately simple: direct typing is the default, because it is
/// what makes NexKey feel like a keyboard rather than a popup. When the helper
/// is running, deletions go through the virtual keyboard and the client cannot
/// tell them from a real key press. Without it, a client that reports its text
/// is asked to delete, and one that does not gets forwarded BackSpace keys,
/// because deleting a number of characters requires knowing where the cursor
/// is. What the policy cannot know is whether the client honours the request,
/// so a frontend that sees a dropped replacement writes fallback_rule() into the
/// configuration and the next session starts one step further down.
class DeliveryPolicy {
public:
    /// Options::direct_typing, the global switch.
    void set_direct_typing(bool enabled) noexcept { m_direct_typing = enabled; }
    [[nodiscard]] bool direct_typing() const noexcept { return m_direct_typing; }

    /// The [app_rules] section: program name to mode. Values that name an input
    /// method ("telex", "vni", ...) are not about delivery and are ignored
    /// here; "off" is handled by the frontend before the policy is asked.
    void set_app_rules(std::unordered_map<std::string, std::string> rules);
    [[nodiscard]] const std::unordered_map<std::string, std::string>& app_rules() const noexcept
    {
        return m_rules;
    }

    [[nodiscard]] Decision decide(const ClientInfo& client) const;

    /// The rule that steps one rung down the ladder after `failed` did not
    /// work: surrounding text deletion falls back to BackSpace keys, and
    /// BackSpace keys fall back to preedit. Empty when there is nothing left to
    /// try (the application is already on the preedit path).
    [[nodiscard]] static std::string_view fallback_rule(const Decision& failed) noexcept;

private:
    bool m_direct_typing{true};
    std::unordered_map<std::string, std::string> m_rules;
};

/// One replacement of the end of the document.
struct Edit {
    /// Characters to remove before typing `commit`.
    std::size_t delete_code_points{0};
    /// Text to type at the cursor afterwards, always well formed UTF-8.
    std::string commit;

    [[nodiscard]] bool empty() const noexcept
    {
        return delete_code_points == 0 && commit.empty();
    }

    friend constexpr bool operator==(const Edit&, const Edit&) = default;
};

/// What to do to turn the end of the document from `before` into `after`.
///
/// The shared prefix is kept, so replacing one letter of a word deletes one
/// character and types one, and appending a letter deletes nothing. The cut
/// never lands inside a code point; the unit is the one the surrounding text
/// protocol counts, so a combining mark is a character of its own and deleting
/// a tone mark takes one character.
[[nodiscard]] Edit plan_edit(std::string_view before, std::string_view after) noexcept;

/// The result of comparing the reported document with what an edit asked for.
enum class EditOutcome : std::uint8_t {
    Landed,          ///< The end of the document is exactly what the edit wrote.
    DeletionIgnored, ///< The characters are still there and the new text follows.
    Unrelated,       ///< Neither: the cursor moved, or something else was typed.
    Unverifiable,    ///< The report cannot decide it, for example because the
                     ///< document has not caught up with the last edit yet.
};

/// The bookkeeping of direct typing in one input context.
///
/// The session mirrors the text it believes is at the cursor, plans the edits
/// that turn it into the next version of the word, and remembers the last edit
/// until the client confirms it. A client that never reports its text simply
/// never confirms anything, which costs nothing: the mirror is still what the
/// next edit is planned against.
class DirectSession {
public:
    /// The text the session believes is in the document before the cursor.
    [[nodiscard]] std::string_view text() const noexcept { return m_text; }

    /// True while the client has not confirmed the last edit.
    [[nodiscard]] bool has_pending() const noexcept { return m_pending.has_value(); }

    /// Plan the edit that turns the mirrored text into `next` and remember it
    /// as pending. The caller applies the edit to the client as returned.
    Edit replace(std::string_view next);

    /// Take note that the document now ends with `text` although the session
    /// did not ask for it. This is what a forwarded BackSpace does: the
    /// application deletes the character itself, so there is nothing to verify
    /// and the next edit has to be planned against the shorter word.
    void adopt(std::string_view text);

    struct Verification {
        EditOutcome outcome{EditOutcome::Landed};
        /// The edit that repairs the document after DeletionIgnored, already
        /// recorded as the new pending edit. Empty for the other outcomes.
        Edit repair;
    };

    /// Check the reported end of the document against the pending edit.
    /// Empty when there is nothing pending. On Unrelated the session forgets
    /// both the mirror and the pending edit, because the document is not the
    /// one it was writing into any more; on Unverifiable it keeps both.
    [[nodiscard]] std::optional<Verification> verify(std::string_view document_tail);

    /// Forget the mirrored text and the pending edit. A word is committed or
    /// dropped with this, and so is an input context whose cursor moved.
    void reset() noexcept;

private:
    struct Pending {
        std::string before; ///< Mirror before the edit.
        std::string after;  ///< Mirror after the edit.
        Edit edit;          ///< What the client was asked to do.
    };

    std::string m_text;
    std::optional<Pending> m_pending;
};

} // namespace nexkey
