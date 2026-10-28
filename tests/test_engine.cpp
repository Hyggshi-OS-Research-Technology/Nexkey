// SPDX-License-Identifier: MIT
// Engine state handling.

#include "Helpers.hpp"
#include "TestHarness.hpp"

#include "nexkey/Engine.hpp"
#include "nexkey/Unicode.hpp"

using testing::telex_options;

namespace {

std::string feed(nexkey::Engine& engine, std::string_view keys)
{
    std::string out;
    for (const char key : keys) {
        out += engine.type(key).commit;
    }
    return out;
}

} // namespace

TEST(engine_commits_on_boundary)
{
    nexkey::Engine engine;
    EXPECT_EQ(feed(engine, "tieengs"), std::string());
    EXPECT_TRUE(engine.composing());
    EXPECT_EQ(engine.preedit(), std::string("ti\u1EBFng"));

    const auto step = engine.type(' ');
    EXPECT_EQ(step.commit, std::string("ti\u1EBFng "));
    EXPECT_TRUE(step.preedit.empty());
    EXPECT_FALSE(engine.composing());
}

TEST(engine_flush)
{
    nexkey::Engine engine;
    feed(engine, "vieetj");
    const auto step = engine.flush();
    EXPECT_EQ(step.commit, std::string("vi\u1EC7t"));
    EXPECT_TRUE(step.handled);
    EXPECT_FALSE(engine.composing());
    EXPECT_EQ(engine.flush().commit, std::string());
}

TEST(engine_discard)
{
    nexkey::Engine engine;
    feed(engine, "abc");
    engine.discard();
    EXPECT_FALSE(engine.composing());
    EXPECT_TRUE(engine.preedit().empty());
}

TEST(engine_backspace_restores)
{
    nexkey::Engine engine;
    feed(engine, "tieengs");
    EXPECT_EQ(engine.preedit(), std::string("ti\u1EBFng"));

    static_cast<void>(engine.backspace());
    EXPECT_EQ(engine.preedit(), std::string("ti\u00EAng"));

    static_cast<void>(engine.backspace());
    EXPECT_EQ(engine.preedit(), std::string("ti\u00EAn"));
}

TEST(engine_every_keystroke_of_a_word)
{
    // What the frontend sees key by key, not only at the end of the word: the
    // addon commits or replaces the document after every one of these steps, so
    // a preedit that is wrong for a moment is a character in the wrong place on
    // the screen.
    struct Step {
        char key;
        const char* preedit;
    };
    const Step diacritic_marks[] = {
        {'c', "c"}, {'a', "ca"}, {'a', "c\u00E2"}, {'s', "c\u1EA5"},
    };
    nexkey::Engine engine;
    for (const auto& step : diacritic_marks) {
        const auto result = engine.type(step.key);
        EXPECT_EQ(result.preedit, std::string(step.preedit));
        // Nothing is committed while the word is being typed: in direct typing
        // the document is edited in place.
        EXPECT_TRUE(result.commit.empty());
        EXPECT_TRUE(result.handled);
    }

    const Step long_word[] = {
        {'t', "t"},    {'i', "ti"},   {'e', "tie"},  {'e', "ti\u00EA"},
        {'n', "ti\u00EAn"}, {'g', "ti\u00EAng"}, {'s', "ti\u1EBFng"},
    };
    nexkey::Engine other;
    for (const auto& step : long_word) {
        const auto result = other.type(step.key);
        EXPECT_EQ(result.preedit, std::string(step.preedit));
        EXPECT_TRUE(result.commit.empty());
    }
}

TEST(engine_every_backspace_of_a_word)
{
    // Each BackSpace must change the visible word, and the change must be the
    // one the user expects: the tone goes first, then the letter it sits on.
    nexkey::Engine engine;
    feed(engine, "caas");
    EXPECT_EQ(engine.preedit(), std::string("c\u1EA5"));

    const char* expected[] = {"c\u00E2", "ca", "c"};
    for (const char* step : expected) {
        const auto result = engine.backspace();
        EXPECT_TRUE(result.handled);
        EXPECT_EQ(result.preedit, std::string(step));
    }

    // The last letter leaves the composition, which the engine still consumes:
    // a preedit frontend clears its preedit, and a direct typing frontend sees
    // an empty preedit and deletes the letter from the document itself.
    const auto emptied = engine.backspace();
    EXPECT_TRUE(emptied.handled);
    EXPECT_TRUE(emptied.preedit.empty());
    EXPECT_FALSE(engine.composing());

    // Only when there is nothing left to compose does the key belong to the
    // application again.
    const auto empty = engine.backspace();
    EXPECT_FALSE(empty.handled);
}

TEST(engine_a_tone_key_twice_removes_the_tone)
{
    nexkey::Engine engine;
    feed(engine, "cas");
    EXPECT_EQ(engine.preedit(), std::string("c\u00E1"));
    static_cast<void>(engine.type('s'));
    EXPECT_EQ(engine.preedit(), std::string("cas"));
}

TEST(engine_backspace_without_composition)
{
    nexkey::Engine engine;
    const auto step = engine.backspace();
    EXPECT_FALSE(step.handled);
}

TEST(engine_backspace_plain_mode)
{
    auto options = telex_options();
    options.smart_backspace = false;
    nexkey::Engine engine(options);
    feed(engine, "tieengs");
    static_cast<void>(engine.backspace());
    EXPECT_EQ(engine.preedit(), std::string("ti\u00EAng"));
}

TEST(engine_non_ascii_keystroke)
{
    nexkey::Engine engine;
    feed(engine, "tieengs");
    const auto step = engine.type(U'\u00E9');
    EXPECT_EQ(step.commit, std::string("ti\u1EBFng\u00E9"));
    EXPECT_FALSE(engine.composing());
}

