// SPDX-License-Identifier: MIT
// UTF-8 decoding, encoding and the measuring helpers used by the commit path.

#include "TestHarness.hpp"

#include "nexkey/Grapheme.hpp"
#include "nexkey/Unicode.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

namespace {

using nexkey::unicode::kReplacementCharacter;

std::string utf8(char32_t code_point)
{
    std::string out;
    nexkey::unicode::append(out, code_point);
    return out;
}

/// `count` times U+FFFD, so a failed decode is easy to read in the output.
std::u32string replacements(std::size_t count)
{
    return std::u32string(count, kReplacementCharacter);
}

std::u32string code_points(std::initializer_list<char32_t> values)
{
    return std::u32string(values);
}

} // namespace

// ---------------------------------------------------------------------------
// Well formed UTF-8
// ---------------------------------------------------------------------------

TEST(unicode_round_trip)
{
    const std::string text = "Ti\u1EBFng Vi\u1EC7t \u0111\u01B0\u1EE3c";
    const auto points = nexkey::unicode::decode(text);
    EXPECT_EQ(points.size(), std::size_t{15});
    EXPECT_EQ(nexkey::unicode::encode(points), text);
    EXPECT_TRUE(nexkey::unicode::is_valid(text));
}

TEST(unicode_handles_four_byte_sequences)
{
    const std::string text{"\xF0\x9F\x98\x80"};
    const auto points = nexkey::unicode::decode(text);
    EXPECT_EQ(points.size(), std::size_t{1});
    EXPECT_EQ(points.front(), char32_t{0x1F600});
    EXPECT_EQ(nexkey::unicode::encode(points), text);
}

TEST(unicode_encoded_length)
{
    EXPECT_EQ(nexkey::unicode::encoded_length(U'a'), std::size_t{1});
    EXPECT_EQ(nexkey::unicode::encoded_length(U'\u00E1'), std::size_t{2});
    EXPECT_EQ(nexkey::unicode::encoded_length(U'\u1EBF'), std::size_t{3});
    EXPECT_EQ(nexkey::unicode::encoded_length(char32_t{0x1F600}), std::size_t{4});
    // Not scalar values: append writes U+FFFD, so the length is three bytes.
    EXPECT_EQ(nexkey::unicode::encoded_length(char32_t{0xD800}), std::size_t{3});
    EXPECT_EQ(nexkey::unicode::encoded_length(char32_t{0x110000}), std::size_t{3});
}

TEST(unicode_sequence_boundaries)
{
    struct Sample {
        char32_t code_point;
        std::string_view bytes;
    };
    const Sample samples[] = {
        {char32_t{0x0000}, std::string_view("\x00", 1)},
        {char32_t{0x007F}, std::string_view("\x7F", 1)},
        {char32_t{0x0080}, std::string_view("\xC2\x80", 2)},
        {char32_t{0x07FF}, std::string_view("\xDF\xBF", 2)},
        {char32_t{0x0800}, std::string_view("\xE0\xA0\x80", 3)},
        {char32_t{0xD7FF}, std::string_view("\xED\x9F\xBF", 3)},
        {char32_t{0xE000}, std::string_view("\xEE\x80\x80", 3)},
        {char32_t{0xFFFD}, std::string_view("\xEF\xBF\xBD", 3)},
        {char32_t{0xFFFF}, std::string_view("\xEF\xBF\xBF", 3)},
        {char32_t{0x10000}, std::string_view("\xF0\x90\x80\x80", 4)},
        {char32_t{0x10FFFF}, std::string_view("\xF4\x8F\xBF\xBF", 4)},
    };

    for (const auto& sample : samples) {
        EXPECT_EQ(utf8(sample.code_point), std::string(sample.bytes));
        EXPECT_EQ(nexkey::unicode::decode(sample.bytes), code_points({sample.code_point}));
        EXPECT_TRUE(nexkey::unicode::is_valid(sample.bytes));
        // Every proper prefix of a sequence is malformed.
        for (std::size_t length = 1; length < sample.bytes.size(); ++length) {
            EXPECT_FALSE(nexkey::unicode::is_valid(sample.bytes.substr(0, length)));
        }
    }
}

