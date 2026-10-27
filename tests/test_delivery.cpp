// SPDX-License-Identifier: MIT
// How the text of a word reaches the document: policy and direct typing edits.

#include "TestHarness.hpp"

#include "nexkey/Delivery.hpp"
#include "nexkey/Settings.hpp"
#include "nexkey/Unicode.hpp"

#include <filesystem>
#include <string>

using nexkey::ClientInfo;
using nexkey::Decision;
using nexkey::DeleteStrategy;
using nexkey::DeliveryMode;
using nexkey::DeliveryPolicy;
using nexkey::DeliverySource;
using nexkey::DirectSession;
using nexkey::Edit;
using nexkey::EditOutcome;

namespace {

Decision decide(const DeliveryPolicy& policy, std::string_view program, bool surrounding_valid)
{
    return policy.decide(ClientInfo{program, surrounding_valid, surrounding_valid});
}

} // namespace

// ---------------------------------------------------------------------------
// Policy: which applications get which path
// ---------------------------------------------------------------------------

TEST(delivery_default_is_direct_when_the_client_reports_its_text)
{
    const DeliveryPolicy policy;
    const auto decision = decide(policy, "gedit", true);
    EXPECT_TRUE(decision.mode == DeliveryMode::Direct);
    EXPECT_TRUE(decision.strategy == DeleteStrategy::SurroundingText);
    EXPECT_TRUE(decision.source == DeliverySource::Default);
}

TEST(delivery_without_surrounding_text_deletes_with_keys)
{
    // Without the document the frontend cannot count characters before the
    // cursor, so the only thing left is forwarding BackSpace.
    const DeliveryPolicy policy;
    const auto decision = decide(policy, "xterm", false);
    EXPECT_TRUE(decision.mode == DeliveryMode::Direct);
    EXPECT_TRUE(decision.strategy == DeleteStrategy::ForwardedKey);
    EXPECT_TRUE(decision.source == DeliverySource::NoSurroundingText);
}

TEST(delivery_setting_turns_direct_typing_off)
{
    DeliveryPolicy policy;
    policy.set_direct_typing(false);
    const auto decision = decide(policy, "gedit", true);
    EXPECT_TRUE(decision.mode == DeliveryMode::Preedit);
    EXPECT_TRUE(decision.source == DeliverySource::Setting);

    // The application rule cannot re-enable it: the switch is the user saying
    // "no direct typing anywhere".
    policy.set_app_rules({{"gedit", "direct"}});
    EXPECT_TRUE(decide(policy, "gedit", true).mode == DeliveryMode::Preedit);
}

TEST(delivery_app_rules_override_the_default)
{
    DeliveryPolicy policy;
    policy.set_app_rules({{"chrome", "preedit"}, {"code", "direct"}, {"term", "direct-keys"}});

    const auto chrome = decide(policy, "chrome", true);
    EXPECT_TRUE(chrome.mode == DeliveryMode::Preedit);
    EXPECT_TRUE(chrome.source == DeliverySource::AppRule);

    const auto code = decide(policy, "code", true);
    EXPECT_TRUE(code.mode == DeliveryMode::Direct);
    EXPECT_TRUE(code.strategy == DeleteStrategy::SurroundingText);
    EXPECT_TRUE(code.source == DeliverySource::AppRule);

    // "direct" still adapts to what the client can do.
    const auto code_no_text = decide(policy, "code", false);
    EXPECT_TRUE(code_no_text.strategy == DeleteStrategy::ForwardedKey);

    const auto terminal = decide(policy, "term", true);
    EXPECT_TRUE(terminal.mode == DeliveryMode::Direct);
    EXPECT_TRUE(terminal.strategy == DeleteStrategy::ForwardedKey);
}

TEST(delivery_app_rules_are_trimmed_and_case_insensitive)
{
    DeliveryPolicy policy;
    policy.set_app_rules({{"chrome", "  PREEDIT "}, {"term", "Direct-Keys"}});
    EXPECT_TRUE(decide(policy, "chrome", true).mode == DeliveryMode::Preedit);
    EXPECT_TRUE(decide(policy, "term", true).strategy == DeleteStrategy::ForwardedKey);
}

TEST(delivery_app_rules_that_name_an_input_method_are_ignored)
{
    // [app_rules] also carries the input method ("code=telex"), which says
    // nothing about how text is delivered.
    DeliveryPolicy policy;
    policy.set_app_rules({{"code", "telex"}, {"term", "off"}, {"other", "vni"}});
    const auto decision = decide(policy, "code", true);
    EXPECT_TRUE(decision.mode == DeliveryMode::Direct);
    EXPECT_TRUE(decision.source == DeliverySource::Default);
}

