// SPDX-License-Identifier: MIT
// Runtime status file used by the diagnostics page.

#include "TestHarness.hpp"

#include "nexkey/Status.hpp"

#include <filesystem>

TEST(status_roundtrip)
{
    const auto directory = std::filesystem::temp_directory_path() / "nexkey-tests-status";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    const auto path = directory / "status.conf";

    nexkey::RuntimeStatus status;
    status.program = "code";
    status.mode = "preedit";
    status.reason = "tự học: xóa lỗi";
    status.surrounding_capability = true;
    status.surrounding_valid = false;
    status.recent_mismatches = 2;
    status.addon_version = "2.0.0";
    status.addon_build = std::string(nexkey::build_id());
    status.updated = 1730000000;
    status.last_learned = "code";
    status.last_learned_at = 1730000001;
    EXPECT_TRUE(nexkey::write_status(status, path));

    nexkey::RuntimeStatus loaded;
    EXPECT_TRUE(nexkey::read_status(path, loaded));
    EXPECT_EQ(loaded.program, std::string("code"));
    EXPECT_EQ(loaded.mode, std::string("preedit"));
    EXPECT_EQ(loaded.reason, std::string("tự học: xóa lỗi"));
    EXPECT_TRUE(loaded.surrounding_capability);
    EXPECT_FALSE(loaded.surrounding_valid);
    EXPECT_EQ(loaded.recent_mismatches, 2);
    EXPECT_EQ(loaded.addon_build, status.addon_build);
    EXPECT_EQ(loaded.updated, static_cast<std::int64_t>(1730000000));
    EXPECT_EQ(loaded.last_learned, std::string("code"));

    std::filesystem::remove_all(directory, error);
}

TEST(status_missing_file)
{
    nexkey::RuntimeStatus loaded;
    EXPECT_FALSE(nexkey::read_status(
        std::filesystem::temp_directory_path() / "nexkey-tests-nothing" / "status.conf", loaded));
}