TEST(unicode_replaces_overlong_forms)
{
    // The classic "modified UTF-8" of Java and the old security holes: these
    // would decode to a NUL, a solidus or an ASCII letter.
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xC0\x80", 2)), replacements(2));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xC1\xBF", 2)), replacements(2));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xE0\x80\xAF", 3)), replacements(3));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xF0\x80\x80\xAF", 4)), replacements(4));
    EXPECT_FALSE(nexkey::unicode::is_valid("\xC0\x80"));
    EXPECT_FALSE(nexkey::unicode::is_valid("\xE0\x80\xAF"));
}

TEST(unicode_rejects_surrogates)
{
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xED\xA0\x80", 3)), replacements(3));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xED\xBF\xBF", 3)), replacements(3));
    EXPECT_FALSE(nexkey::unicode::is_valid("\xED\xA0\x80"));
    // U+D7FF and U+E000 sit next to the surrogate range and are fine.
    EXPECT_TRUE(nexkey::unicode::is_valid("\xED\x9F\xBF"));
    EXPECT_TRUE(nexkey::unicode::is_valid("\xEE\x80\x80"));
}

TEST(unicode_rejects_code_points_above_the_unicode_range)
{
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xF4\x90\x80\x80", 4)), replacements(4));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xF5\x80\x80\x80", 4)), replacements(4));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xFE\xFF", 2)), replacements(2));
    EXPECT_FALSE(nexkey::unicode::is_valid("\xF4\x90\x80\x80"));
    EXPECT_FALSE(nexkey::unicode::is_valid("\xFE"));
}

TEST(unicode_replaces_invalid_sequences)
{
    const std::string broken{"\xFF\x41"};
    const auto points = nexkey::unicode::decode(broken);
    EXPECT_EQ(points.size(), std::size_t{2});
    EXPECT_EQ(points.front(), kReplacementCharacter);
    EXPECT_EQ(points.back(), char32_t{'A'});
}

TEST(unicode_replaces_one_maximal_subpart_per_error)
{
    // Unicode 15.0, table 3-8: a truncated but still valid prefix is one error,
    // the bytes that cannot belong to it are separate errors.
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xE1\x80\x41", 3)),
              code_points({kReplacementCharacter, U'A'}));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xF0\x9F\x41", 3)),
              code_points({kReplacementCharacter, U'A'}));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xE1", 1)), replacements(1));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xE1\x80", 2)), replacements(1));
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\xF0\x9F\x98", 3)), replacements(1));
    // A lone continuation byte is its own error.
    EXPECT_EQ(nexkey::unicode::decode(std::string_view("\x80\xBF", 2)), replacements(2));
}

TEST(unicode_append_stays_valid)
{
    // Surrogates and out of range code points have no UTF-8 encoding.
    EXPECT_EQ(utf8(char32_t{0xD800}), std::string("\xEF\xBF\xBD"));
    EXPECT_EQ(utf8(char32_t{0xDFFF}), std::string("\xEF\xBF\xBD"));
    EXPECT_EQ(utf8(char32_t{0x110000}), std::string("\xEF\xBF\xBD"));
    EXPECT_EQ(utf8(char32_t{0xFFFFFFFF}), std::string("\xEF\xBF\xBD"));

    std::u32string text{char32_t{0xD83D}, U'a', char32_t{0x110000}};
    EXPECT_EQ(nexkey::unicode::encode(text), std::string("\xEF\xBF\xBD" "a\xEF\xBF\xBD"));
    EXPECT_TRUE(nexkey::unicode::is_valid(nexkey::unicode::encode(text)));
}

