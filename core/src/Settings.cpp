// SPDX-License-Identifier: MIT
// NexKey - configuration file handling.

#include "nexkey/Settings.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

namespace nexkey {

namespace {

std::string_view trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string to_lower(std::string_view value)
{
    std::string out(value);
    std::ranges::transform(out, out.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return out;
}

bool parse_bool(std::string_view value, bool fallback)
{
    const auto lower = to_lower(trim(value));
    if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") {
        return true;
    }
    if (lower == "false" || lower == "no" || lower == "off" || lower == "0") {
        return false;
    }
    return fallback;
}

std::string unquote(std::string_view value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return std::string(value.substr(1, value.size() - 2));
    }
    return std::string(value);
}

} // namespace

std::filesystem::path default_config_path()
{
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "nexkey" / "nexkey.conf";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".config" / "nexkey" / "nexkey.conf";
    }
    return std::filesystem::path("nexkey.conf");
}

Settings load_settings(const std::filesystem::path& path)
{
    Settings settings;
    settings.macros = load_macros();
    std::ifstream file(path);
    if (!file) {
        return settings;
    }

    std::string section;
    std::string line;
    while (std::getline(file, line)) {
        const auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            continue;
        }
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            section = to_lower(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }
        const auto separator = trimmed.find('=');
        if (separator == std::string_view::npos) {
            continue;
        }
        const auto key = to_lower(trim(trimmed.substr(0, separator)));
        const auto value = trim(trimmed.substr(separator + 1));

        if (section == "input") {
            if (key == "method") {
                if (const auto method = method_from_string(to_lower(value))) {
                    settings.options.method = *method;
                }
            } else if (key == "tone_style") {
                if (const auto style = tone_style_from_string(to_lower(value))) {
                    settings.options.tone_style = *style;
                }
            } else if (key == "enabled") {
                settings.enabled = parse_bool(value, settings.enabled);
            }
        } else if (section == "output") {
            if (key == "encoding") {
                if (const auto encoding = encoding_from_string(to_lower(value))) {
                    settings.options.encoding = *encoding;
                }
            }
        } else if (section == "engine") {
            if (key == "smart_backspace") {
                settings.options.smart_backspace = parse_bool(value, settings.options.smart_backspace);
            } else if (key == "english_fallback") {
                settings.options.english_fallback =
                    parse_bool(value, settings.options.english_fallback);
            } else if (key == "bracket_shortcuts") {
                settings.options.bracket_shortcuts =
                    parse_bool(value, settings.options.bracket_shortcuts);
            } else if (key == "candidates") {
                settings.options.candidates = parse_bool(value, settings.options.candidates);
            } else if (key == "direct_typing" || key == "no_preedit") {
                settings.options.direct_typing =
                    parse_bool(value, settings.options.direct_typing);
            } else if (key == "spell_check") {
                settings.options.spell_check = parse_bool(value, settings.options.spell_check);
            } else if (key == "macro_enabled" || key == "macro") {
                settings.options.macro_enabled = parse_bool(value, settings.options.macro_enabled);
            } else if (key == "capitalize_macro") {
                settings.options.capitalize_macro = parse_bool(value, settings.options.capitalize_macro);
            } else if (key == "custom_dict" || key == "custom_dictionary") {
                settings.options.custom_dict = parse_bool(value, settings.options.custom_dict);
            }
        } else if (section == "macro") {
            if (key == "time_format") {
                settings.time_format = unquote(value);
            } else if (key == "date_format") {
                settings.date_format = unquote(value);
            }
        } else if (section == "keymap") {
            if (key == "preset") {
                settings.keymap_preset = unquote(value);
            }
        } else if (section == "app_rules") {
            settings.app_rules[std::string(key)] = unquote(value);
        } else if (section == "custom_dictionary") {
            const auto word = unquote(key.empty() ? value : key);
            if (!word.empty()) {
                settings.custom_dictionary.push_back(std::string(word));
            }
        } else if (section == "ui") {
            if (key == "toggle_key") {
                settings.toggle_key = unquote(value);
            } else if (key == "show_tray_icon") {
                settings.show_tray_icon = parse_bool(value, settings.show_tray_icon);
            } else if (key == "start_with_system") {
                settings.start_with_system = parse_bool(value, settings.start_with_system);
            }
        }
    }
    return settings;
}

