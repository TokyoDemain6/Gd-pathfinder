// A minimal test harness, to keep the project free of dependencies.
#pragma once

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

inline int g_checks = 0;
inline int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(actual, expected, eps)                                        \
    do {                                                                         \
        ++g_checks;                                                              \
        const double check_a = (actual);                                         \
        const double check_e = (expected);                                       \
        if (!(std::abs(check_a - check_e) <= (eps))) {                           \
            ++g_failures;                                                        \
            std::printf("FAIL %s:%d: %s = %.9f, expected %.9f\n", __FILE__,      \
                        __LINE__, #actual, check_a, check_e);                    \
        }                                                                        \
    } while (0)

inline int finish(const char* suite) {
    std::printf("%s: %d checks, %d failed\n", suite, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

inline std::string read_fixture(const std::string& name) {
    std::ifstream in(std::string(GDPF_FIXTURES) + "/" + name, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}