TEST(unicode_encoded_length_matches_append)
{
    std::string buffer;
    for (char32_t code_point = 0; code_point <= 0x110000; ++code_point) {
        buffer.clear();
        nexkey::unicode::append(buffer, code_point);
        EXPECT_EQ(buffer.size(), nexkey::unicode::encoded_length(code_point));
        const auto expected =
            nexkey::unicode::is_scalar_value(code_point) ? code_point : kReplacementCharacter;
        EXPECT_EQ(nexkey::unicode::decode(buffer), code_points({expected}));
        if (nexkey::unicode::encoded_length(code_point) != buffer.size()) {
            return; // one report per broken range is enough
        }
    }
}

TEST(unicode_output_is_always_decodable)
{
    // Whatever is fed in, what comes out of a decode/encode round trip is valid
    // UTF-8. This is the property the frontends rely on when they compare the
    // document with what they wrote.
    const std::string samples[] = {
        "",
        "Ti\u1EBFng",
        std::string("\xE1\x80\x41", 3),
        std::string("\xED\xA0\x80", 3),
        std::string("\xF0\x9F\x98\x80\xFF\x41", 6),
        std::string("\xF4\x90\x80\x80", 4),
        std::string("a\x80\x80" "b", 4),
    };
    for (const auto& sample : samples) {
        const auto round_trip = nexkey::unicode::encode(nexkey::unicode::decode(sample));
        EXPECT_TRUE(nexkey::unicode::is_valid(round_trip));
    }
}

TEST(unicode_counts_match_decode)
{
    const std::string samples[] = {
        "",
        "Vi\u1EC7t",
        "e\u0302\u0301",
        std::string("\xE1\x80\x41", 3),
        std::string("\xED\xA0\x80", 3),
        std::string("\xFF", 1),
        "\xE2\x9D\xA4\xEF\xB8\x8F",
    };
    for (const auto& sample : samples) {
        EXPECT_EQ(nexkey::unicode::count_code_points(sample),
                  nexkey::unicode::decode(sample).size());
    }
}

TEST(unicode_count_code_points)
{
    EXPECT_EQ(nexkey::unicode::count_code_points(""), std::size_t{0});
    EXPECT_EQ(nexkey::unicode::count_code_points("NexKey 2.0"), std::size_t{10});
    EXPECT_EQ(nexkey::unicode::count_code_points("Vi\u1EC7t"), std::size_t{4});
    // Three code points, one visible character.
    EXPECT_EQ(nexkey::unicode::count_code_points("e\u0302\u0301"), std::size_t{3});
    EXPECT_EQ(nexkey::unicode::count_code_points(std::string("\xFF\x41", 2)), std::size_t{2});
}

TEST(unicode_prefix_length)
{
    // One byte for V and i, three for the e with its two marks.
    EXPECT_EQ(nexkey::unicode::prefix_length("Vi\u1EC7t", 3), std::size_t{5});
    EXPECT_EQ(nexkey::unicode::prefix_length("Vi\u1EC7t", 0), std::size_t{0});
    EXPECT_EQ(nexkey::unicode::prefix_length("Vi\u1EC7t", 4), std::size_t{6});
    EXPECT_EQ(nexkey::unicode::prefix_length("Vi\u1EC7t", 99), std::size_t{6});

    // Never split a code point, whatever the count.
    const std::string text = "e\u0302\u0301 \u0111\u01B0\u1EE3c \xF0\x9F\x98\x80";
    for (std::size_t count = 0; count <= 12; ++count) {
        const auto length = nexkey::unicode::prefix_length(text, count);
        EXPECT_TRUE(nexkey::unicode::is_valid(text.substr(0, length)));
        EXPECT_TRUE(nexkey::unicode::is_valid(text.substr(length)));
    }
}

// ---------------------------------------------------------------------------
// Grapheme clusters
// ---------------------------------------------------------------------------

