// SPDX-License-Identifier: MIT
// Candidate window content.

#include "Helpers.hpp"
#include "TestHarness.hpp"

#include "nexkey/Engine.hpp"

namespace {

std::vector<nexkey::Candidate> candidates_for(std::string_view keys, nexkey::Options options = {})
{
    nexkey::Engine engine(options);
    for (const char key : keys) {
        static_cast<void>(engine.type(key));
    }
    return engine.candidates();
}

} // namespace

TEST(candidates_offer_the_other_tone_style)
{
    const auto candidates = candidates_for("hoaf");
    EXPECT_EQ(candidates.size(), std::size_t{3});
    if (candidates.size() == 3) {
        EXPECT_EQ(candidates[0].text, std::string("ho\u00E0"));
        EXPECT_EQ(candidates[0].comment, std::string());
        EXPECT_EQ(candidates[1].text, std::string("h\u00F2a"));
        EXPECT_EQ(candidates[1].comment, std::string("traditional"));
        EXPECT_EQ(candidates[2].text, std::string("hoaf"));
        EXPECT_EQ(candidates[2].comment, std::string("raw"));
    }
}

TEST(candidates_follow_the_active_style)
{
    auto options = testing::telex_options(nexkey::ToneStyle::Traditional);
    const auto candidates = candidates_for("hoaf", options);
    EXPECT_TRUE(candidates.size() >= 2);
    if (candidates.size() >= 2) {
        EXPECT_EQ(candidates[0].text, std::string("h\u00F2a"));
        EXPECT_EQ(candidates[1].text, std::string("ho\u00E0"));
        EXPECT_EQ(candidates[1].comment, std::string("modern"));
    }
}

TEST(candidates_keep_the_raw_keystrokes)
{
    const auto candidates = candidates_for("tieengs");
    EXPECT_EQ(candidates.size(), std::size_t{2});
    if (candidates.size() == 2) {
        EXPECT_EQ(candidates[0].text, std::string("ti\u1EBFng"));
        EXPECT_EQ(candidates[1].text, std::string("tieengs"));
    }
}

TEST(candidates_are_empty_without_alternatives)
{
    EXPECT_TRUE(candidates_for("ng").empty());
    EXPECT_TRUE(candidates_for("").empty());
    EXPECT_TRUE(candidates_for("windows").empty());
}

TEST(candidates_can_be_disabled)
{
    auto options = testing::telex_options();
    options.candidates = false;
    EXPECT_TRUE(candidates_for("hoaf", options).empty());
}

TEST(candidates_commit_the_selected_entry)
{
    nexkey::Engine engine;
    for (const char key : std::string_view("hoaf")) {
        static_cast<void>(engine.type(key));
    }
    const auto candidates = engine.candidates();
    EXPECT_TRUE(candidates.size() >= 3);
    if (candidates.size() >= 3) {
        const auto step = engine.commit_candidate(2);
        EXPECT_EQ(step.commit, std::string("hoaf"));
        EXPECT_TRUE(step.handled);
        EXPECT_FALSE(engine.composing());
        EXPECT_EQ(engine.flush().commit, std::string());
    }
}

TEST(candidates_ignore_out_of_range_selection)
{
    nexkey::Engine engine;
    for (const char key : std::string_view("hoaf")) {
        static_cast<void>(engine.type(key));
    }
    EXPECT_FALSE(engine.commit_candidate(42).handled);
    EXPECT_TRUE(engine.composing());
}

TEST(candidates_are_encoded_like_the_preedit)
{
    auto options = testing::telex_options();
    options.encoding = nexkey::Encoding::Viqr;
    const auto candidates = candidates_for("tieengs", options);
    EXPECT_TRUE(candidates.size() >= 2);
    if (candidates.size() >= 2) {
        EXPECT_EQ(candidates[0].text, std::string("tie^'ng"));
        EXPECT_EQ(candidates[1].text, std::string("tieengs"));
    }
}
