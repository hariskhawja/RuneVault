// test_sanity.cpp
// RuneVault — scaffolding sanity checks.
//
// Proves the assert-based harness runs and that the test binary links against
// the provided rng utilities (shuffle.cpp). Real per-feature tests are added
// alongside each implementation task.

#include "test_util.h"

#include "shuffle.h"

RV_TEST(rng_range_stays_within_inclusive_bounds) {
    for (int i = 0; i < 100; ++i) {
        int v = rng::range(1, 6);
        RV_CHECK(v >= 1);
        RV_CHECK(v <= 6);
    }
}

RV_TEST(rng_chance_extremes_are_deterministic) {
    RV_CHECK(rng::chance(1.0) == true);
    RV_CHECK(rng::chance(0.0) == false);
}