TEST(unicode_count_graphemes)
{
    EXPECT_EQ(nexkey::unicode::count_graphemes(""), std::size_t{0});
    EXPECT_EQ(nexkey::unicode::count_graphemes("NexKey"), std::size_t{6});
    EXPECT_EQ(nexkey::unicode::count_graphemes("Vi\u1EC7t"), std::size_t{4});
    // The same word in NFD: e + circumflex + acute is still one character.
    EXPECT_EQ(nexkey::unicode::count_graphemes("Vie\u0302\u0301t"), std::size_t{4});
    EXPECT_EQ(nexkey::unicode::count_graphemes("e\u0302\u0301"), std::size_t{1});
    EXPECT_EQ(nexkey::unicode::count_graphemes("a\u0301\u0302b"), std::size_t{2});
    // Emoji with a variation selector or a skin tone stay one character.
    EXPECT_EQ(nexkey::unicode::count_graphemes("\xE2\x9D\xA4\xEF\xB8\x8F"), std::size_t{1});
    EXPECT_EQ(nexkey::unicode::count_graphemes("\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD"), std::size_t{1});
    EXPECT_EQ(nexkey::unicode::count_graphemes("\xF0\x9F\x98\x80\xF0\x9F\x91\x8D"), std::size_t{2});
    // A combining mark without a base is a character of its own.
    EXPECT_EQ(nexkey::unicode::count_graphemes("\u0301" "a"), std::size_t{2});
}

TEST(unicode_grapheme_boundary)
{
    const std::string precomposed = "Vi\u1EC7t"; // V i e+marks t, six bytes
    EXPECT_EQ(nexkey::unicode::grapheme_boundary(precomposed, 0), precomposed.size());
    EXPECT_EQ(nexkey::unicode::grapheme_boundary(precomposed, 1), std::size_t{5});
    EXPECT_EQ(nexkey::unicode::grapheme_boundary(precomposed, 2), std::size_t{2});
    EXPECT_EQ(nexkey::unicode::grapheme_boundary(precomposed, 3), std::size_t{1});
    EXPECT_EQ(nexkey::unicode::grapheme_boundary(precomposed, 4), std::size_t{0});
    EXPECT_EQ(nexkey::unicode::grapheme_boundary(precomposed, 99), std::size_t{0});

    // The decomposed form removes the whole letter, not just the accent.
    const std::string decomposed = "e\u0302\u0301";
    EXPECT_EQ(nexkey::unicode::grapheme_boundary(decomposed, 1), std::size_t{0});
    EXPECT_EQ(decomposed.substr(0, nexkey::unicode::grapheme_boundary(decomposed, 0)),
              decomposed);

    EXPECT_EQ(precomposed.substr(nexkey::unicode::grapheme_boundary(precomposed, 2)),
              std::string("\u1EC7t"));
    EXPECT_EQ(nexkey::unicode::grapheme_boundary("", 1), std::size_t{0});
}

// ---------------------------------------------------------------------------
// Common prefix: the cut between "delete" and "commit"
// ---------------------------------------------------------------------------

TEST(unicode_common_prefix_counts)
{
    const std::string old_text = "Ti\u1EBFng Vi\u1EC7t";
    const std::string new_text = "Ti\u1EBFng Vi\u1EC7c";
    const auto prefix = nexkey::unicode::common_prefix(old_text, new_text);
    const std::string expected = "Ti\u1EBFng Vi\u1EC7"; // 9 characters, 5 of them ASCII
    EXPECT_EQ(prefix.bytes, expected.size());
    EXPECT_EQ(prefix.code_points, std::size_t{9});
    EXPECT_EQ(prefix.graphemes, std::size_t{9});
    EXPECT_EQ(old_text.substr(0, prefix.bytes), expected);
    EXPECT_EQ(new_text.substr(0, prefix.bytes), expected);
}

