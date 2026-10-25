// SPDX-License-Identifier: MIT
// Minimal dependency free test harness.

#pragma once

#include <iostream>
#include <optional>
#include <sstream>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace testing {

struct Case {
    std::string name;
    void (*function)();
};

inline std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

inline int& failures()
{
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, void (*function)())
    {
        registry().push_back(Case{name, function});
    }
};

inline std::string describe_string(std::string_view value)
{
    std::ostringstream stream;
    stream << '"';
    for (const char raw : value) {
        const auto character = static_cast<unsigned char>(raw);
        if (character >= 0x20U && character < 0x7FU) {
            stream << static_cast<char>(character);
        } else {
            stream << "\\x" << std::hex << static_cast<int>(character) << std::dec;
        }
    }
    stream << '"';
    return stream.str();
}

template <typename T>
std::string describe(const T& value)
{
    if constexpr (std::is_same_v<T, std::string>) {
        return describe_string(value);
    } else if constexpr (std::is_same_v<T, std::string_view>) {
        return describe_string(value);
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
        return "nullptr";
    } else if constexpr (std::is_enum_v<T>) {
        std::ostringstream stream;
        stream << "enum(" << static_cast<long long>(value) << ")";
        return stream.str();
    } else if constexpr (std::is_same_v<T, char32_t>) {
        std::ostringstream stream;
        stream << "U+" << std::hex << static_cast<unsigned long>(value);
        return stream.str();
    } else if constexpr (requires(std::ostream& stream, const T& item) { stream << item; }) {
        std::ostringstream stream;
        stream << value;
        return stream.str();
    } else {
        return "<value>";
    }
}

template <typename T>
std::string describe(const std::optional<T>& value)
{
    return value.has_value() ? "some(" + describe(*value) + ")" : std::string("none");
}

inline void fail(const std::source_location& location, const std::string& message)
{
    ++failures();
    std::cerr << location.file_name() << ':' << location.line() << ": " << message << '\n';
}

} // namespace testing

#define TEST(name)                                                             \
    static void name();                                                        \
    static ::testing::Registrar registrar_##name(#name, &name);                \
    static void name()

#define EXPECT_TRUE(condition)                                                 \
    do {                                                                       \
        if (!(condition)) {                                                    \
            ::testing::fail(std::source_location::current(),                   \
                            "expected true: " #condition);                     \
        }                                                                      \
    } while (false)

#define EXPECT_FALSE(condition)                                                \
    do {                                                                       \
        if ((condition)) {                                                     \
            ::testing::fail(std::source_location::current(),                   \
                            "expected false: " #condition);                    \
        }                                                                      \
    } while (false)

#define EXPECT_EQ(actual, expected)                                            \
    do {                                                                       \
        const auto& actual_value = (actual);                                    \
        const auto& expected_value = (expected);                                \
        if (!(actual_value == expected_value)) {                                \
            ::testing::fail(std::source_location::current(),                    \
                            #actual " == " #expected "\n  actual:   " +         \
                                ::testing::describe(actual_value) +              \
                                "\n  expected: " +                              \
                                ::testing::describe(expected_value));            \
        }                                                                      \
    } while (false)
