// SPDX-License-Identifier: MIT
// Telex behaviour.

#include "Helpers.hpp"
#include "TestHarness.hpp"

using testing::render_word;
using testing::telex_options;
using testing::type_text;

TEST(telex_tone_only)
{
    EXPECT_EQ(render_word("as"), std::string("\u00E1"));
    EXPECT_EQ(render_word("af"), std::string("\u00E0"));
    EXPECT_EQ(render_word("ar"), std::string("\u1EA3"));
    EXPECT_EQ(render_word("ax"), std::string("\u00E3"));
    EXPECT_EQ(render_word("aj"), std::string("\u1EA1"));
    EXPECT_EQ(render_word("asz"), std::string("a"));
}

TEST(telex_double_letters)
{
    EXPECT_EQ(render_word("aa"), std::string("\u00E2"));
    EXPECT_EQ(render_word("ee"), std::string("\u00EA"));
    EXPECT_EQ(render_word("oo"), std::string("\u00F4"));
    EXPECT_EQ(render_word("dd"), std::string("\u0111"));
    EXPECT_EQ(render_word("aaa"), std::string("aa"));
    EXPECT_EQ(render_word("ddd"), std::string("dd"));
    EXPECT_EQ(render_word("aw"), std::string("\u0103"));
    EXPECT_EQ(render_word("ow"), std::string("\u01A1"));
    EXPECT_EQ(render_word("uw"), std::string("\u01B0"));
    EXPECT_EQ(render_word("w"), std::string("\u01B0"));
    EXPECT_EQ(render_word("aww"), std::string("aw"));
}

TEST(telex_bracket_shortcuts)
{
    EXPECT_EQ(render_word("["), std::string("\u01A1"));
    EXPECT_EQ(render_word("]"), std::string("\u01B0"));
    EXPECT_EQ(render_word("{"), std::string("\u01A0"));
    EXPECT_EQ(render_word("}"), std::string("\u01AF"));
    EXPECT_EQ(render_word("u["), std::string("u\u01A1"));

    auto options = telex_options();
    options.bracket_shortcuts = false;
    EXPECT_EQ(render_word("[", options), std::string("["));
}

TEST(telex_toneless_words)
{
    EXPECT_EQ(render_word("tieengs"), std::string("ti\u1EBFng"));
    EXPECT_EQ(render_word("Vieetj"), std::string("Vi\u1EC7t"));
    EXPECT_EQ(render_word("truowngf"), std::string("tr\u01B0\u1EDDng"));
    EXPECT_EQ(render_word("dduwowngf"), std::string("\u0111\u01B0\u1EDDng"));
    EXPECT_EQ(render_word("nguowif"), std::string("ng\u01B0\u1EDDi"));
    EXPECT_EQ(render_word("nghieeng"), std::string("nghi\u00EAng"));
    EXPECT_EQ(render_word("dduwowcj"), std::string("\u0111\u01B0\u1EE3c"));
    EXPECT_EQ(render_word("thuyeefn"), std::string("thuy\u1EC1n"));
    EXPECT_EQ(render_word("ddi"), std::string("\u0111i"));
    EXPECT_EQ(render_word("quaf"), std::string("qu\u00E0"));
    EXPECT_EQ(render_word("giaf"), std::string("gi\u00E0"));
}

TEST(telex_tone_position)
{
    // Open syllables with o/u glides follow the configured style.
    EXPECT_EQ(render_word("hoaf"), std::string("ho\u00E0"));
    EXPECT_EQ(render_word("hoaf", telex_options(nexkey::ToneStyle::Traditional)),
              std::string("h\u00F2a"));
    EXPECT_EQ(render_word("thuyr"), std::string("thu\u1EF7"));
    EXPECT_EQ(render_word("thuyr", telex_options(nexkey::ToneStyle::Traditional)),
              std::string("th\u1EE7y"));
    EXPECT_EQ(render_word("khoer"), std::string("kho\u1EBB"));
    EXPECT_EQ(render_word("khoer", telex_options(nexkey::ToneStyle::Traditional)),
              std::string("kh\u1ECFe"));
    // Open syllables like ua, ui, oi always put tone on the main vowel (e.g. của, not cuả).
    EXPECT_EQ(render_word("cuar"), std::string("c\u1EE7a"));
    EXPECT_EQ(render_word("cuar", telex_options(nexkey::ToneStyle::Traditional)),
              std::string("c\u1EE7a"));
    EXPECT_EQ(render_word("muaf"), std::string("m\u00F9a"));
    EXPECT_EQ(render_word("tuis"), std::string("t\u00FAi"));
    EXPECT_EQ(render_word("thois"), std::string("th\u00F3i"));
    // Closed syllables put the tone on the second vowel of the nucleus.
    EXPECT_EQ(render_word("hoanf"), std::string("ho\u00E0n"));
    EXPECT_EQ(render_word("hoanf", telex_options(nexkey::ToneStyle::Traditional)),
              std::string("ho\u00E0n"));
    // Triphthongs take the tone on the middle vowel.
    EXPECT_EQ(render_word("oais"), std::string("o\u00E1i"));
    // Marked vowels win.
    EXPECT_EQ(render_word("vieejt"), std::string("vi\u1EC7t"));
    EXPECT_EQ(render_word("tiees"), std::string("ti\u1EBF"));
    EXPECT_EQ(render_word("thuowng"), std::string("th\u01B0\u01A1ng"));
}

TEST(telex_uppercase)
{
    EXPECT_EQ(render_word("DD"), std::string("\u0110"));
    EXPECT_EQ(render_word("VIEETJ"), std::string("VI\u1EC6T"));
    EXPECT_EQ(render_word("Tieengs"), std::string("Ti\u1EBFng"));
    EXPECT_EQ(render_word("HOAF"), std::string("HO\u00C0"));
}

TEST(telex_english_fallback)
{
    EXPECT_EQ(render_word("hello"), std::string("hello"));
    EXPECT_EQ(render_word("windows"), std::string("windows"));
    EXPECT_EQ(render_word("jazz"), std::string("jazz"));
    EXPECT_EQ(render_word("world"), std::string("world"));
    EXPECT_EQ(render_word("asdf"), std::string("asdf"));

    auto options = telex_options(nexkey::ToneStyle::Modern, false);
    EXPECT_EQ(render_word("asdf", options), std::string("\u00E0d"));
}

TEST(telex_toggle_escape)
{
    // Pressing a tone key twice types it literally.
    EXPECT_EQ(render_word("ass"), std::string("as"));
    EXPECT_EQ(render_word("miss"), std::string("mis"));
}

TEST(telex_simple_method)
{
    auto options = testing::method_options(nexkey::Method::SimpleTelex);
    EXPECT_EQ(render_word("aa", options), std::string("\u00E2"));
    EXPECT_EQ(render_word("w", options), std::string("w"));
    EXPECT_EQ(render_word("aw", options), std::string("aw"));
    EXPECT_EQ(render_word("tieengs", options), std::string("ti\u1EBFng"));
}

TEST(telex_sentences)
{
    EXPECT_EQ(type_text("tieengs Vieetj"), std::string("ti\u1EBFng Vi\u1EC7t"));
    EXPECT_EQ(type_text("tooi yeeu Vieetj Nam"), std::string("t\u00F4i y\u00EAu Vi\u1EC7t Nam"));
    EXPECT_EQ(type_text("tieengs "), std::string("ti\u1EBFng "));
    EXPECT_EQ(type_text("a.b"), std::string("a.b"));
}