TEST(unicode_common_prefix_never_splits_a_code_point)
{
    // 'a' (61) and 'a' with an acute (C3 A1) share their first byte only, so
    // a byte based prefix would cut the sequence of the second one in half.
    const auto prefix = nexkey::unicode::common_prefix("a", "\u00E1");
    EXPECT_EQ(prefix.bytes, std::size_t{0});
    EXPECT_EQ(prefix.code_points, std::size_t{0});

    // The two Vietnamese letters 1EBF (E1 BA BF) and 1EC7 (E1 BB 87) share
    // their first two bytes, not three.
    const auto vietnamese = nexkey::unicode::common_prefix("ti\u1EBFng", "ti\u1EC7ng");
    EXPECT_EQ(vietnamese.bytes, std::size_t{2});
    EXPECT_EQ(vietnamese.code_points, std::size_t{2});

    // Malformed input stops at the end of the maximal subpart as well.
    const auto broken = nexkey::unicode::common_prefix(std::string("\xE1\x80\x41", 3),
                                                       std::string("\xE1\x80\x42", 3));
    EXPECT_EQ(broken.bytes, std::size_t{2});
    EXPECT_EQ(broken.code_points, std::size_t{1});
}

TEST(unicode_common_prefix_basics)
{
    EXPECT_EQ(nexkey::unicode::common_prefix("", "abc").bytes, std::size_t{0});
    EXPECT_EQ(nexkey::unicode::common_prefix("abc", "").code_points, std::size_t{0});
    EXPECT_EQ(nexkey::unicode::common_prefix("abc", "abcd").bytes, std::size_t{3});
    EXPECT_EQ(nexkey::unicode::common_prefix("abc", "abc").graphemes, std::size_t{3});
    EXPECT_EQ(nexkey::unicode::common_prefix("abc", "abd").code_points, std::size_t{2});

    // One more combining mark on the same letter: the shared part is one
    // character even though it is cut in the middle of a cluster.
    const auto marks = nexkey::unicode::common_prefix("e\u0302", "e\u0302\u0301");
    EXPECT_EQ(marks.bytes, std::size_t{3});
    EXPECT_EQ(marks.code_points, std::size_t{2});
    EXPECT_EQ(marks.graphemes, std::size_t{1});
}

// ---------------------------------------------------------------------------
// Properties that have to hold for every input
// ---------------------------------------------------------------------------

TEST(unicode_measuring_invariants)
{
    // A deterministic pseudo random generator, so a failure can be reproduced
    // and the suite has no hidden dependency on the clock.
    std::uint32_t state = 0x9E3779B9U;
    const auto next = [&state](std::uint32_t limit) {
        state = state * 1664525U + 1013904223U;
        return static_cast<std::size_t>((state >> 8U) % limit);
    };

    // The alphabet mixes ASCII, precomposed Vietnamese, combining marks, a
    // legacy byte pair and emoji parts, so clusters of every length appear.
    constexpr char32_t alphabet[] = {
        U'a', U' ', U'\n', U'\u0301', U'\u0302', U'\u0323', U'\u1EBF', U'\u0111',
        U'\uFE0F', char32_t{0x1F44D}, char32_t{0x1F3FD},
    };

    for (int round = 0; round < 400; ++round) {
        const std::size_t length = 1 + next(10);
        std::u32string sample;
        for (std::size_t i = 0; i < length; ++i) {
            sample.push_back(alphabet[next(std::size(alphabet))]);
        }
        const auto text = nexkey::unicode::encode(sample);

        EXPECT_EQ(nexkey::unicode::count_code_points(text), sample.size());
        // Round tripping never changes the text and always produces valid UTF-8.
        EXPECT_EQ(nexkey::unicode::decode(text), sample);
        EXPECT_TRUE(nexkey::unicode::is_valid(text));
        EXPECT_EQ(nexkey::unicode::encode(nexkey::unicode::decode(text)), text);

        // Neither helper ever cuts a code point in half.
        for (std::size_t count = 0; count <= sample.size(); ++count) {
            const auto bytes = nexkey::unicode::prefix_length(text, count);
            EXPECT_TRUE(nexkey::unicode::is_valid(text.substr(0, bytes)));
            EXPECT_TRUE(nexkey::unicode::is_valid(text.substr(bytes)));

            const auto cut = nexkey::unicode::grapheme_boundary(text, count);
            EXPECT_TRUE(nexkey::unicode::is_valid(text.substr(0, cut)));
            EXPECT_TRUE(nexkey::unicode::is_valid(text.substr(cut)));
        }

        // Cutting the last cluster off always removes something, and what it
        // removes is one base character followed only by its own marks. (A run
        // of marks without a base is a cluster of its own, so the first code
        // point of the tail may itself be an extending one.)
        const auto last_cluster = nexkey::unicode::grapheme_boundary(text, 1);
        EXPECT_TRUE(last_cluster < text.size());
        const auto tail = nexkey::unicode::decode(text.substr(last_cluster));
        for (std::size_t i = 1; i < tail.size(); ++i) {
            EXPECT_TRUE(nexkey::unicode::is_extending(tail[i]));
        }

        // Removing one cluster at a time has to reach the end after exactly
        // count_graphemes steps.
        std::size_t clusters = 0;
        auto rest = std::string_view(text);
        while (!rest.empty()) {
            const auto cut = nexkey::unicode::grapheme_boundary(rest, 1);
            rest = rest.substr(0, cut);
            ++clusters;
            if (clusters > text.size()) {
                break; // a helper that does not make progress must not hang
            }
        }
        EXPECT_EQ(clusters, nexkey::unicode::count_graphemes(text));
    }
}

