// SPDX-License-Identifier: MIT
// Test runner.

#include "TestHarness.hpp"

#include <iostream>

int main()
{
    int failed_cases = 0;
    for (const auto& test : testing::registry()) {
        const int before = testing::failures();
        test.function();
        if (testing::failures() == before) {
            std::cout << "ok   " << test.name << '\n';
        } else {
            ++failed_cases;
            std::cout << "FAIL " << test.name << '\n';
        }
    }

    std::cout << '\n'
              << testing::registry().size() << " cases, " << failed_cases << " failed, "
              << testing::failures() << " failed checks\n";
    return testing::failures() == 0 ? 0 : 1;
}
