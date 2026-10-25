// SPDX-License-Identifier: MIT
// Character set conversion.

#include "Helpers.hpp"
#include "TestHarness.hpp"

#include "nexkey/Converter.hpp"
#include "nexkey/Unicode.hpp"

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
