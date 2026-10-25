// SPDX-License-Identifier: MIT
// NexKey - keystroke parsing for Telex, VNI and VIQR.

#include "Word.hpp"

#include "Syllable.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace nexkey::detail {

namespace {

constexpr char to_lower(char value) noexcept
{
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

constexpr bool is_upper(char value) noexcept
{
    return value >= 'A' && value <= 'Z';
}

constexpr bool is_letter(char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

constexpr bool is_digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

void append_literal(ParsedWord& word, char key)
{
    ParsedLetter letter{};
    letter.base = static_cast<char32_t>(to_lower(key));
    letter.upper = is_upper(key);
    letter.mark = Mark::None;
    letter.trigger = key;
    word.letters.push_back(letter);
}

void append_letter(ParsedWord& word, char32_t base, Mark mark, bool upper)
{
    ParsedLetter letter{};
    letter.base = base;
    letter.mark = mark;
    letter.upper = upper;
    letter.trigger = ' ';
    word.letters.push_back(letter);
}

ParsedLetter* last_letter(ParsedWord& word) noexcept
{
    return word.letters.empty() ? nullptr : &word.letters.back();
}

ParsedLetter* previous_letter(ParsedWord& word) noexcept
{
    return word.letters.size() < 2 ? nullptr : &word.letters[word.letters.size() - 2];
}

bool last_trigger_is(const ParsedLetter& letter, char key) noexcept
{
    return to_lower(letter.trigger) == to_lower(key);
}

bool has_vowel(const ParsedWord& word) noexcept
{
    return std::ranges::any_of(word.letters, [](const ParsedLetter& letter) {
        return is_vowel(letter.base);
    });
}

bool mark_fits(Mark mark, char32_t base) noexcept
{
    switch (mark) {
    case Mark::Circumflex:
        return base == U'a' || base == U'e' || base == U'o';
    case Mark::Breve:
        return base == U'a';
    case Mark::Horn:
        return base == U'o' || base == U'u';
    case Mark::Stroke:
        return base == U'd';
    case Mark::None:
        break;
    }
    return false;
}

/// Apply a mark to the previous letter, or drop it again when the same key is
/// pressed twice. Returns true when the keystroke was consumed.
bool apply_mark_toggle(ParsedWord& word, Mark mark, char key)
{
    auto* previous = last_letter(word);
    if (previous == nullptr) {
        return false;
    }
    if (previous->mark == Mark::None && mark_fits(mark, previous->base)) {
        previous->mark = mark;
        previous->trigger = key;
        return true;
    }
    if (previous->mark == mark && mark_fits(mark, previous->base) &&
        last_trigger_is(*previous, key)) {
        previous->mark = Mark::None;
        previous->trigger = ' ';
        append_literal(word, key);
        word.escaped = true;
        return true;
    }
    return false;
}

/// Horn the vowel pair "uo" as a whole, which is how the word-initial
/// horned pair is usually typed. Returns true when the key was consumed.
bool apply_horn_pair_toggle(ParsedWord& word, char key)
{
    auto* previous = last_letter(word);
    auto* before = previous_letter(word);
    if (previous == nullptr || before == nullptr) {
        return false;
    }
    if (previous->base != U'o' || before->base != U'u') {
        return false;
    }
    if (previous->mark == Mark::None && before->mark == Mark::None) {
        previous->mark = Mark::Horn;
        previous->trigger = key;
        before->mark = Mark::Horn;
        before->trigger = key;
        return true;
    }
    if (previous->mark == Mark::Horn && before->mark == Mark::Horn &&
        last_trigger_is(*previous, key) && last_trigger_is(*before, key)) {
        previous->mark = Mark::None;
        previous->trigger = ' ';
        before->mark = Mark::None;
        before->trigger = ' ';
        append_literal(word, key);
        word.escaped = true;
        return true;
    }
    return false;
}

/// Apply a tone mark, or drop it when the same key is pressed twice.
/// Returns true when the keystroke was consumed.
bool apply_tone_toggle(ParsedWord& word, Tone tone, char key)
{
    if (tone == Tone::None) {
        return false;
    }
    if (word.tone == tone) {
        word.tone = Tone::None;
        word.tone_trigger = ' ';
        append_literal(word, key);
        word.escaped = true;
        return true;
    }
    if (has_vowel(word)) {
        word.tone = tone;
        word.tone_trigger = key;
        return true;
    }
    return false;
}

std::optional<Tone> telex_tone(char key) noexcept
{
    switch (to_lower(key)) {
    case 's':
        return Tone::Acute;
    case 'f':
        return Tone::Grave;
    case 'r':
        return Tone::Hook;
    case 'x':
        return Tone::Tilde;
    case 'j':
        return Tone::Dot;
    default:
        return std::nullopt;
    }
}

std::optional<Tone> vni_tone(char key) noexcept
{
    switch (key) {
    case '1':
        return Tone::Acute;
    case '2':
        return Tone::Grave;
    case '3':
        return Tone::Hook;
    case '4':
        return Tone::Tilde;
    case '5':
        return Tone::Dot;
    default:
        return std::nullopt;
    }
}

std::optional<Tone> viqr_tone(char key) noexcept
{
    switch (key) {
    case '\'':
        return Tone::Acute;
    case '`':
        return Tone::Grave;
    case '?':
        return Tone::Hook;
    case '~':
        return Tone::Tilde;
    case '.':
        return Tone::Dot;
    default:
        return std::nullopt;
    }
}

std::optional<Mark> viqr_mark(char key) noexcept
{
    switch (key) {
    case '^':
        return Mark::Circumflex;
    case '(':
        return Mark::Breve;
    case '+':
        return Mark::Horn;
    case '-':
        return Mark::Stroke;
    default:
        return std::nullopt;
    }
}

ParsedWord parse_telex(std::string_view raw, const Options& options, bool simple)
{
    ParsedWord word;
    for (const char key : raw) {
        const char lower = to_lower(key);

        if (!simple && options.bracket_shortcuts) {
            if (lower == '[' || lower == ']' || lower == '{' || lower == '}') {
                const bool want_o = lower == '[' || lower == '{';
                const bool upper = key == '{' || key == '}';
                const char32_t target = want_o ? U'o' : U'u';
                if (auto* previous = last_letter(word); previous != nullptr &&
                    previous->base == target) {
                    if (previous->mark == Mark::None) {
                        previous->mark = Mark::Horn;
                        previous->trigger = key;
                        continue;
                    }
                    if (previous->mark == Mark::Horn && last_trigger_is(*previous, key)) {
                        previous->mark = Mark::None;
                        previous->trigger = ' ';
                        append_literal(word, key);
                        word.escaped = true;
                        continue;
                    }
                }
                append_letter(word, target, Mark::Horn, upper);
                continue;
            }
        }

        // A doubled letter produces a vowel modifier or the stroked d.
        if (auto* previous = last_letter(word); previous != nullptr) {
            if (previous->base == static_cast<char32_t>(lower)) {
                if (previous->mark == Mark::None &&
                    (lower == 'a' || lower == 'e' || lower == 'o' || lower == 'd')) {
                    previous->mark = lower == 'd' ? Mark::Stroke : Mark::Circumflex;
                    previous->trigger = key;
                    continue;
                }
                if (last_trigger_is(*previous, key) && mark_fits(previous->mark, previous->base) &&
                    previous->mark != Mark::None) {
                    previous->mark = Mark::None;
                    previous->trigger = ' ';
                    append_literal(word, key);
                    word.escaped = true;
                    continue;
                }
            }
        }

        if (!simple && lower == 'w') {
            if (apply_horn_pair_toggle(word, key)) {
                continue;
            }
            if (auto* previous = last_letter(word); previous != nullptr) {
                if (previous->mark == Mark::None &&
                    (previous->base == U'a' || previous->base == U'o' ||
                     previous->base == U'u')) {
                    previous->mark = previous->base == U'a' ? Mark::Breve : Mark::Horn;
                    previous->trigger = key;
                    continue;
                }
                if (last_trigger_is(*previous, key) &&
                    (previous->mark == Mark::Breve || previous->mark == Mark::Horn)) {
                    previous->mark = Mark::None;
                    previous->trigger = ' ';
                    append_literal(word, key);
                    word.escaped = true;
                    continue;
                }
            }
            if (!has_vowel(word)) {
                append_letter(word, U'u', Mark::Horn, is_upper(key));
                continue;
            }
            append_literal(word, key);
            continue;
        }

        if (const auto tone = telex_tone(key)) {
            if (apply_tone_toggle(word, *tone, key)) {
                continue;
            }
            append_literal(word, key);
            continue;
        }

        if (lower == 'z' && word.tone != Tone::None) {
            word.tone = Tone::None;
            word.tone_trigger = ' ';
            continue;
        }

        append_literal(word, key);
    }
    return word;
}

ParsedWord parse_vni(std::string_view raw)
{
    ParsedWord word;
    for (const char key : raw) {
        if (const auto tone = vni_tone(key)) {
            if (apply_tone_toggle(word, *tone, key)) {
                continue;
            }
            append_literal(word, key);
            continue;
        }

        if (is_digit(key)) {
            if (key == '0' && word.tone != Tone::None) {
                word.tone = Tone::None;
                word.tone_trigger = ' ';
                continue;
            }
            Mark mark = Mark::None;
            switch (key) {
            case '6':
                mark = Mark::Circumflex;
                break;
            case '7':
                mark = Mark::Horn;
                break;
            case '8':
                mark = Mark::Breve;
                break;
            case '9':
                mark = Mark::Stroke;
                break;
            default:
                break;
            }
            if (mark == Mark::Horn && apply_horn_pair_toggle(word, key)) {
                continue;
            }
            if (mark != Mark::None && apply_mark_toggle(word, mark, key)) {
                continue;
            }
            append_literal(word, key);
            continue;
        }

        append_literal(word, key);
    }
    return word;
}

ParsedWord parse_viqr(std::string_view raw)
{
    ParsedWord word;
    for (const char key : raw) {
        if (const auto mark = viqr_mark(key)) {
            if (apply_mark_toggle(word, *mark, key)) {
                continue;
            }
            append_literal(word, key);
            continue;
        }

        if (const auto tone = viqr_tone(key)) {
            const auto* previous = word.letters.empty() ? nullptr : &word.letters.back();
            if (previous != nullptr && is_vowel(previous->base) &&
                apply_tone_toggle(word, *tone, key)) {
                continue;
            }
            append_literal(word, key);
            continue;
        }

        append_literal(word, key);
    }
    return word;
}

} // namespace

std::vector<Letter> ParsedWord::to_letters() const
{
    std::vector<Letter> result;
    result.reserve(letters.size());
    for (const auto& letter : letters) {
        result.push_back(Letter{letter.base, letter.mark, letter.upper});
    }
    return result;
}

ParsedWord parse(Method method, std::string_view raw, const Options& options)
{
    switch (method) {
    case Method::Telex:
        return parse_telex(raw, options, false);
    case Method::SimpleTelex:
        return parse_telex(raw, options, true);
    case Method::Vni:
        return parse_vni(raw);
    case Method::Viqr:
        return parse_viqr(raw);
    }
    return {};
}

std::u32string compose_word(const ParsedWord& word, const Options& options)
{
    const auto letters = word.to_letters();
    const auto position =
        word.tone == Tone::None ? std::nullopt : tone_index(letters, options.tone_style);

    std::u32string out;
    out.reserve(letters.size());
    for (std::size_t index = 0; index < letters.size(); ++index) {
        const auto tone = position.has_value() && *position == index ? word.tone : Tone::None;
        out.push_back(compose(letters[index], tone));
    }
    return out;
}

bool is_word_boundary(char32_t code_point, const Options& options) noexcept
{
    if (code_point > 0x7F) {
        return false;
    }
    const auto value = static_cast<char>(code_point);
    if (std::isalnum(static_cast<unsigned char>(value)) != 0) {
        return false;
    }

    switch (options.method) {
    case Method::Vni:
        break;
    case Method::Viqr:
        if (std::string_view{"^()+-'`?~."}.find(value) != std::string_view::npos) {
            return false;
        }
        break;
    case Method::Telex:
    case Method::SimpleTelex:
        if (options.bracket_shortcuts &&
            std::string_view{"[]{}"}.find(value) != std::string_view::npos) {
            return false;
        }
        break;
    }
    return true;
}

} // namespace nexkey::detail