Settings load_settings()
{
    return load_settings(default_config_path());
}

bool save_settings(const Settings& settings, const std::filesystem::path& path)
{
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }

    file << "# NexKey configuration file.\n"
         << "# Edit with the tray application or with 'nexkey config set'.\n"
         << "\n[input]\n"
         << "method=" << to_string(settings.options.method) << '\n'
         << "tone_style=" << to_string(settings.options.tone_style) << '\n'
         << "enabled=" << (settings.enabled ? "true" : "false") << '\n'
         << "\n[output]\n"
         << "encoding=" << to_string(settings.options.encoding) << '\n'
         << "\n[engine]\n"
         << "smart_backspace=" << (settings.options.smart_backspace ? "true" : "false") << '\n'
         << "english_fallback=" << (settings.options.english_fallback ? "true" : "false") << '\n'
         << "bracket_shortcuts=" << (settings.options.bracket_shortcuts ? "true" : "false") << '\n'
         << "candidates=" << (settings.options.candidates ? "true" : "false") << '\n'
         << "direct_typing=" << (settings.options.direct_typing ? "true" : "false") << '\n'
         << "spell_check=" << (settings.options.spell_check ? "true" : "false") << '\n'
         << "macro_enabled=" << (settings.options.macro_enabled ? "true" : "false") << '\n'
         << "capitalize_macro=" << (settings.options.capitalize_macro ? "true" : "false") << '\n'
         << "custom_dict=" << (settings.options.custom_dict ? "true" : "false") << '\n'
         << "\n[macro]\n"
         << "time_format=" << settings.time_format << '\n'
         << "date_format=" << settings.date_format << '\n'
         << "\n[keymap]\n"
         << "preset=" << settings.keymap_preset << '\n'
         << "\n[ui]\n"
         << "toggle_key=" << settings.toggle_key << '\n'
         << "show_tray_icon=" << (settings.show_tray_icon ? "true" : "false") << '\n'
         << "start_with_system=" << (settings.start_with_system ? "true" : "false") << '\n';

    if (!settings.app_rules.empty()) {
        file << "\n[app_rules]\n";
        for (const auto& [app, mode] : settings.app_rules) {
            file << app << '=' << mode << '\n';
        }
    }

    if (!settings.custom_dictionary.empty()) {
        file << "\n[custom_dictionary]\n";
        for (const auto& word : settings.custom_dictionary) {
            file << word << "=1\n";
        }
    }
    return file.good();
}

bool save_settings(const Settings& settings)
{
    return save_settings(settings, default_config_path());
}

std::filesystem::path default_macros_path()
{
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path(xdg) / "nexkey" / "macros.conf";
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".config" / "nexkey" / "macros.conf";
    }
    return std::filesystem::path("macros.conf");
}

std::unordered_map<std::string, std::string> load_macros(const std::filesystem::path& path)
{
    std::unordered_map<std::string, std::string> macros;
    std::ifstream file(path);
    if (!file) {
        return {
            {"vn", "Việt Nam"},
            {"hn", "Hà Nội"},
            {"hcm", "TP. Hồ Chí Minh"},
            {"dc", "được"},
            {"ko", "không"},
            {"k", "không"},
            {"ng", "người"},
            {"vs", "với"},
            {"bt", "bình thường"},
            {":)", "😊"},
            {":D", "😃"},
            {"<3", "❤️"},
            {"(y)", "👍"},
            {":heart:", "❤️"},
            {":like:", "👍"},
            {":fire:", "🔥"},
            {":star:", "⭐"},
            {":check:", "✅"},
            {":party:", "🎉"},
            {":cry:", "😭"},
            {":wink:", "😉"}
        };
    }
    std::string line;
    while (std::getline(file, line)) {
        const auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            continue;
        }
        const auto sep = trimmed.find('=');
        if (sep == std::string_view::npos) {
            continue;
        }
        auto key = std::string(trim(trimmed.substr(0, sep)));
        auto val = unquote(trim(trimmed.substr(sep + 1)));
        if (!key.empty() && !val.empty()) {
            macros[std::move(key)] = std::move(val);
        }
    }
    return macros;
}

