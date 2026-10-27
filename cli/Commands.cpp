// SPDX-License-Identifier: MIT
// NexKey - command implementations.

#include "Cli.hpp"

#include "nexkey/Converter.hpp"
#include "nexkey/Delivery.hpp"
#include "nexkey/Engine.hpp"
#include "nexkey/Settings.hpp"
#include "nexkey/Types.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

namespace nexkey::cli {

namespace {

std::string read_stream(std::istream& in)
{
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::string join(const Args& args)
{
    std::string out;
    for (const auto& value : args) {
        if (!out.empty()) {
            out.push_back(' ');
        }
        out += value;
    }
    return out;
}

std::string_view bool_text(bool value)
{
    return value ? "true" : "false";
}

void print_settings(std::ostream& out, const Settings& settings)
{
    out << "mode=" << (settings.enabled ? "VI" : "EN") << '\n'
        << "enabled=" << bool_text(settings.enabled) << '\n'
        << "method=" << to_string(settings.options.method) << '\n'
        << "encoding=" << to_string(settings.options.encoding) << '\n'
        << "tone_style=" << to_string(settings.options.tone_style) << '\n'
        << "smart_backspace=" << bool_text(settings.options.smart_backspace) << '\n'
        << "english_fallback=" << bool_text(settings.options.english_fallback) << '\n'
        << "bracket_shortcuts=" << bool_text(settings.options.bracket_shortcuts) << '\n'
        << "candidates=" << bool_text(settings.options.candidates) << '\n'
        << "direct_typing=" << bool_text(settings.options.direct_typing) << '\n'
        << "toggle_key=" << settings.toggle_key << '\n'
        << "show_tray_icon=" << bool_text(settings.show_tray_icon) << '\n'
        << "start_with_system=" << bool_text(settings.start_with_system) << '\n';
    if (!settings.app_rules.empty()) {
        std::vector<std::string> programs;
        programs.reserve(settings.app_rules.size());
        for (const auto& [program, rule] : settings.app_rules) {
            static_cast<void>(rule);
            programs.push_back(program);
        }
        std::ranges::sort(programs);
        for (const auto& program : programs) {
            out << "app_rules." << program << '=' << settings.app_rules.at(program) << '\n';
        }
    }
    out << "config=" << default_config_path().string() << '\n';
}

/// The program part of an "app_rules.<program>" key, lower cased, or empty.
std::string app_rule_program(std::string_view key)
{
    constexpr std::string_view prefix{"app_rules."};
    if (!key.starts_with(prefix)) {
        return {};
    }
    std::string program(key.substr(prefix.size()));
    for (char& character : program) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return program;
}

std::optional<std::string> config_value(const Settings& settings, std::string_view key)
{
    if (const auto program = app_rule_program(key); !program.empty()) {
        if (const auto found = settings.app_rules.find(program); found != settings.app_rules.end()) {
            return found->second;
        }
        return std::nullopt;
    }
    if (key == "input.method") {
        return std::string(to_string(settings.options.method));
    }
    if (key == "input.tone_style") {
        return std::string(to_string(settings.options.tone_style));
    }
    if (key == "input.enabled") {
        return std::string(bool_text(settings.enabled));
    }
    if (key == "output.encoding") {
        return std::string(to_string(settings.options.encoding));
    }
    if (key == "engine.smart_backspace") {
        return std::string(bool_text(settings.options.smart_backspace));
    }
    if (key == "engine.english_fallback") {
        return std::string(bool_text(settings.options.english_fallback));
    }
    if (key == "engine.bracket_shortcuts") {
        return std::string(bool_text(settings.options.bracket_shortcuts));
    }
    if (key == "engine.candidates") {
        return std::string(bool_text(settings.options.candidates));
    }
    if (key == "engine.direct_typing" || key == "engine.no_preedit" || key == "direct_typing") {
        return std::string(bool_text(settings.options.direct_typing));
    }
    if (key == "engine.spell_check" || key == "spell_check") {
        return std::string(bool_text(settings.options.spell_check));
    }
    if (key == "engine.macro_enabled" || key == "macro_enabled" || key == "macro") {
        return std::string(bool_text(settings.options.macro_enabled));
    }
    if (key == "engine.capitalize_macro" || key == "capitalize_macro") {
        return std::string(bool_text(settings.options.capitalize_macro));
    }
    if (key == "engine.custom_dict" || key == "custom_dict") {
        return std::string(bool_text(settings.options.custom_dict));
    }
    if (key == "ui.toggle_key" || key == "toggle_key") {
        return settings.toggle_key;
    }
    if (key == "ui.show_tray_icon" || key == "show_tray_icon") {
        return std::string(bool_text(settings.show_tray_icon));
    }
    if (key == "ui.start_with_system" || key == "start_with_system") {
        return std::string(bool_text(settings.start_with_system));
    }
    return std::nullopt;
}

bool parse_bool_value(std::string_view text, bool& out)
{
    if (text == "true" || text == "yes" || text == "1" || text == "on") {
        out = true;
        return true;
    }
    if (text == "false" || text == "no" || text == "0" || text == "off") {
        out = false;
        return true;
    }
    return false;
}

bool apply_config(Settings& settings, std::string_view key, std::string_view value)
{
    if (const auto program = app_rule_program(key); !program.empty()) {
        // An empty value drops the rule, which is how a program gets the
        // default path back after NexKey learned a slower one for it.
        if (value.empty() || value == "auto" || value == "default") {
            settings.app_rules.erase(program);
            return true;
        }
        // The rule may name an input method, a delivery mode or both, so the
        // same parser the addon uses decides whether the value is usable.
        if (!nexkey::parse_app_rule(value).valid) {
            return false;
        }
        settings.app_rules[program] = std::string(value);
        return true;
    }
    if (key == "input.method" || key == "method") {
        const auto method = method_from_string(value);
        if (!method) {
            return false;
        }
        settings.options.method = *method;
        return true;
    }
    if (key == "input.tone_style" || key == "tone_style") {
        const auto style = tone_style_from_string(value);
        if (!style) {
            return false;
        }
        settings.options.tone_style = *style;
        return true;
    }
    if (key == "output.encoding" || key == "encoding") {
        const auto encoding = encoding_from_string(value);
        if (!encoding) {
            return false;
        }
        settings.options.encoding = *encoding;
        return true;
    }
    if (key == "ui.toggle_key" || key == "toggle_key") {
        settings.toggle_key = std::string(value);
        return true;
    }
    bool flag = false;
    if (!parse_bool_value(value, flag)) {
        return false;
    }
    if (key == "input.enabled" || key == "enabled") {
        settings.enabled = flag;
    } else if (key == "engine.smart_backspace" || key == "smart_backspace") {
        settings.options.smart_backspace = flag;
    } else if (key == "engine.english_fallback" || key == "english_fallback") {
        settings.options.english_fallback = flag;
    } else if (key == "engine.bracket_shortcuts" || key == "bracket_shortcuts") {
        settings.options.bracket_shortcuts = flag;
    } else if (key == "engine.candidates" || key == "candidates") {
        settings.options.candidates = flag;
    } else if (key == "engine.direct_typing" || key == "engine.no_preedit" || key == "direct_typing") {
        settings.options.direct_typing = flag;
    } else if (key == "engine.spell_check" || key == "spell_check") {
        settings.options.spell_check = flag;
    } else if (key == "engine.macro_enabled" || key == "macro_enabled" || key == "macro") {
        settings.options.macro_enabled = flag;
    } else if (key == "engine.capitalize_macro" || key == "capitalize_macro") {
        settings.options.capitalize_macro = flag;
    } else if (key == "engine.custom_dict" || key == "custom_dict") {
        settings.options.custom_dict = flag;
    } else if (key == "ui.show_tray_icon" || key == "show_tray_icon") {
        settings.show_tray_icon = flag;
    } else if (key == "ui.start_with_system" || key == "start_with_system") {
        settings.start_with_system = flag;
    } else {
        return false;
    }
    return true;
}

std::string compose_line(const std::string& line, const Options& options,
                         const std::vector<std::string>& custom_words, bool trace,
                         std::ostream& trace_out)
{
    Engine engine(options);
    engine.set_custom_words(custom_words);
    std::string out;
    std::size_t index = 0;
    for (const char key : line) {
        const auto step = engine.type(key);
        out += step.commit;
        ++index;
        if (trace) {
            trace_out << "  " << index << ": '" << key << "' -> [" << engine.preedit() << "]\n";
        }
    }
    out += engine.flush().commit;
    return out;
}

} // namespace

int run_test(const Args& args)
{
    const OptionParser parser(args, {"method", "encoding", "tone-style"});
    if (!parser.unknown().empty()) {
        std::cerr << "nexkey: unknown option " << parser.unknown().front() << '\n';
        return 2;
    }

    const auto settings = load_settings();
    auto options = settings.options;
    if (const auto value = parser.value("method")) {
        const auto method = method_from_string(*value);
        if (!method) {
            std::cerr << "nexkey: unknown input method " << *value << '\n';
            return 2;
        }
        options.method = *method;
    }
    if (const auto value = parser.value("encoding")) {
        const auto encoding = encoding_from_string(*value);
        if (!encoding) {
            std::cerr << "nexkey: unknown character set " << *value << '\n';
            return 2;
        }
        options.encoding = *encoding;
    }
    if (const auto value = parser.value("tone-style")) {
        const auto style = tone_style_from_string(*value);
        if (!style) {
            std::cerr << "nexkey: unknown tone style " << *value << '\n';
            return 2;
        }
        options.tone_style = *style;
    }
    if (parser.has("no-fallback")) {
        options.english_fallback = false;
    }
    if (parser.has("no-brackets")) {
        options.bracket_shortcuts = false;
    }
    if (parser.has("no-smart-backspace")) {
        options.smart_backspace = false;
    }
    if (parser.has("no-candidates")) {
        options.candidates = false;
    }

    const bool trace = parser.has("trace");
    Args lines;
    if (!parser.positional().empty()) {
        lines.push_back(join(parser.positional()));
    } else {
        std::istringstream input(read_stream(std::cin));
        std::string line;
        while (std::getline(input, line)) {
            lines.push_back(line);
        }
    }

    const bool show_candidates = parser.has("candidates");
    for (const auto& line : lines) {
        std::cout << compose_line(line, options, settings.custom_dictionary, trace, std::cout)
                  << '\n';
        if (show_candidates) {
            Engine engine(options);
            engine.set_custom_words(settings.custom_dictionary);
            for (const char key : line) {
                static_cast<void>(engine.type(key));
            }
            const auto candidates = engine.candidates();
            for (std::size_t index = 0; index < candidates.size(); ++index) {
                std::cout << "  [" << index + 1 << "] " << candidates[index].text;
                if (!candidates[index].comment.empty()) {
                    std::cout << "  (" << candidates[index].comment << ')';
                }
                std::cout << '\n';
            }
        }
    }
    return 0;
}

int run_convert(const Args& args)
{
    const OptionParser parser(args, {"from", "to", "input", "output"});
    if (!parser.unknown().empty()) {
        std::cerr << "nexkey: unknown option " << parser.unknown().front() << '\n';
        return 2;
    }

    const auto from_name = parser.value("from");
    const auto to_name = parser.value("to");
    if (!from_name || !to_name) {
        std::cerr << "nexkey: convert needs --from and --to\n";
        return 2;
    }
    const auto from = encoding_from_string(*from_name);
    const auto to = encoding_from_string(*to_name);
    if (!from || !to) {
        std::cerr << "nexkey: unknown character set\n";
        return 2;
    }

    std::string input;
    if (const auto path = parser.value("input")) {
        std::ifstream file(*path, std::ios::binary);
        if (!file) {
            std::cerr << "nexkey: cannot read " << *path << '\n';
            return 1;
        }
        input = read_stream(file);
    } else {
        input = read_stream(std::cin);
    }

    const auto converted = Converter::convert(input, *from, *to);
    if (const auto path = parser.value("output")) {
        std::ofstream file(*path, std::ios::binary);
        if (!file) {
            std::cerr << "nexkey: cannot write " << *path << '\n';
            return 1;
        }
        file << converted;
        return file.good() ? 0 : 1;
    }
    std::cout << converted;
    return 0;
}

int run_config(const Args& args)
{
    const OptionParser parser(args, {});
    const auto& positional = parser.positional();
    const auto command = positional.empty() ? std::string("show") : positional.front();

    if (command == "path") {
        std::cout << default_config_path().string() << '\n';
        return 0;
    }

    if (command == "show") {
        print_settings(std::cout, load_settings());
        return 0;
    }

    if (command == "get") {
        if (positional.size() < 2) {
            std::cerr << "nexkey: config get needs a key\n";
            return 2;
        }
        const auto value = config_value(load_settings(), positional[1]);
        if (!value) {
            std::cerr << "nexkey: unknown key " << positional[1] << '\n';
            return 2;
        }
        std::cout << *value << '\n';
        return 0;
    }

    if (command == "set") {
        if (positional.size() < 3) {
            std::cerr << "nexkey: config set needs a key and a value\n";
            return 2;
        }
        auto settings = load_settings();
        if (!apply_config(settings, positional[1], positional[2])) {
            std::cerr << "nexkey: invalid value for " << positional[1] << '\n';
            return 2;
        }
        if (!save_settings(settings)) {
            std::cerr << "nexkey: cannot write " << default_config_path().string() << '\n';
            return 1;
        }
        std::cout << positional[1] << '=';
        if (const auto updated = config_value(settings, positional[1]); updated.has_value()) {
            std::cout << *updated;
        }
        std::cout << '\n';
        return 0;
    }

    std::cerr << "nexkey: unknown config command " << command << '\n';
    return 2;
}

int run_status(const Args& args)
{
    static_cast<void>(args);
    print_settings(std::cout, load_settings());
    return 0;
}

int run_toggle(const Args& args)
{
    static_cast<void>(args);
    auto settings = load_settings();
    settings.enabled = !settings.enabled;
    if (!save_settings(settings)) {
        std::cerr << "nexkey: cannot write " << default_config_path().string() << '\n';
        return 1;
    }
    std::cout << "mode=" << (settings.enabled ? "VI" : "EN") << '\n';
    return 0;
}

int run_macro(const Args& args)
{
    const OptionParser parser(args, {});
    const auto& positional = parser.positional();
    const auto command = positional.empty() ? std::string("list") : positional.front();

    if (command == "path") {
        std::cout << default_macros_path().string() << '\n';
        return 0;
    }

    auto macros = load_macros();

    if (command == "list") {
        std::vector<std::pair<std::string, std::string>> sorted(macros.begin(), macros.end());
        std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [k, v] : sorted) {
            std::cout << k << '=' << v << '\n';
        }
        return 0;
    }