TEST(engine_control_key_flushes)
{
    nexkey::Engine engine;
    feed(engine, "as");
    const auto step = engine.type(U'\u007F');
    EXPECT_EQ(step.commit, std::string("\u00E1"));
    EXPECT_FALSE(step.handled);
    EXPECT_FALSE(engine.composing());
}

TEST(engine_options_can_change_mid_word)
{
    nexkey::Engine engine;
    feed(engine, "hoaf");
    EXPECT_EQ(engine.preedit(), std::string("ho\u00E0"));

    auto options = telex_options(nexkey::ToneStyle::Traditional);
    engine.set_options(options);
    EXPECT_EQ(engine.preedit(), std::string("h\u00F2a"));
}

TEST(engine_plausibility)
{
    nexkey::Engine engine;
    EXPECT_TRUE(engine.plausible("tieengs"));
    EXPECT_TRUE(engine.plausible("ng"));
    EXPECT_FALSE(engine.plausible("windows"));
}

TEST(engine_macro_expansion)
{
    nexkey::Engine engine;
    engine.set_macros({{"vn", "Việt Nam"}, {"ko", "không"}});
    feed(engine, "vn");
    const auto step = engine.type(' ');
    EXPECT_EQ(step.commit, std::string("Việt Nam "));
    EXPECT_FALSE(engine.composing());
}

TEST(engine_emoji_shortcuts)
{
    nexkey::Engine engine;
    engine.set_macros({{":fire:", "🔥"}, {":)", "😊"}, {"<3", "❤️"}});

    // Instant colon emoji completion
    feed(engine, ":fire");
    const auto step_fire = engine.type(':');
    EXPECT_EQ(step_fire.commit, std::string("🔥"));
    EXPECT_FALSE(engine.composing());

    // Smiley on space
    feed(engine, ":)");
    const auto step_smile = engine.type(' ');
    EXPECT_EQ(step_smile.commit, std::string("😊 "));
    EXPECT_FALSE(engine.composing());

    // Heart on space
    feed(engine, "<3");
    const auto step_heart = engine.type(' ');
    EXPECT_EQ(step_heart.commit, std::string("❤️ "));
    EXPECT_FALSE(engine.composing());
}


TEST(engine_macro_capitalization)
{
    // The shorthand decides the case of the expansion: VN means all capitals,
    // Vn only capitalizes the first letter, vn keeps the text as it is.
    nexkey::Engine engine;
    engine.set_macros({{"vn", "Vi\u1EC7t Nam"}});

    feed(engine, "vn");
    EXPECT_EQ(engine.type(' ').commit, std::string("Vi\u1EC7t Nam "));

    feed(engine, "Vn");
    EXPECT_EQ(engine.type(' ').commit, std::string("Vi\u1EC7t Nam "));

    feed(engine, "VN");
    const auto all_caps = engine.type(' ').commit;
    EXPECT_TRUE(nexkey::unicode::is_valid(all_caps));
    if (nexkey::unicode::has_full_unicode()) {
        EXPECT_EQ(all_caps, std::string("VI\u1EC6T NAM "));
    } else {
        // Without the Unicode database only ASCII letters change case.
        EXPECT_EQ(all_caps, std::string("VI\u1EC7T NAM "));
    }
}

TEST(engine_macro_capitalization_of_a_vietnamese_letter)
{
    // A macro that starts with a Vietnamese letter is capitalized as a whole
    // letter, not by touching the first byte of its UTF-8 sequence.
    nexkey::Engine engine;
    engine.set_macros({{"ad", "\u0103n tr\u01B0a"}});
    feed(engine, "Ad");
    const auto step = engine.type(' ');
    EXPECT_TRUE(nexkey::unicode::is_valid(step.commit));
    if (nexkey::unicode::has_full_unicode()) {
        EXPECT_EQ(step.commit, std::string("\u0102n tr\u01B0a "));
    }
}

TEST(engine_custom_dictionary_keeps_words_as_typed)
{
    // "test" would be "tét" under the Telex rules. A user who types it every day
    // puts it in the personal dictionary and gets it back unchanged.
    nexkey::Options options;
    options.custom_dict = true;
    nexkey::Engine engine(options);
    engine.set_custom_words({"test", "docs", "build"});

    feed(engine, "test");
    EXPECT_EQ(engine.type(' ').commit, std::string("test "));

    feed(engine, "docs");
    EXPECT_EQ(engine.type(' ').commit, std::string("docs "));

    // The lookup ignores ASCII case, like the macro shorthand does.
    feed(engine, "Test");
    EXPECT_EQ(engine.type(' ').commit, std::string("Test "));

    // Words outside the list keep following the Vietnamese rules: "next" is not
    // in it, so the tone key still turns it into "nẽt".
    feed(engine, "next");
    EXPECT_EQ(engine.type(' ').commit, std::string("nẽt "));

    // And so do Vietnamese words: the dictionary is a list of exceptions.
    feed(engine, "vieejt");
    EXPECT_EQ(engine.type(' ').commit, std::string("vi\u1EC7t "));
}

TEST(engine_custom_dictionary_needs_the_switch)
{
    // The word list is stored either way, but Options::custom_dict decides
    // whether it is used, so a user can turn the feature off without losing the
    // entries.
    nexkey::Options options;
    options.custom_dict = false;
    nexkey::Engine engine(options);
    engine.set_custom_words({"test"});
    EXPECT_EQ(engine.custom_words().size(), std::size_t{1});

    feed(engine, "test");
    EXPECT_EQ(engine.type(' ').commit, std::string("tét "));

    nexkey::Options enabled = options;
    enabled.custom_dict = true;
    engine.set_options(enabled);
    feed(engine, "test");
    EXPECT_EQ(engine.type(' ').commit, std::string("test "));
}
