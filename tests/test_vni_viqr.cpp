// SPDX-License-Identifier: MIT
// VNI and VIQR behaviour.

#include "Helpers.hpp"
#include "TestHarness.hpp"

using testing::method_options;
using testing::render_word;
using testing::type_text;

TEST(vni_tones)
{
    auto options = method_options(nexkey::Method::Vni);
    EXPECT_EQ(render_word("a1", options), std::string("\u00E1"));
    EXPECT_EQ(render_word("a2", options), std::string("\u00E0"));
    EXPECT_EQ(render_word("a3", options), std::string("\u1EA3"));
    EXPECT_EQ(render_word("a4", options), std::string("\u00E3"));
    EXPECT_EQ(render_word("a5", options), std::string("\u1EA1"));
    EXPECT_EQ(render_word("a10", options), std::string("a"));
}

TEST(vni_modifiers)
{
    auto options = method_options(nexkey::Method::Vni);
    EXPECT_EQ(render_word("a6", options), std::string("\u00E2"));
    EXPECT_EQ(render_word("a8", options), std::string("\u0103"));
    EXPECT_EQ(render_word("o6", options), std::string("\u00F4"));
    EXPECT_EQ(render_word("o7", options), std::string("\u01A1"));
    EXPECT_EQ(render_word("u7", options), std::string("\u01B0"));
    EXPECT_EQ(render_word("d9", options), std::string("\u0111"));
    EXPECT_EQ(render_word("e6", options), std::string("\u00EA"));
}

TEST(vni_words)
{
    auto options = method_options(nexkey::Method::Vni);
    EXPECT_EQ(render_word("tie6ng1", options), std::string("ti\u1EBFng"));
    EXPECT_EQ(render_word("Vie6t5", options), std::string("Vi\u1EC7t"));
    EXPECT_EQ(render_word("tru7o7ng2", options), std::string("tr\u01B0\u1EDDng"));
    EXPECT_EQ(render_word("d9i", options), std::string("\u0111i"));
    EXPECT_EQ(render_word("ngu7o7i2", options), std::string("ng\u01B0\u1EDDi"));
    EXPECT_EQ(type_text("tie6ng1 Vie6t5", options), std::string("ti\u1EBFng Vi\u1EC7t"));
}

TEST(vni_digits_are_literal_without_a_vowel)
{
    auto options = method_options(nexkey::Method::Vni);
    EXPECT_EQ(render_word("123", options), std::string("123"));
    EXPECT_EQ(type_text("abc 123", options), std::string("abc 123"));
}

TEST(viqr_tones)
{
    auto options = method_options(nexkey::Method::Viqr);
    EXPECT_EQ(render_word("a'", options), std::string("\u00E1"));
    EXPECT_EQ(render_word("a`", options), std::string("\u00E0"));
    EXPECT_EQ(render_word("a?", options), std::string("\u1EA3"));
    EXPECT_EQ(render_word("a~", options), std::string("\u00E3"));
    EXPECT_EQ(render_word("a.", options), std::string("\u1EA1"));
}

TEST(viqr_modifiers)
{
    auto options = method_options(nexkey::Method::Viqr);
    EXPECT_EQ(render_word("a^", options), std::string("\u00E2"));
    EXPECT_EQ(render_word("a(", options), std::string("\u0103"));
    EXPECT_EQ(render_word("o+", options), std::string("\u01A1"));
    EXPECT_EQ(render_word("u+", options), std::string("\u01B0"));
    EXPECT_EQ(render_word("d-", options), std::string("\u0111"));
    EXPECT_EQ(render_word("e^", options), std::string("\u00EA"));
}

TEST(viqr_words)
{
    auto options = method_options(nexkey::Method::Viqr);
    EXPECT_EQ(render_word("vie^.t", options), std::string("vi\u1EC7t"));
    EXPECT_EQ(render_word("tie^'ng", options), std::string("ti\u1EBFng"));
    EXPECT_EQ(render_word("d-ie^`u", options), std::string("\u0111i\u1EC1u"));
    EXPECT_EQ(render_word("tru+o+`ng", options), std::string("tr\u01B0\u1EDDng"));
}
