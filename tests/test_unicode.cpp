// SPDX-License-Identifier: MIT
// UTF-8 helpers.

#include "TestHarness.hpp"

#include "nexkey/Grapheme.hpp"
#include "nexkey/Unicode.hpp"

TEST(unicode_round_trip)
{
    const std::string text = "Ti\u1EBFng Vi\u1EC7t \u0111\u01B0\u1EE3c";
    const auto code_points = nexkey::unicode::decode(text);
    EXPECT_EQ(code_points.size(), std::size_t{15});
    EXPECT_EQ(nexkey::unicode::encode(code_points), text);
}

TEST(unicode_handles_four_byte_sequences)
{
    const std::string text{"\xF0\x9F\x98\x80"};
    const auto code_points = nexkey::unicode::decode(text);
    EXPECT_EQ(code_points.size(), std::size_t{1});
    EXPECT_EQ(code_points.front(), char32_t{0x1F600});
    EXPECT_EQ(nexkey::unicode::encode(code_points), text);
}

TEST(unicode_replaces_invalid_sequences)
{
    const std::string broken{"\xFF\x41"};
    const auto code_points = nexkey::unicode::decode(broken);
    EXPECT_EQ(code_points.size(), std::size_t{2});
    EXPECT_EQ(code_points.front(), nexkey::unicode::kReplacementCharacter);
    EXPECT_EQ(code_points.back(), char32_t{'A'});
}

TEST(unicode_encoded_length)
{
    EXPECT_EQ(nexkey::unicode::encoded_length(U'a'), std::size_t{1});
    EXPECT_EQ(nexkey::unicode::encoded_length(U'\u00E1'), std::size_t{2});
    EXPECT_EQ(nexkey::unicode::encoded_length(U'\u1EBF'), std::size_t{3});
    EXPECT_EQ(nexkey::unicode::encoded_length(char32_t{0x1F600}), std::size_t{4});
}

TEST(grapheme_compose_and_decompose)
{
    const nexkey::Letter letter{U'e', nexkey::Mark::Circumflex, false};
    EXPECT_EQ(nexkey::compose(letter, nexkey::Tone::Acute), char32_t{0x1EBF});
    EXPECT_EQ(nexkey::compose(letter, nexkey::Tone::None), char32_t{0x00EA});
    EXPECT_EQ(nexkey::compose({U'e', nexkey::Mark::Circumflex, true}, nexkey::Tone::Acute),
              char32_t{0x1EBE});

    const auto decomposed = nexkey::decompose(char32_t{0x1EBF});
    EXPECT_TRUE(decomposed.has_value());
    EXPECT_EQ(decomposed->letter, letter);
    EXPECT_EQ(decomposed->tone, nexkey::Tone::Acute);

    EXPECT_FALSE(nexkey::decompose(U'z').has_value());
}

TEST(grapheme_d_stroke)
{
    EXPECT_EQ(nexkey::compose({U'd', nexkey::Mark::Stroke, false}, nexkey::Tone::None),
              char32_t{0x0111});
    EXPECT_EQ(nexkey::compose({U'd', nexkey::Mark::Stroke, true}, nexkey::Tone::None),
              char32_t{0x0110});
    const auto decomposed = nexkey::decompose(char32_t{0x0110});
    EXPECT_TRUE(decomposed.has_value());
    EXPECT_EQ(decomposed->letter.base, char32_t{'d'});
    EXPECT_TRUE(decomposed->letter.upper);
    EXPECT_EQ(decomposed->letter.mark, nexkey::Mark::Stroke);
}
