// SPDX-License-Identifier: MIT
// Character set conversion.

#include "Helpers.hpp"
#include "TestHarness.hpp"

#include "nexkey/Converter.hpp"
#include "nexkey/Unicode.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace {

std::string encode(nexkey::Encoding encoding, const std::string& utf8)
{
    return nexkey::Converter::encode(encoding, nexkey::unicode::decode(utf8));
}

} // namespace

TEST(tcvn3_encode)
{
    const std::string text = "Ti\u1EBFng";
    const std::string expected{"Ti\xD5ng"};
    EXPECT_EQ(encode(nexkey::Encoding::Tcvn3, text), expected);
    EXPECT_EQ(encode(nexkey::Encoding::Tcvn3, "\u0111"), std::string("\xAE"));
}

TEST(tcvn3_round_trip)
{
    const std::string text = "Ti\u1EBFng Vi\u1EC7t: \u0103 \u00E2 \u00EA \u00F4 \u01A1 \u01B0";
    const auto bytes = encode(nexkey::Encoding::Tcvn3, text);
    const auto back = nexkey::Converter::decode(nexkey::Encoding::Tcvn3, bytes);
    EXPECT_EQ(nexkey::unicode::encode(back), text);
}

TEST(tcvn3_keeps_ascii)
{
    EXPECT_EQ(encode(nexkey::Encoding::Tcvn3, "NexKey 2.0"), std::string("NexKey 2.0"));
}

TEST(viqr_encode)
{
    EXPECT_EQ(encode(nexkey::Encoding::Viqr, "\u1EC7"), std::string("e^."));
    EXPECT_EQ(encode(nexkey::Encoding::Viqr, "\u0111\u01B0\u1EE3c"), std::string("d-u+o+.c"));
}

TEST(viqr_round_trip)
{
    const std::string text = "Vi\u1EC7t Nam";
    const auto bytes = encode(nexkey::Encoding::Viqr, text);
    EXPECT_EQ(bytes, std::string("Vie^.t Nam"));
    const auto back = nexkey::Converter::decode(nexkey::Encoding::Viqr, bytes);
    EXPECT_EQ(nexkey::unicode::encode(back), text);
}

TEST(nfd_encode)
{
    const std::string expected{"e\u0302\u0301"};
    EXPECT_EQ(encode(nexkey::Encoding::UnicodeDecomposed, "\u1EBF"), expected);
    EXPECT_EQ(encode(nexkey::Encoding::UnicodeDecomposed, "\u01A1"), std::string("o\u031B"));
}

TEST(nfd_round_trip)
{
    const std::string text = "Ti\u1EBFng Vi\u1EC7t \u0111\u01B0\u1EE3c";
    const auto bytes = encode(nexkey::Encoding::UnicodeDecomposed, text);
    const auto back = nexkey::Converter::decode(nexkey::Encoding::UnicodeDecomposed, bytes);
    EXPECT_EQ(nexkey::unicode::encode(back), text);
}

TEST(convert_between_encodings)
{
    const std::string unicode = "Ti\u1EBFng Vi\u1EC7t";
    const auto tcvn3 = nexkey::Converter::convert(unicode, nexkey::Encoding::Unicode,
                                                 nexkey::Encoding::Tcvn3);
    const auto back = nexkey::Converter::convert(tcvn3, nexkey::Encoding::Tcvn3,
                                                nexkey::Encoding::Unicode);
    EXPECT_EQ(back, unicode);

    const auto nfd = nexkey::Converter::convert(unicode, nexkey::Encoding::Unicode,
                                                nexkey::Encoding::UnicodeDecomposed);
    EXPECT_EQ(nexkey::Converter::convert(nfd, nexkey::Encoding::UnicodeDecomposed,
                                         nexkey::Encoding::Unicode),
              unicode);
    EXPECT_EQ(nexkey::Converter::convert(unicode, nexkey::Encoding::Unicode,
                                         nexkey::Encoding::Unicode),
              unicode);
}

TEST(engine_outputs_other_encodings)
{
    auto options = testing::telex_options();
    options.encoding = nexkey::Encoding::Tcvn3;
    EXPECT_EQ(testing::render_word("tieengs", options), std::string("ti\xD5ng"));

    options.encoding = nexkey::Encoding::Viqr;
    EXPECT_EQ(testing::render_word("tieengs", options), std::string("tie^'ng"));

    options.encoding = nexkey::Encoding::UnicodeDecomposed;
    EXPECT_EQ(testing::render_word("tieengs", options), std::string("tie\u0302\u0301ng"));
}