TEST(unicode_common_prefix_invariants)
{
    std::uint32_t state = 0x12345678U;
    const auto next = [&state](std::uint32_t limit) {
        state = state * 1664525U + 1013904223U;
        return static_cast<std::size_t>((state >> 8U) % limit);
    };

    constexpr char32_t alphabet[] = {U'a', U'b', U'c', U'i', U't', U'n', U'g', U' ',
                                     U'\u0301', U'\u1EC7', U'\u1EBF', char32_t{0x1F600}};
    for (int round = 0; round < 400; ++round) {
        std::u32string left;
        std::u32string right;
        const std::size_t common = next(5);
        for (std::size_t i = 0; i < common; ++i) {
            const char32_t code_point = alphabet[next(std::size(alphabet))];
            left.push_back(code_point);
            right.push_back(code_point);
        }
        for (std::size_t i = next(4); i > 0; --i) {
            left.push_back(alphabet[next(std::size(alphabet))]);
        }
        for (std::size_t i = next(4); i > 0; --i) {
            right.push_back(alphabet[next(std::size(alphabet))]);
        }

        const auto a = nexkey::unicode::encode(left);
        const auto b = nexkey::unicode::encode(right);
        const auto prefix = nexkey::unicode::common_prefix(a, b);

        EXPECT_TRUE(prefix.bytes <= a.size() && prefix.bytes <= b.size());
        const auto shared_bytes = a.substr(0, prefix.bytes);
        EXPECT_EQ(shared_bytes, b.substr(0, prefix.bytes));
        EXPECT_TRUE(nexkey::unicode::is_valid(shared_bytes));
        EXPECT_EQ(prefix.code_points, nexkey::unicode::count_code_points(shared_bytes));
        EXPECT_EQ(prefix.graphemes, nexkey::unicode::count_graphemes(shared_bytes));
        // The shared length is exactly the run of equal code points at the
        // start, even when the two strings share the first byte of a sequence
        // and nothing more (1EBF and 1EC7 are such a pair).
        const auto left_points = nexkey::unicode::decode(a);
        const auto right_points = nexkey::unicode::decode(b);
        const auto shared = std::min(left_points.size(), right_points.size());
        std::size_t expected = 0;
        while (expected < shared && left_points[expected] == right_points[expected]) {
            ++expected;
        }
        EXPECT_EQ(prefix.code_points, expected);
        EXPECT_EQ(prefix.bytes,
                  nexkey::unicode::encode(std::u32string_view(left_points).substr(0, expected))
                      .size());
    }
}

// ---------------------------------------------------------------------------
// Composition tables, unchanged behaviour
// ---------------------------------------------------------------------------

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
