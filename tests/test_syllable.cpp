// SPDX-License-Identifier: MIT
// Syllable analysis, tone placement and plausibility.

#include "Helpers.hpp"
#include "Syllable.hpp"
#include "TestHarness.hpp"

namespace {

std::optional<std::size_t> tone_of(const std::string& word, nexkey::ToneStyle style)
{
    const auto letters = testing::letters_of(word);
    return nexkey::detail::tone_index(letters, style);
}

bool plausible(const std::string& word)
{
    const auto letters = testing::letters_of(word);
    return nexkey::detail::is_plausible(letters);
}

constexpr auto kModern = nexkey::ToneStyle::Modern;
constexpr auto kTraditional = nexkey::ToneStyle::Traditional;

} // namespace

TEST(syllable_structure)
{
    const auto letters = testing::letters_of("nghi\u00EAng");
    const auto structure = nexkey::detail::analyse(letters);
    EXPECT_EQ(structure.onset, std::size_t{3});
    EXPECT_EQ(structure.nucleus, std::size_t{2});
    EXPECT_EQ(structure.coda, std::size_t{2});
    EXPECT_TRUE(structure.complete);
}

TEST(syllable_structure_with_d_stroke)
{
    const auto letters = testing::letters_of("\u0111\u01B0\u1EDDng");
    const auto structure = nexkey::detail::analyse(letters);
    EXPECT_EQ(structure.onset, std::size_t{1});
    EXPECT_EQ(structure.nucleus, std::size_t{2});
    EXPECT_EQ(structure.coda, std::size_t{2});
}

TEST(tone_index_single_vowel)
{
    EXPECT_EQ(tone_of("a", kModern), std::optional<std::size_t>{0});
    EXPECT_EQ(tone_of("thu", kModern), std::optional<std::size_t>{2});
}

TEST(tone_index_open_diphthongs)
{
    EXPECT_EQ(tone_of("hoa", kModern), std::optional<std::size_t>{2});
    EXPECT_EQ(tone_of("hoa", kTraditional), std::optional<std::size_t>{1});
    EXPECT_EQ(tone_of("thuy", kModern), std::optional<std::size_t>{3});
    EXPECT_EQ(tone_of("thuy", kTraditional), std::optional<std::size_t>{2});
    EXPECT_EQ(tone_of("khoe", kModern), std::optional<std::size_t>{3});
    EXPECT_EQ(tone_of("khoe", kTraditional), std::optional<std::size_t>{2});
    EXPECT_EQ(tone_of("mai", kModern), std::optional<std::size_t>{1});
}

TEST(tone_index_closed_diphthongs)
{
    EXPECT_EQ(tone_of("hoan", kModern), std::optional<std::size_t>{2});
    EXPECT_EQ(tone_of("thuynh", kModern), std::optional<std::size_t>{3});
}

TEST(tone_index_marked_vowel)
{
    EXPECT_EQ(tone_of("ti\u00EAng", kModern), std::optional<std::size_t>{2});
    EXPECT_EQ(tone_of("thu\u01A1ng", kModern), std::optional<std::size_t>{3});
    EXPECT_EQ(tone_of("uy\u00EAn", kModern), std::optional<std::size_t>{2});
}

TEST(tone_index_triphthongs)
{
    EXPECT_EQ(tone_of("oai", kModern), std::optional<std::size_t>{1});
    EXPECT_EQ(tone_of("y\u00EAu", kModern), std::optional<std::size_t>{1});
    EXPECT_EQ(tone_of("ti\u00EAu", kModern), std::optional<std::size_t>{2});
}

TEST(tone_index_qu_and_gi_onsets)
{
    EXPECT_EQ(tone_of("qua", kModern), std::optional<std::size_t>{2});
    EXPECT_EQ(tone_of("gia", kModern), std::optional<std::size_t>{2});
    EXPECT_EQ(tone_of("gi", kModern), std::optional<std::size_t>{1});
}

TEST(plausibility)
{
    EXPECT_TRUE(plausible("a"));
    EXPECT_TRUE(plausible("ng"));
    EXPECT_TRUE(plausible("\u0111"));
    EXPECT_TRUE(plausible("ngh"));
    EXPECT_TRUE(plausible("ti\u1EBFng"));
    EXPECT_TRUE(plausible("\u0111\u01B0\u1EE3c"));
    EXPECT_TRUE(plausible("quy\u1EBFt"));
    EXPECT_FALSE(plausible("windows"));
    EXPECT_FALSE(plausible("as"));
    EXPECT_FALSE(plausible("jazz"));
    EXPECT_FALSE(plausible("ngf"));
}