// ---------------------------------------------------------------------------
// Unicode normalization
// ---------------------------------------------------------------------------

namespace {

/// Combining classes of the marks of the Vietnamese alphabet, copied from the
/// Unicode character database so that the test does not depend on the encoder
/// it is checking.
constexpr int combining_class(char32_t code_point)
{
    switch (code_point) {
    case 0x031B: // combining horn
        return 216;
    case 0x0323: // combining dot below
        return 220;
    default:
        return 230; // the accents that sit above the letter
    }
}

} // namespace

TEST(nfd_of_every_letter_is_canonical)
{
    // NFD is not "base letter, then the marks in typing order": UAX #15 sorts
    // the marks by combining class. Writing the dot of ộ after its circumflex
    // produces a string that looks right and is not NFD.
    for (const char32_t base : {U'a', U'e', U'i', U'o', U'u', U'y'}) {
        for (const nexkey::Mark mark : {nexkey::Mark::None, nexkey::Mark::Circumflex,
                                        nexkey::Mark::Breve, nexkey::Mark::Horn}) {
            for (const bool upper : {false, true}) {
                for (int tone = 0; tone < 6; ++tone) {
                    const auto letter = nexkey::compose(nexkey::Letter{base, mark, upper},
                                                        static_cast<nexkey::Tone>(tone));
                    const auto nfd = nexkey::unicode::decode(nexkey::Converter::encode(
                        nexkey::Encoding::UnicodeDecomposed, std::u32string{letter}));

                    EXPECT_FALSE(nfd.empty());
                    EXPECT_EQ(nfd.front(), upper ? base - 32 : base);
                    int previous = 0;
                    for (std::size_t index = 1; index < nfd.size(); ++index) {
                        EXPECT_TRUE(combining_class(nfd[index]) >= previous);
                        previous = combining_class(nfd[index]);
                    }
                    EXPECT_EQ(nexkey::Converter::decode(
                                  nexkey::Encoding::UnicodeDecomposed,
                                  nexkey::unicode::encode(nfd)),
                              std::u32string{letter});
                }
            }
        }
    }

    // The stroked d has no decomposition at all.
    EXPECT_EQ(encode(nexkey::Encoding::UnicodeDecomposed, "\u0111"), std::string("\u0111"));
}

TEST(nfd_puts_the_dot_below_in_front)
{
    // The four letters whose tone mark has a lower combining class than their
    // vowel mark used to be written in the wrong order.
    EXPECT_EQ(encode(nexkey::Encoding::UnicodeDecomposed, "\u1ED9"), std::string("o\u0323\u0302"));
    EXPECT_EQ(encode(nexkey::Encoding::UnicodeDecomposed, "\u1EC7"), std::string("e\u0323\u0302"));
    EXPECT_EQ(encode(nexkey::Encoding::UnicodeDecomposed, "\u1EAD"), std::string("a\u0323\u0302"));
    EXPECT_EQ(encode(nexkey::Encoding::UnicodeDecomposed, "\u1EB7"), std::string("a\u0323\u0306"));
    // The horn is lighter than the dot, so it stays in front (ự, ợ).
    EXPECT_EQ(encode(nexkey::Encoding::UnicodeDecomposed, "\u1EF1"),
              std::string("u\u031B\u0323"));
    EXPECT_EQ(encode(nexkey::Encoding::UnicodeDecomposed, "\u1EE3"),
              std::string("o\u031B\u0323"));
}

TEST(normalize_both_forms)
{
    const std::string nfc = "Ti\u1EBFng Vi\u1EC7t \u0111\u01B0\u1EE3c";
    const std::string nfd = "Tie\u0302\u0301ng Vie\u0323\u0302t \u0111u\u031Bo\u031B\u0323c";

    EXPECT_EQ(nexkey::Converter::normalize(nfd, nexkey::Normalization::Nfc), nfc);
    EXPECT_EQ(nexkey::Converter::normalize(nfc, nexkey::Normalization::Nfd), nfd);
    // Both forms are fixed points of their own normalization.
    EXPECT_EQ(nexkey::Converter::normalize(nfd, nexkey::Normalization::Nfd), nfd);
    EXPECT_EQ(nexkey::Converter::normalize(nfc, nexkey::Normalization::Nfc), nfc);
    EXPECT_EQ(nexkey::Converter::normalize("", nexkey::Normalization::Nfd), std::string());
}

