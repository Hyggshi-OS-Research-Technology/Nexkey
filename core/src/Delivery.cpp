// SPDX-License-Identifier: MIT
// NexKey - delivery policy and the bookkeeping of direct typing.

#include "nexkey/Delivery.hpp"

#include "nexkey/Types.hpp"
#include "nexkey/Unicode.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace nexkey {

namespace {

std::string_view trimmed(std::string_view text) noexcept
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

/// The delivery part of the rule of one program, or an empty string.
std::string delivery_of(const std::unordered_map<std::string, std::string>& rules,
                        std::string_view program)
{
    if (program.empty()) {
        return {};
    }
    const auto found = rules.find(std::string(program));
    if (found == rules.end()) {
        return {};
    }
    return parse_app_rule(found->second).delivery;
}

} // namespace

AppRule parse_app_rule(std::string_view value)
{
    AppRule rule;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find_first_of("+,", start);
        const auto part = unicode::lower_ascii(
            trimmed(value.substr(start, end == std::string_view::npos ? std::string_view::npos
                                                                     : end - start)));
        start = end == std::string_view::npos ? value.size() + 1 : end + 1;
        if (part.empty()) {
            continue;
        }

        if (part == "off" || part == "en" || part == "english") {
            rule.off = true;
            rule.valid = true;
            continue;
        }
        if (method_from_string(part)) {
            rule.method = part;
            rule.valid = true;
            continue;
        }
        if (part == "preedit" || part == "compose" || part == "direct" ||
            part == "direct-keys" || part == "direct-backspace" || part == "direct-uinput" ||
            part == "direct-inject") {
            rule.delivery = part;
            rule.valid = true;
            continue;
        }
        // A part nobody understands makes the whole value invalid, so the user
        // sees the typo instead of a rule that silently does nothing.
        return AppRule{};
    }
    return rule;
}

std::string with_delivery(const AppRule& rule, std::string_view delivery)
{
    std::string value;
    if (!rule.method.empty()) {
        value += rule.method;
    }
    if (!delivery.empty()) {
        if (!value.empty()) {
            value += '+';
        }
        value += delivery;
    }
    return value;
}

void DeliveryPolicy::set_app_rules(std::unordered_map<std::string, std::string> rules)
{
    m_rules.clear();
    m_rules.reserve(rules.size());
    for (auto& [program, value] : rules) {
        if (program.empty()) {
            continue;
        }
        m_rules.emplace(std::move(program), parse_app_rule(value).delivery);
    }
}

Decision DeliveryPolicy::decide(const ClientInfo& client) const
{
    if (!m_direct_typing) {
        return Decision{DeliveryMode::Preedit, DeleteStrategy::ForwardedKey,
                        DeliverySource::Setting};
    }

    // The best available deletion, in order: the virtual keyboard (a real key
    // press, impossible for a client to tell apart), the surrounding text (a
    // request some clients ignore) and finally a forwarded key.
    const DeleteStrategy best = client.helper  ? DeleteStrategy::InjectedKey
                                : client.surrounding_valid ? DeleteStrategy::SurroundingText
                                                           : DeleteStrategy::ForwardedKey;

    const std::string rule = delivery_of(m_rules, client.program);
    if (rule == "preedit" || rule == "compose") {
        return Decision{DeliveryMode::Preedit, DeleteStrategy::ForwardedKey,
                        DeliverySource::AppRule};
    }
    if (rule == "direct") {
        return Decision{DeliveryMode::Direct, best, DeliverySource::AppRule};
    }
    if (rule == "direct-keys" || rule == "direct-backspace") {
        return Decision{DeliveryMode::Direct, DeleteStrategy::ForwardedKey,
                        DeliverySource::AppRule};
    }
    if (rule == "direct-uinput" || rule == "direct-inject") {
        // Forced: used by the learned fallback that walks back up the ladder
        // once a client has proven it accepts real key presses.
        return Decision{DeliveryMode::Direct,
                        client.helper ? DeleteStrategy::InjectedKey : DeleteStrategy::ForwardedKey,
                        DeliverySource::AppRule};
    }

    switch (best) {
    case DeleteStrategy::InjectedKey:
        return Decision{DeliveryMode::Direct, best, DeliverySource::Default};
    case DeleteStrategy::SurroundingText:
        return Decision{DeliveryMode::Direct, best, DeliverySource::Default};
    case DeleteStrategy::ForwardedKey:
        return Decision{DeliveryMode::Direct, best, DeliverySource::NoSurroundingText};
    }
    return Decision{DeliveryMode::Direct, DeleteStrategy::ForwardedKey,
                    DeliverySource::NoSurroundingText};
}