    if (command == "get") {
        if (positional.size() < 2) {
            std::cerr << "nexkey: macro get needs a key\n";
            return 2;
        }
        if (auto it = macros.find(positional[1]); it != macros.end()) {
            std::cout << it->second << '\n';
            return 0;
        }
        std::cerr << "nexkey: macro not found: " << positional[1] << '\n';
        return 1;
    }

    if (command == "set") {
        if (positional.size() < 3) {
            std::cerr << "nexkey: macro set needs a key and a value\n";
            return 2;
        }
        macros[positional[1]] = positional[2];
        if (!save_macros(macros)) {
            std::cerr << "nexkey: cannot write " << default_macros_path().string() << '\n';
            return 1;
        }
        std::cout << positional[1] << '=' << positional[2] << '\n';
        return 0;
    }

    if (command == "delete" || command == "del" || command == "rm") {
        if (positional.size() < 2) {
            std::cerr << "nexkey: macro delete needs a key\n";
            return 2;
        }
        if (macros.erase(positional[1]) > 0) {
            if (!save_macros(macros)) {
                std::cerr << "nexkey: cannot write " << default_macros_path().string() << '\n';
                return 1;
            }
            std::cout << "deleted: " << positional[1] << '\n';
            return 0;
        }
        std::cerr << "nexkey: macro not found: " << positional[1] << '\n';
        return 1;
    }

    std::cerr << "nexkey: unknown macro command " << command << '\n';
    return 2;
}

int run_emoji(const Args& args)
{
    const OptionParser parser(args, {});
    const auto& positional = parser.positional();
    const std::string query = positional.empty() ? std::string{} : positional.front();

    const auto macros = load_macros();
    std::vector<std::pair<std::string, std::string>> matches;
    for (const auto& [k, v] : macros) {
        if (k.starts_with(":") || k == "<3" || k == "(y)" || k.starts_with(";")) {
            if (query.empty() || k.find(query) != std::string::npos || v.find(query) != std::string::npos) {
                matches.emplace_back(k, v);
            }
        }
    }
    std::ranges::sort(matches, [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [k, v] : matches) {
        std::cout << v << "  " << k << '\n';
    }
    return 0;
}

} // namespace nexkey::cli

