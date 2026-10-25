// SPDX-License-Identifier: MIT
// Configuration file handling.

#include "TestHarness.hpp"

#include "nexkey/Settings.hpp"

#include <filesystem>
#include <fstream>

namespace {

class TempFile {
public:
    explicit TempFile(const char* name)
        : m_path(std::filesystem::temp_directory_path() / "nexkey-tests" / name)
    {
        std::error_code error;
        std::filesystem::remove_all(m_path.parent_path(), error);
    }

    ~TempFile()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path.parent_path(), error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

} // namespace

TEST(settings_defaults)
{
    const TempFile file{"missing/nexkey.conf"};
    const auto settings = nexkey::load_settings(file.path());
    EXPECT_EQ(settings.options.method, nexkey::Method::Telex);
    EXPECT_EQ(settings.options.encoding, nexkey::Encoding::Unicode);
    EXPECT_EQ(settings.options.tone_style, nexkey::ToneStyle::Modern);
    EXPECT_TRUE(settings.options.smart_backspace);
    EXPECT_TRUE(settings.options.english_fallback);
    EXPECT_TRUE(settings.enabled);
    EXPECT_EQ(settings.toggle_key, std::string("Control+Shift"));
}

TEST(settings_round_trip)
{
    const TempFile file{"round-trip/nexkey.conf"};

    nexkey::Settings settings;
    settings.options.method = nexkey::Method::Vni;
    settings.options.encoding = nexkey::Encoding::Tcvn3;
    settings.options.tone_style = nexkey::ToneStyle::Traditional;
    settings.options.smart_backspace = false;
    settings.options.english_fallback = false;
    settings.options.bracket_shortcuts = false;
    settings.enabled = false;
    settings.toggle_key = "Control+space";
    settings.show_tray_icon = false;
    settings.start_with_system = true;

    EXPECT_TRUE(nexkey::save_settings(settings, file.path()));

    const auto loaded = nexkey::load_settings(file.path());
    EXPECT_EQ(loaded.options.method, nexkey::Method::Vni);
    EXPECT_EQ(loaded.options.encoding, nexkey::Encoding::Tcvn3);
    EXPECT_EQ(loaded.options.tone_style, nexkey::ToneStyle::Traditional);
    EXPECT_FALSE(loaded.options.smart_backspace);
    EXPECT_FALSE(loaded.options.english_fallback);
    EXPECT_FALSE(loaded.options.bracket_shortcuts);
    EXPECT_FALSE(loaded.enabled);
    EXPECT_EQ(loaded.toggle_key, std::string("Control+space"));
    EXPECT_FALSE(loaded.show_tray_icon);
    EXPECT_TRUE(loaded.start_with_system);
}

TEST(settings_ignores_invalid_values)
{
    const TempFile file{"invalid/nexkey.conf"};
    std::filesystem::create_directories(file.path().parent_path());
    {
        std::ofstream out(file.path());
        out << "# comment\n"
            << "[input]\n"
            << "method=unknown\n"
            << "tone_style=old\n"
            << "enabled=maybe\n"
            << "[output]\n"
            << "encoding=abc\n"
            << "garbage line without separator\n";
    }

    const auto settings = nexkey::load_settings(file.path());
    EXPECT_EQ(settings.options.method, nexkey::Method::Telex);
    EXPECT_EQ(settings.options.tone_style, nexkey::ToneStyle::Traditional);
    EXPECT_EQ(settings.options.encoding, nexkey::Encoding::Tcvn3);
    EXPECT_TRUE(settings.enabled);
}

TEST(settings_default_path_is_under_xdg)
{
    const auto path = nexkey::default_config_path();
    EXPECT_FALSE(path.empty());
    EXPECT_EQ(path.filename(), std::filesystem::path("nexkey.conf"));
}
