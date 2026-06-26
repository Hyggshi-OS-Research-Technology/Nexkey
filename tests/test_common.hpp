#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

inline int g_failures = 0;

#define EXPECT_EQ(actual, expected) \
    do { \
        const auto actual_value = (actual); \
        const auto expected_value = (expected); \
        if (actual_value != expected_value) { \
            ++g_failures; \
            std::cerr << __FILE__ << ":" << __LINE__ << " expected [" << expected_value \
                      << "] got [" << actual_value << "]\n"; \
        } \
    } while (false)

#define EXPECT_FALSE(value) \
    do { \
        if ((value)) { \
            ++g_failures; \
            std::cerr << __FILE__ << ":" << __LINE__ << " expected false\n"; \
        } \
    } while (false)

void test_telex();
void test_vni();
void test_encoding();
void test_buffer();