std::unordered_map<std::string, std::string> load_macros()
{
    return load_macros(default_macros_path());
}

bool save_macros(const std::unordered_map<std::string, std::string>& macros, const std::filesystem::path& path)
{
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }
    file << "# NexKey macro and emoji table.\n"
         << "# Format: shortcut=replacement\n\n";
    for (const auto& [k, v] : macros) {
        file << k << '=' << v << '\n';
    }
    return file.good();
}

bool save_macros(const std::unordered_map<std::string, std::string>& macros)
{
    return save_macros(macros, default_macros_path());
}

namespace {

std::string escape_json(std::string_view str)
{
    std::string out;
    out.reserve(str.size() + 8);
    for (char c : str) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
            } else {
                out += c;
            }
            break;
        }
    }
    return out;
}

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : m_s(input) {}

    bool parse_backup(Settings& out)
    {
        skip_ws();
        if (peek() != '{') return false;
        get();
        while (has_more()) {
            skip_ws();
            if (peek() == '}') { get(); return true; }
            auto key = parse_string();
            skip_colon();
            if (key == "main_settings") {
                parse_main_settings(out);
            } else if (key == "macros") {
                parse_macros(out);
            } else if (key == "custom_keymaps") {
                parse_custom_keymaps(out);
            } else if (key == "app_rules") {
                parse_app_rules(out);
            } else if (key == "custom_dictionary") {
                parse_custom_dictionary(out);
            } else {
                skip_value();
            }
            skip_comma();
        }
        return true;
    }

