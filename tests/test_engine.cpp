// SPDX-License-Identifier: MIT
// Engine state handling.

#include "Helpers.hpp"
#include "TestHarness.hpp"

#include "nexkey/Engine.hpp"

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
