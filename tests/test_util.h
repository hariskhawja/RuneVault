// test_util.h
// RuneVault — tiny assert-based test harness (no external dependencies).
//
// Usage in a test file:
//
//     #include "test_util.h"
//     #include "runevault/geometry.h"
//
//     RV_TEST(geometry_step_north_decreases_row) {
//         using namespace runevault;
//         Position p{5, 5};
//         RV_CHECK(Geometry::step(p, Direction::No).row == 4);
//     }
//
// Every RV_TEST registers itself automatically. A single test runner
// (tests/test_main.cpp) calls rv::test::run_all(), so individual test files
// must NOT define their own main().

#ifndef RUNEVAULT_TEST_UTIL_H
#define RUNEVAULT_TEST_UTIL_H

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace rv {
namespace test {

struct Case {
    std::string name;
    std::function<void(int &)> fn; // increments the failure counter on a failed check
};

// Meyers-singleton registry so registration works across translation units.
inline std::vector<Case> &registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(const std::string &name, std::function<void(int &)> fn) {
        registry().push_back(Case{name, std::move(fn)});
    }
};

// Runs every registered test. Returns 0 if all passed, 1 otherwise.
inline int run_all() {
    int failedCases = 0;
    for (const auto &c : registry()) {
        int failures = 0;
        c.fn(failures);
        if (failures == 0) {
            std::printf("[ PASS ] %s\n", c.name.c_str());
        } else {
            std::printf("[ FAIL ] %s (%d check(s) failed)\n", c.name.c_str(),
                        failures);
            ++failedCases;
        }
    }
    std::printf("\n%d test(s), %d failed.\n",
                static_cast<int>(registry().size()), failedCases);
    return failedCases == 0 ? 0 : 1;
}

} // namespace test
} // namespace rv

// Report a failed check without aborting the whole binary.
#define RV_CHECK(cond)                                                         \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("    check failed: %s (%s:%d)\n", #cond, __FILE__,      \
                        __LINE__);                                             \
            ++rv_failures__;                                                   \
        }                                                                      \
    } while (0)

// Define and auto-register a test case.
#define RV_TEST(name)                                                          \
    static void rv_test_##name(int &rv_failures__);                            \
    static ::rv::test::Registrar rv_reg_##name(#name, &rv_test_##name);        \
    static void rv_test_##name(int &rv_failures__)

#endif // RUNEVAULT_TEST_UTIL_H