TEST(delivery_fallback_ladder_steps_down_once_per_failure)
{
    const Decision surrounding{DeliveryMode::Direct, DeleteStrategy::SurroundingText,
                               DeliverySource::Default};
    const Decision keys{DeliveryMode::Direct, DeleteStrategy::ForwardedKey,
                        DeliverySource::NoSurroundingText};
    const Decision preedit{DeliveryMode::Preedit, DeleteStrategy::ForwardedKey,
                           DeliverySource::AppRule};

    // The client ignored a deletion: try its own BackSpace handling.
    EXPECT_EQ(DeliveryPolicy::fallback_rule(surrounding), std::string_view("direct-keys"));
    // It ignored that too: only the underlined path is left.
    EXPECT_EQ(DeliveryPolicy::fallback_rule(keys), std::string_view("preedit"));
    // Nothing below preedit.
    EXPECT_EQ(DeliveryPolicy::fallback_rule(preedit), std::string_view());
}

TEST(delivery_fallback_rule_is_read_back_by_the_policy)
{
    DeliveryPolicy policy;
    const auto first = decide(policy, "chrome", true);
    EXPECT_TRUE(first.strategy == DeleteStrategy::SurroundingText);

    policy.set_app_rules({{"chrome", std::string(DeliveryPolicy::fallback_rule(first))}});
    const auto second = decide(policy, "chrome", true);
    EXPECT_TRUE(second.mode == DeliveryMode::Direct);
    EXPECT_TRUE(second.strategy == DeleteStrategy::ForwardedKey);

    policy.set_app_rules({{"chrome", std::string(DeliveryPolicy::fallback_rule(second))}});
    EXPECT_TRUE(decide(policy, "chrome", true).mode == DeliveryMode::Preedit);
}

// ---------------------------------------------------------------------------
// Edits: what has to be deleted and typed
// ---------------------------------------------------------------------------

TEST(delivery_plan_appending_types_without_deleting)
{
    EXPECT_EQ(nexkey::plan_edit("", "k"), (Edit{0, "k"}));
    EXPECT_EQ(nexkey::plan_edit("k", "kh"), (Edit{0, "h"}));
    EXPECT_EQ(nexkey::plan_edit("kh", "kha"), (Edit{0, "a"}));
    EXPECT_EQ(nexkey::plan_edit("", ""), Edit{});
}

TEST(delivery_plan_replacing_one_letter)
{
    // "kha" -> "kha" with a hook above the a: one character goes, one comes.
    const auto edit = nexkey::plan_edit("kha", "kh\u1EA3");
    EXPECT_EQ(edit.delete_code_points, std::size_t{1});
    EXPECT_EQ(edit.commit, std::string("\u1EA3"));
}

TEST(delivery_plan_counts_characters_not_bytes)
{
    // The Vietnamese letters are three bytes each, but the surrounding text
    // protocol counts characters.
    const std::string before = "vi\u1EC7t";
    const std::string after = "vi\u1EC7t nam";
    const auto edit = nexkey::plan_edit(before, after);
    EXPECT_EQ(edit.delete_code_points, std::size_t{0});
    EXPECT_EQ(edit.commit, std::string(" nam"));

    // A word that is rewritten completely deletes what it had.
    const auto rewritten = nexkey::plan_edit("\u0111\u01B0\u1EDDng", "duong");
    EXPECT_EQ(rewritten.delete_code_points, nexkey::unicode::count_code_points("\u0111\u01B0\u1EDDng"));
    EXPECT_EQ(rewritten.commit, std::string("duong"));
}

TEST(delivery_plan_handles_decomposed_text)
{
    // The same letter in NFD: the cut may fall between the base letter and its
    // marks, which is still a character boundary.
    const std::string nfd = "kha\u0309";
    const auto edit = nexkey::plan_edit("kha", nfd);
    EXPECT_EQ(edit.delete_code_points, std::size_t{0});
    EXPECT_EQ(edit.commit, std::string("\u0309"));
    EXPECT_TRUE(nexkey::unicode::is_valid(edit.commit));
}

TEST(delivery_plan_never_cuts_an_emoji)
{
    const std::string family =
        "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
    const auto edit = nexkey::plan_edit("ok", "ok " + family);
    EXPECT_EQ(edit.delete_code_points, std::size_t{0});
    EXPECT_EQ(edit.commit, " " + family);
    EXPECT_TRUE(nexkey::unicode::is_valid(edit.commit));
}

TEST(delivery_plan_of_the_same_text_is_empty)
{
    EXPECT_TRUE(nexkey::plan_edit("vi\u1EC7t", "vi\u1EC7t").empty());
    EXPECT_TRUE(nexkey::plan_edit("", "").empty());
}

// ---------------------------------------------------------------------------
// Session: tracking what the client really did
// ---------------------------------------------------------------------------

