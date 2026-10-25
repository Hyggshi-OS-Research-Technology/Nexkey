// SPDX-License-Identifier: MIT
// NexKey command line interface.

#pragma once

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nexkey::cli {

using Args = std::vector<std::string>;

void print_help(std::ostream& out);
void print_version(std::ostream& out);
void print_methods(std::ostream& out);
void print_encodings(std::ostream& out);

int run_test(const Args& args);
int run_convert(const Args& args);
int run_config(const Args& args);
int run_status(const Args& args);
int run_toggle(const Args& args);

/// Parse "--name value" and "--name=value" options.
class OptionParser {
public:
    /// `value_options` lists the options that take a value, so that flags
    /// without a value do not swallow the following argument.
    explicit OptionParser(const Args& args, std::initializer_list<std::string_view> value_options);

    /// Returns the value of an option, or std::nullopt when it is absent.
    [[nodiscard]] std::optional<std::string> value(std::string_view name) const;
    [[nodiscard]] bool has(std::string_view name) const;
    /// Positional arguments, in order.
    [[nodiscard]] const Args& positional() const { return m_positional; }
    /// Options that were not recognised.
    [[nodiscard]] const Args& unknown() const { return m_unknown; }

private:
    Args m_positional;
    Args m_unknown;
    std::vector<std::pair<std::string, std::string>> m_options;
};

} // namespace nexkey::cli