std::string_view DeliveryPolicy::fallback_rule(const Decision& failed) noexcept
{
    if (failed.mode != DeliveryMode::Direct) {
        return {};
    }
    switch (failed.strategy) {
    case DeleteStrategy::InjectedKey:
        // The helper did not deliver. The next rung asks the client instead.
        return "direct";
    case DeleteStrategy::SurroundingText:
        return "direct-keys";
    case DeleteStrategy::ForwardedKey:
        return "preedit";
    }
    return "preedit";
}

Edit plan_edit(std::string_view before, std::string_view after) noexcept
{
    const auto prefix = unicode::common_prefix(before, after);
    if (prefix.bytes >= before.size() && prefix.bytes >= after.size()) {
        return Edit{};
    }

    Edit edit;
    edit.delete_code_points = unicode::count_code_points(before.substr(prefix.bytes));
    edit.commit.assign(after.substr(prefix.bytes));
    return edit;
}

Edit DirectSession::replace(std::string_view next)
{
    Edit edit = plan_edit(m_text, next);
    if (!edit.empty()) {
        m_pending = Pending{m_text, std::string(next), edit};
    }
    m_text.assign(next);
    return edit;
}

void DirectSession::adopt(std::string_view text)
{
    m_text.assign(text);
    m_pending.reset();
}

namespace {

/// True when `text` ends with `tail`.
///
/// The document the frontend reports is the whole text before the cursor, while
/// the session mirrors only the word that is being typed. Every comparison in
/// verify() is therefore a comparison of the *end* of the report: comparing the
/// whole report with the word would find a difference in every document that
/// already contains something, and a session that concludes "this is not my
/// document" on every keystroke drops the word it is typing.
bool ends_with(std::string_view text, std::string_view tail) noexcept
{
    return text.size() >= tail.size() && text.substr(text.size() - tail.size()) == tail;
}

} // namespace

std::optional<DirectSession::Verification> DirectSession::verify(std::string_view document_tail)
{
    if (!m_pending.has_value()) {
        return std::nullopt;
    }

    const Pending pending = *m_pending;

    // Checked first: the new text is the strongest evidence that the edit
    // landed, and it is also what a session that only ever appends has to see.
    if (ends_with(document_tail, pending.after)) {
        m_pending.reset();
        return Verification{EditOutcome::Landed, Edit{}};
    }

    // A report that still shows the text from before the edit means the client
    // has not processed it yet; deciding now would delete a second character
    // when the first deletion simply had not arrived.
    if (ends_with(document_tail, pending.before)) {
        return Verification{EditOutcome::Unverifiable, Edit{}};
    }

    // A deletion on its own leaves nothing behind that could tell "ignored"
    // from "not reported yet", so there is nothing to decide either.
    if (pending.edit.commit.empty()) {
        return Verification{EditOutcome::Unverifiable, Edit{}};
    }

    std::string ignored = pending.before;
    ignored += pending.edit.commit;
    if (ends_with(document_tail, ignored)) {
        const Edit repair = plan_edit(ignored, pending.after);
        m_pending = Pending{std::move(ignored), pending.after, repair};
        return Verification{EditOutcome::DeletionIgnored, repair};
    }

    m_pending.reset();
    m_text.clear();
    return Verification{EditOutcome::Unrelated, Edit{}};
}

void DirectSession::reset() noexcept
{
    m_text.clear();
    m_pending.reset();
}

} // namespace nexkey