TEST(delivery_session_mirrors_the_word)
{
    DirectSession session;
    EXPECT_EQ(session.text(), std::string_view());
    EXPECT_FALSE(session.has_pending());

    const auto first = session.replace("k");
    EXPECT_EQ(first, (Edit{0, "k"}));
    EXPECT_EQ(session.text(), std::string_view("k"));
    EXPECT_TRUE(session.has_pending());

    const auto second = session.replace("kh");
    EXPECT_EQ(second, (Edit{0, "h"}));
    EXPECT_EQ(session.text(), std::string_view("kh"));
}

TEST(delivery_session_confirms_an_edit_that_landed)
{
    DirectSession session;
    static_cast<void>(session.replace("kha"));
    const auto verification = session.verify("kha");
    EXPECT_TRUE(verification.has_value());
    EXPECT_TRUE(verification->outcome == EditOutcome::Landed);
    EXPECT_TRUE(verification->repair.empty());
    EXPECT_FALSE(session.has_pending());
    EXPECT_EQ(session.text(), std::string_view("kha"));
}

TEST(delivery_session_repairs_a_ignored_deletion)
{
    // "kha" -> "kh\u1EA3" asks for one deletion and one character. A client
    // that ignores the deletion leaves "kha\u1EA3".
    DirectSession session;
    static_cast<void>(session.replace("kha"));
    static_cast<void>(session.replace("kh\u1EA3"));
    const auto verification = session.verify("kha\u1EA3");
    EXPECT_TRUE(verification.has_value());
    EXPECT_TRUE(verification->outcome == EditOutcome::DeletionIgnored);

    // The repair deletes everything from the shared prefix onwards, so the
    // duplicated letter goes as well, and types the wanted tail again.
    EXPECT_EQ(verification->repair, (Edit{2, "\u1EA3"}));
    // ... and the document is now what the repair asks for.
    const auto repaired = session.verify("kh\u1EA3");
    EXPECT_TRUE(repaired.has_value());
    EXPECT_TRUE(repaired->outcome == EditOutcome::Landed);
    EXPECT_FALSE(session.has_pending());
}

TEST(delivery_session_ignores_a_report_from_before_the_edit)
{
    // The client has not processed the edit yet, so the report still shows the
    // old text. Deleting again here would remove a second character.
    DirectSession session;
    static_cast<void>(session.replace("kha"));
    static_cast<void>(session.replace("kh\u1EA3"));
    const auto verification = session.verify("kha");
    EXPECT_TRUE(verification.has_value());
    EXPECT_TRUE(verification->outcome == EditOutcome::Unverifiable);
    EXPECT_TRUE(verification->repair.empty());
    EXPECT_TRUE(session.has_pending());
    EXPECT_EQ(session.text(), std::string_view("kh\u1EA3"));
}

TEST(delivery_session_forgets_a_document_it_does_not_own)
{
    // The cursor moved to another word, so nothing in the report matches.
    DirectSession session;
    static_cast<void>(session.replace("kha"));
    static_cast<void>(session.replace("kh\u1EA3"));
    const auto verification = session.verify("something else");
    EXPECT_TRUE(verification.has_value());
    EXPECT_TRUE(verification->outcome == EditOutcome::Unrelated);
    EXPECT_FALSE(session.has_pending());
    EXPECT_EQ(session.text(), std::string_view());
}

TEST(delivery_session_cannot_check_a_pure_deletion)
{
    // Deleting the last letter leaves nothing to look for: the report may
    // simply be the one from before the deletion.
    DirectSession session;
    static_cast<void>(session.replace("ng"));
    const auto edit = session.replace("n");
    EXPECT_EQ(edit, (Edit{1, ""}));
    const auto verification = session.verify("ng");
    EXPECT_TRUE(verification.has_value());
    EXPECT_TRUE(verification->outcome == EditOutcome::Unverifiable);
}

TEST(delivery_session_adopts_a_change_it_did_not_ask_for)
{
    // A forwarded BackSpace lets the application delete the character, so the
    // session only learns where the word now ends.
    DirectSession session;
    static_cast<void>(session.replace("kha"));
    session.adopt("kh");
    EXPECT_EQ(session.text(), std::string_view("kh"));
    EXPECT_FALSE(session.has_pending());

    // The next edit is planned against the shorter word, so it only appends.
    EXPECT_EQ(session.replace("kh\u1EA3"), (Edit{0, "\u1EA3"}));
    const auto verification = session.verify("kh\u1EA3");
    EXPECT_TRUE(verification.has_value());
    EXPECT_TRUE(verification->outcome == EditOutcome::Landed);
}

TEST(delivery_session_reset_forgets_everything)
{
    DirectSession session;
    static_cast<void>(session.replace("kha"));
    session.reset();
    EXPECT_EQ(session.text(), std::string_view());
    EXPECT_FALSE(session.has_pending());
    // A new word starts from an empty document, so it types all of it.
    EXPECT_EQ(session.replace("em"), (Edit{0, "em"}));
}