private:
    std::string_view m_s;
    std::size_t m_pos{0};

    bool has_more() const { return m_pos < m_s.size(); }
    char peek() const { return has_more() ? m_s[m_pos] : '\0'; }
    char get() { return has_more() ? m_s[m_pos++] : '\0'; }

    void skip_ws()
    {
        while (has_more() && (std::isspace(static_cast<unsigned char>(m_s[m_pos])))) {
            ++m_pos;
        }
    }

    void skip_colon()
    {
        skip_ws();
        if (peek() == ':') get();
        skip_ws();
    }

    void skip_comma()
    {
        skip_ws();
        if (peek() == ',') get();
        skip_ws();
    }

    std::string parse_string()
    {
        skip_ws();
        if (peek() != '"') return {};
        get();
        std::string res;
        while (has_more()) {
            char c = get();
            if (c == '"') break;
            if (c == '\\' && has_more()) {
                char esc = get();
                switch (esc) {
                case '"': res += '"'; break;
                case '\\': res += '\\'; break;
                case 'n': res += '\n'; break;
                case 'r': res += '\r'; break;
                case 't': res += '\t'; break;
                default: res += esc; break;
                }
            } else {
                res += c;
            }
        }
        return res;
    }

    bool parse_bool()
    {
        skip_ws();
        if (m_s.substr(m_pos, 4) == "true") {
            m_pos += 4;
            return true;
        }
        if (m_s.substr(m_pos, 5) == "false") {
            m_pos += 5;
            return false;
        }
        return false;
    }

    void skip_value()
    {
        skip_ws();
        if (peek() == '"') {
            parse_string();
        } else if (peek() == '{') {
            get();
            int depth = 1;
            while (has_more() && depth > 0) {
                char c = get();
                if (c == '"') {
                    while (has_more()) {
                        char sc = get();
                        if (sc == '"') break;
                        if (sc == '\\' && has_more()) get();
                    }
                } else if (c == '{') ++depth;
                else if (c == '}') --depth;
            }
        } else if (peek() == '[') {
            get();
            int depth = 1;
            while (has_more() && depth > 0) {
                char c = get();
                if (c == '"') {
                    while (has_more()) {
                        char sc = get();
                        if (sc == '"') break;
                        if (sc == '\\' && has_more()) get();
                    }
                } else if (c == '[') ++depth;
                else if (c == ']') --depth;
            }
        } else {
            while (has_more() && peek() != ',' && peek() != '}' && peek() != ']') {
                get();
            }
        }
    }

    void parse_main_settings(Settings& out)
    {
        skip_ws();
        if (peek() != '{') return;
        get();
        while (has_more()) {
            skip_ws();
            if (peek() == '}') { get(); break; }
            auto k = parse_string();
            skip_colon();
            if (k == "method") {
                if (auto m = method_from_string(parse_string())) out.options.method = *m;
            } else if (k == "tone_style") {
                if (auto t = tone_style_from_string(parse_string())) out.options.tone_style = *t;
            } else if (k == "encoding") {
                if (auto e = encoding_from_string(parse_string())) out.options.encoding = *e;
            } else if (k == "enabled") {
                out.enabled = parse_bool();
            } else if (k == "smart_backspace") {
                out.options.smart_backspace = parse_bool();
            } else if (k == "english_fallback") {
                out.options.english_fallback = parse_bool();
            } else if (k == "bracket_shortcuts") {
                out.options.bracket_shortcuts = parse_bool();
            } else if (k == "candidates") {
                out.options.candidates = parse_bool();
            } else if (k == "direct_typing") {
                out.options.direct_typing = parse_bool();
            } else if (k == "spell_check") {
                out.options.spell_check = parse_bool();
            } else if (k == "macro_enabled") {
                out.options.macro_enabled = parse_bool();
            } else if (k == "capitalize_macro") {
                out.options.capitalize_macro = parse_bool();
            } else if (k == "custom_dict") {
                out.options.custom_dict = parse_bool();
            } else if (k == "toggle_key") {
                out.toggle_key = parse_string();
            } else if (k == "show_tray_icon") {
                out.show_tray_icon = parse_bool();
            } else if (k == "start_with_system") {
                out.start_with_system = parse_bool();
            } else if (k == "time_format") {
                out.time_format = parse_string();
            } else if (k == "date_format") {
                out.date_format = parse_string();
            } else {
                skip_value();
            }
            skip_comma();
        }
    }

    void parse_macros(Settings& out)
    {
        skip_ws();
        if (peek() != '{') return;
        get();
        while (has_more()) {
            skip_ws();
            if (peek() == '}') { get(); break; }
            auto k = parse_string();
            skip_colon();
            auto v = parse_string();
            if (!k.empty() && !v.empty()) {
                out.macros[std::move(k)] = std::move(v);
            }
            skip_comma();
        }
    }

    void parse_custom_keymaps(Settings& out)
    {
        skip_ws();
        if (peek() != '{') return;
        get();
        while (has_more()) {
            skip_ws();
            if (peek() == '}') { get(); break; }
            auto k = parse_string();
            skip_colon();
            if (k == "preset") {
                out.keymap_preset = parse_string();
            } else {
                skip_value();
            }
            skip_comma();
        }
    }

    void parse_app_rules(Settings& out)
    {
        skip_ws();
        if (peek() != '{') return;
        get();
        while (has_more()) {
            skip_ws();
            if (peek() == '}') { get(); break; }
            auto k = parse_string();
            skip_colon();
            auto v = parse_string();
            if (!k.empty() && !v.empty()) {
                out.app_rules[std::move(k)] = std::move(v);
            }
            skip_comma();
        }
    }

    void parse_custom_dictionary(Settings& out)
    {
        skip_ws();
        if (peek() != '[') return;
        get();
        while (has_more()) {
            skip_ws();
            if (peek() == ']') { get(); break; }
            auto word = parse_string();
            if (!word.empty()) {
                out.custom_dictionary.push_back(std::move(word));
            }
            skip_comma();
        }
    }
};

} // namespace

