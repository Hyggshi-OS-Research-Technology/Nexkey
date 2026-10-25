// SPDX-License-Identifier: MIT
// NexKey - help screens and option parsing.

#include "Cli.hpp"

#include "nexkey/Types.hpp"

#include <algorithm>
#include <ostream>

namespace nexkey::cli {

void print_version(std::ostream& out)
{
    out << kProgramName << ' ' << kVersion << "\n"
        << "Vietnamese input method for Linux.\n"
        << "MIT licensed. Built with C++23.\n";
}

void print_methods(std::ostream& out)
{
    out << "Input methods:\n"
        << "  telex          Full Telex, including the w and bracket shortcuts (default)\n"
        << "  telex-simple   Telex with tones and doubled letters only\n"
        << "  vni            VNI, digits 1-9 for tones and vowel modifiers\n"
        << "  viqr           VIQR, pure ASCII notation\n";
}

void print_encodings(std::ostream& out)
{
    out << "Character sets:\n"
        << "  unicode              Precomposed UTF-8 (default)\n"
        << "  unicode-decomposed   UTF-8 in Unicode NFD\n"
        << "  tcvn3                Legacy TCVN3 (ABC) single byte charset\n"
        << "  viqr                 Pure ASCII VIQR notation\n";
}

void print_help(std::ostream& out)
{
    out << "NexKey " << kVersion << " - Vietnamese input method for Linux\n"
        << "\n"
        << "Usage:\n"
        << "  nexkey <command> [options]\n"
        << "\n"
        << "Commands:\n"
        << "  test [options] [text...]  Compose text through the engine\n"
        << "  convert [options]         Convert text between character sets\n"
        << "  status                    Print the current configuration\n"
        << "  toggle                    Switch between Vietnamese and English mode\n"
        << "  config [show|get|set|path]\n"
        << "                            Inspect or change the configuration\n"
        << "  methods                   List the input methods\n"
        << "  encodings                 List the character sets\n"
        << "  version                   Print the version\n"
        << "  help                      Print this help\n"
        << "\n"
        << "Options for test:\n"
        << "  --method <name>           telex (default), telex-simple, vni, viqr\n"
        << "  --encoding <name>         Output character set\n"
        << "  --tone-style <name>       modern (default) or traditional\n"
        << "  --no-fallback             Apply the rules even to non-Vietnamese words\n"
        << "  --no-brackets             Disable the Telex bracket shortcuts\n"
        << "  --no-smart-backspace      Delete one keystroke per backspace\n"
        << "  --no-candidates           Do not offer alternative interpretations\n"
        << "  --candidates              List the candidate window entries\n"
        << "  --trace                   Print the composition after every keystroke\n"
        << "\n"
        << "Options for convert:\n"
        << "  --from <name>             Source character set (required)\n"
        << "  --to <name>               Target character set (required)\n"
        << "  --input <file>            Read from a file instead of standard input\n"
        << "  --output <file>           Write to a file instead of standard output\n"
        << "\n"
        << "Config keys:\n"
        << "  input.method              input.tone_style            input.enabled\n"
        << "  output.encoding           engine.smart_backspace      engine.english_fallback\n"
        << "  engine.bracket_shortcuts  engine.candidates           ui.toggle_key\n"
        << "  ui.show_tray_icon\n"
        << "  ui.start_with_system\n"
        << "\n"
        << "Examples:\n"
        << "  nexkey test \"tieengs\"\n"
        << "  nexkey test --method vni \"tie6ng1\"\n"
        << "  nexkey test --tone-style traditional \"hoaf\"\n"
        << "  nexkey test --candidates \"hoaf\"\n"
        << "  nexkey convert --from tcvn3 --to unicode < input.txt\n"
        << "  nexkey config set input.method vni\n";
}

OptionParser::OptionParser(const Args& args,
                           std::initializer_list<std::string_view> value_options)
{
    for (std::size_t index = 0; index < args.size(); ++index) {
        const auto& argument = args[index];
        if (argument.size() > 2 && argument.starts_with("--")) {
            if (const auto separator = argument.find('='); separator != std::string::npos) {
                m_options.emplace_back(argument.substr(2, separator - 2),
                                       argument.substr(separator + 1));
                continue;
            }

            const auto name = argument.substr(2);
            const bool takes_value = std::ranges::find(value_options, name) != value_options.end();
            if (takes_value && index + 1 < args.size() && !args[index + 1].starts_with("--")) {
                m_options.emplace_back(name, args[++index]);
                continue;
            }
            m_options.emplace_back(argument.substr(2), std::string{});
            continue;
        }
        if (argument.starts_with('-') && argument.size() > 1) {
            m_unknown.push_back(argument);
            continue;
        }
        m_positional.push_back(argument);
    }
}

std::optional<std::string> OptionParser::value(std::string_view name) const
{
    for (const auto& [key, value] : m_options) {
        if (key == name) {
            return value;
        }
    }
    return std::nullopt;
}

bool OptionParser::has(std::string_view name) const
{
    for (const auto& [key, value] : m_options) {
        static_cast<void>(value);
        if (key == name) {
            return true;
        }
    }
    return false;
}

} // namespace nexkey::cli