TEST(delivery_session_verify_without_a_pending_edit_does_nothing)
{
    DirectSession session;
    EXPECT_FALSE(session.verify("anything").has_value());
}

TEST(delivery_session_survives_text_outside_the_alphabet)
{
    // A word of emoji and marks: the mirror is still exact, because the edit
    // planner works on code points.
    const std::string family =
        "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
    DirectSession session;
    static_cast<void>(session.replace(family));
    static_cast<void>(session.replace(family + "s"));
    const auto verification = session.verify(family + "s");
    EXPECT_TRUE(verification.has_value());
    EXPECT_TRUE(verification->outcome == EditOutcome::Landed);
}

// ---------------------------------------------------------------------------
// The rule NexKey learns is one the configuration file can hold
// ---------------------------------------------------------------------------

TEST(delivery_learned_rule_survives_the_configuration_file)
{
    const auto path = std::filesystem::temp_directory_path() / "nexkey-tests-delivery" /
                      "nexkey.conf";
    std::error_code error;
    std::filesystem::remove_all(path.parent_path(), error);

    nexkey::Settings settings;
    settings.app_rules["chrome"] = "direct-keys";
    settings.app_rules["code"] = "preedit";
    EXPECT_TRUE(nexkey::save_settings(settings, path));

    const auto loaded = nexkey::load_settings(path);
    EXPECT_EQ(loaded.app_rules.at("chrome"), std::string("direct-keys"));
    EXPECT_EQ(loaded.app_rules.at("code"), std::string("preedit"));

    // The shell of the addon: rules in, decision out.
    DeliveryPolicy policy;
    policy.set_direct_typing(loaded.options.direct_typing);
    policy.set_app_rules(loaded.app_rules);
    const auto chrome = decide(policy, "chrome", true);
    EXPECT_TRUE(chrome.mode == DeliveryMode::Direct);
    EXPECT_TRUE(chrome.strategy == DeleteStrategy::ForwardedKey);
    EXPECT_TRUE(decide(policy, "code", true).mode == DeliveryMode::Preedit);
    // A program without a rule is unaffected.
    EXPECT_TRUE(decide(policy, "gedit", true).source == DeliverySource::Default);

    std::filesystem::remove_all(path.parent_path(), error);
}

// ---------------------------------------------------------------------------
// One [app_rules] value, two independent halves
// ---------------------------------------------------------------------------

TEST(delivery_app_rule_parses_a_method_a_mode_or_both)
{
    const auto method = nexkey::parse_app_rule("telex");
    EXPECT_TRUE(method.valid);
    EXPECT_EQ(method.method, std::string("telex"));
    EXPECT_EQ(method.delivery, std::string());

    const auto mode = nexkey::parse_app_rule("preedit");
    EXPECT_TRUE(mode.valid);
    EXPECT_EQ(mode.method, std::string());
    EXPECT_EQ(mode.delivery, std::string("preedit"));

    const auto both = nexkey::parse_app_rule("Telex + Direct-Keys");
    EXPECT_TRUE(both.valid);
    EXPECT_EQ(both.method, std::string("telex"));
    EXPECT_EQ(both.delivery, std::string("direct-keys"));

    // A comma works as well, and the order does not matter.
    const auto reversed = nexkey::parse_app_rule("direct,vni");
    EXPECT_TRUE(reversed.valid);
    EXPECT_EQ(reversed.method, std::string("vni"));
    EXPECT_EQ(reversed.delivery, std::string("direct"));

    const auto off = nexkey::parse_app_rule("off");
    EXPECT_TRUE(off.valid);
    EXPECT_TRUE(off.off);

    EXPECT_FALSE(nexkey::parse_app_rule("texlex").valid);
    EXPECT_FALSE(nexkey::parse_app_rule("").valid);
    EXPECT_FALSE(nexkey::parse_app_rule("telex+hurry").valid);
}

TEST(delivery_learned_rule_keeps_the_input_method)
{
    // The user typed "code=telex"; the application drops a replacement, and
    // learning must not take Telex away from it.
    const auto rule = nexkey::parse_app_rule("telex");
    EXPECT_EQ(nexkey::with_delivery(rule, "direct-keys"), std::string("telex+direct-keys"));

    DeliveryPolicy policy;
    policy.set_app_rules({{"code", "telex+direct-keys"}});
    const auto decision = decide(policy, "code", true);
    EXPECT_TRUE(decision.mode == DeliveryMode::Direct);
    EXPECT_TRUE(decision.strategy == DeleteStrategy::ForwardedKey);

    // A rule that only names a mode keeps working on its own.
    policy.set_app_rules({{"code", "preedit"}});
    EXPECT_TRUE(decide(policy, "code", true).mode == DeliveryMode::Preedit);
}