std::string export_backup_json(const Settings& settings)
{
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": \"" << kVersion << "\",\n";

    // 1. Main Settings
    ss << "  \"main_settings\": {\n";
    ss << "    \"method\": \"" << to_string(settings.options.method) << "\",\n";
    ss << "    \"tone_style\": \"" << to_string(settings.options.tone_style) << "\",\n";
    ss << "    \"encoding\": \"" << to_string(settings.options.encoding) << "\",\n";
    ss << "    \"enabled\": " << (settings.enabled ? "true" : "false") << ",\n";
    ss << "    \"smart_backspace\": " << (settings.options.smart_backspace ? "true" : "false") << ",\n";
    ss << "    \"english_fallback\": " << (settings.options.english_fallback ? "true" : "false") << ",\n";
    ss << "    \"bracket_shortcuts\": " << (settings.options.bracket_shortcuts ? "true" : "false") << ",\n";
    ss << "    \"candidates\": " << (settings.options.candidates ? "true" : "false") << ",\n";
    ss << "    \"direct_typing\": " << (settings.options.direct_typing ? "true" : "false") << ",\n";
    ss << "    \"spell_check\": " << (settings.options.spell_check ? "true" : "false") << ",\n";
    ss << "    \"macro_enabled\": " << (settings.options.macro_enabled ? "true" : "false") << ",\n";
    ss << "    \"capitalize_macro\": " << (settings.options.capitalize_macro ? "true" : "false") << ",\n";
    ss << "    \"custom_dict\": " << (settings.options.custom_dict ? "true" : "false") << ",\n";
    ss << "    \"toggle_key\": \"" << escape_json(settings.toggle_key) << "\",\n";
    ss << "    \"show_tray_icon\": " << (settings.show_tray_icon ? "true" : "false") << ",\n";
    ss << "    \"start_with_system\": " << (settings.start_with_system ? "true" : "false") << ",\n";
    ss << "    \"time_format\": \"" << escape_json(settings.time_format) << "\",\n";
    ss << "    \"date_format\": \"" << escape_json(settings.date_format) << "\"\n";
    ss << "  },\n";

    // 2. Macros
    ss << "  \"macros\": {\n";
    bool first_macro = true;
    for (const auto& [k, v] : settings.macros) {
        if (!first_macro) ss << ",\n";
        first_macro = false;
        ss << "    \"" << escape_json(k) << "\": \"" << escape_json(v) << "\"";
    }
    ss << "\n  },\n";

    // 3. Custom Keymaps / Presets
    ss << "  \"custom_keymaps\": {\n";
    ss << "    \"preset\": \"" << escape_json(settings.keymap_preset) << "\"\n";
    ss << "  },\n";

    // 4. App Rules
    ss << "  \"app_rules\": {\n";
    bool first_rule = true;
    for (const auto& [app, mode] : settings.app_rules) {
        if (!first_rule) ss << ",\n";
        first_rule = false;
        ss << "    \"" << escape_json(app) << "\": \"" << escape_json(mode) << "\"";
    }
    ss << "\n  },\n";

    // 5. Custom Dictionary
    ss << "  \"custom_dictionary\": [\n";
    for (std::size_t i = 0; i < settings.custom_dictionary.size(); ++i) {
        ss << "    \"" << escape_json(settings.custom_dictionary[i]) << "\"";
        if (i + 1 < settings.custom_dictionary.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";

    return ss.str();
}

bool export_backup_file(const std::filesystem::path& path, const Settings& settings)
{
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        return false;
    }
    file << export_backup_json(settings);
    return file.good();
}

bool import_backup_file(const std::filesystem::path& path, Settings& out_settings)
{
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    JsonParser parser(content);
    return parser.parse_backup(out_settings);
}

} // namespace nexkey