TEST(normalize_composes_mixed_input)
{
    // A precomposed letter followed by a combining mark is one letter.
    EXPECT_EQ(nexkey::Converter::normalize("\u0103\u0301", nexkey::Normalization::Nfc),
              std::string("\u1EAF"));
    EXPECT_EQ(nexkey::Converter::normalize("a\u0306\u0301", nexkey::Normalization::Nfc),
              std::string("\u1EAF"));
    EXPECT_EQ(nexkey::Converter::normalize("o\u0302\u0323", nexkey::Normalization::Nfd),
              std::string("o\u0323\u0302"));

    // Marks that arrive in the "wrong" order are put in canonical order first.
    // Acute and circumflex have the same combining class, so their order is
    // kept: NFC gives the precomposed a with an acute plus a combining
    // circumflex, because no single code point spells that pair. The built-in
    // fallback is more helpful than the standard and composes the Vietnamese
    // letter instead; both results are valid, they are just not the same text.
    const auto reordered = nexkey::Converter::normalize("a\u0301\u0302", nexkey::Normalization::Nfc);
    EXPECT_TRUE(nexkey::unicode::is_valid(reordered));
    if (nexkey::unicode::has_full_unicode()) {
        EXPECT_EQ(reordered, std::string("\u00E1\u0302"));
    } else {
        EXPECT_EQ(reordered, std::string("\u1EA5"));
    }

    // Whatever the input, normalizing twice changes nothing.
    for (const std::string_view sample : {"a\u0301\u0302", "\u1EC7", "o\u0323\u0302", "abc"}) {
        for (const auto form : {nexkey::Normalization::Nfc, nexkey::Normalization::Nfd}) {
            const auto once = nexkey::Converter::normalize(sample, form);
            EXPECT_EQ(nexkey::Converter::normalize(once, form), once);
        }
    }
}

TEST(normalize_outside_the_vietnamese_alphabet)
{
    // Text that has no decomposition is not touched, whatever the backend.
    EXPECT_EQ(nexkey::Converter::normalize("\u65E5\u672C\u8A9E", nexkey::Normalization::Nfc),
              std::string("\u65E5\u672C\u8A9E"));
    EXPECT_EQ(nexkey::Converter::normalize("\xF0\x9F\x98\x80 ok", nexkey::Normalization::Nfd),
              std::string("\xF0\x9F\x98\x80 ok"));
    EXPECT_EQ(nexkey::Converter::normalize("NexKey 2.0", nexkey::Normalization::Nfd),
              std::string("NexKey 2.0"));

    // A letter outside the Vietnamese alphabet is decomposed by ICU and left
    // alone by the built-in fallback, which only knows the alphabet.
    const auto foreign = nexkey::Converter::normalize("na\u00EFve", nexkey::Normalization::Nfd);
    EXPECT_TRUE(nexkey::unicode::is_valid(foreign));
    if (nexkey::unicode::has_full_unicode()) {
        EXPECT_EQ(foreign, std::string("nai\u0308ve"));
    } else {
        EXPECT_EQ(foreign, std::string("na\u00EFve"));
    }

    // Malformed input is decoded with U+FFFD, so the output stays valid UTF-8.
    const auto repaired = nexkey::Converter::normalize(std::string("\xFF\x41", 2),
                                                       nexkey::Normalization::Nfd);
    EXPECT_TRUE(nexkey::unicode::is_valid(repaired));
}

TEST(equal_normalized)
{
    const std::string nfc = "Ti\u1EBFng Vi\u1EC7t";
    const std::string nfd = "Tie\u0302\u0301ng Vie\u0323\u0302t";
    EXPECT_TRUE(nexkey::Converter::equal_normalized(nfc, nfd));
    EXPECT_TRUE(nexkey::Converter::equal_normalized(nfd, nfc));
    EXPECT_TRUE(nexkey::Converter::equal_normalized(nfc, nfc));
    EXPECT_TRUE(nexkey::Converter::equal_normalized("", ""));

    // Different text stays different, including one tone mark.
    EXPECT_FALSE(nexkey::Converter::equal_normalized(nfc, "Ti\u1EBFng Vi\u1EC7n"));
    EXPECT_FALSE(nexkey::Converter::equal_normalized("ti\u1EBFng", "ti\u1EC7ng"));
    EXPECT_FALSE(nexkey::Converter::equal_normalized(nfc, ""));

    // Both spellings count outside the Vietnamese alphabet too, but only when
    // the Unicode database is available.
    EXPECT_EQ(nexkey::Converter::equal_normalized("na\u00EFve", "nai\u0308ve"),
              nexkey::unicode::has_full_unicode());
}
