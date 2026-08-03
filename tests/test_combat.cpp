// test_combat.cpp
// RuneVault — unit tests for the combat damage formula (task 7.1).
//
// Covers Requirements 6.2 and 6.6:
//     Damage = ceil( (100 / (100 + WR_defender)) * Pwr_attacker )
//
// Uses the project's assert-based harness (tests/test_util.h). No main() here;
// tests/test_main.cpp runs every registered case.

#include "test_util.h"

#include "runevault/combat.h"

// Known values: exact integers and non-integer cases that must round UP.
RV_TEST(combat_damage_known_values) {
    using namespace runevault;

    // 100/100 * 20 = 20 (exact).
    RV_CHECK(computeDamage(20, 0) == 20);

    // 100/200 * 20 = 10 (exact).
    RV_CHECK(computeDamage(20, 100) == 10);

    // 100/110 * 18 = 16.36... -> ceil = 17.
    RV_CHECK(computeDamage(18, 10) == 17);

    // 100/105 * 30 = 28.57... -> ceil = 29.
    RV_CHECK(computeDamage(30, 5) == 29);
}

// The ceiling must round any non-integer result strictly up.
RV_TEST(combat_damage_rounds_up_for_non_integers) {
    using namespace runevault;

    // 100/101 * 1 = 0.990... -> ceil = 1.
    RV_CHECK(computeDamage(1, 1) == 1);

    // 100/103 * 2 = 1.94... -> ceil = 2.
    RV_CHECK(computeDamage(2, 3) == 2);

    // 100/150 * 7 = 4.66... -> ceil = 5.
    RV_CHECK(computeDamage(7, 50) == 5);
}

// Zero (or non-positive) attacker Power deals no damage.
RV_TEST(combat_damage_zero_power_is_zero) {
    using namespace runevault;

    RV_CHECK(computeDamage(0, 0) == 0);
    RV_CHECK(computeDamage(0, 50) == 0);
    RV_CHECK(computeDamage(0, 999) == 0);
}

// Damage is never negative across a range of inputs.
RV_TEST(combat_damage_is_non_negative) {
    using namespace runevault;

    for (int pwr = 0; pwr <= 60; ++pwr) {
        for (int wr = 0; wr <= 60; ++wr) {
            RV_CHECK(computeDamage(pwr, wr) >= 0);
        }
    }
}

// Higher defender WR reduces (never increases) damage: monotonic non-increasing
// in WR for a fixed attacker Power.
RV_TEST(combat_damage_monotonic_non_increasing_in_wr) {
    using namespace runevault;

    const int pwr = 40;
    int previous = computeDamage(pwr, 0);
    for (int wr = 1; wr <= 200; ++wr) {
        int current = computeDamage(pwr, wr);
        RV_CHECK(current <= previous);
        previous = current;
    }
}

// ---------------------------------------------------------------------------
// Warden/Aegis damage-taken ordering (task 7.3).
//
// Covers Requirements 6.7, 6.8, 6.9, and 9.8. The strict order is:
//   1. Warden -1  ->  2. Aegis ceil(/2)  ->  3. max(1, ...)
// ---------------------------------------------------------------------------

// Neither Warden nor Aegis: damage passes through, floored at 1.
RV_TEST(mitigation_neither_passes_through) {
    using namespace runevault;

    RV_CHECK(applyDamageTakenOrdering(10, false, false) == 10);
    RV_CHECK(applyDamageTakenOrdering(1, false, false) == 1);
    RV_CHECK(applyDamageTakenOrdering(37, false, false) == 37);
}

// Warden only (Requirement 6.7): max(1, D - 1).
RV_TEST(mitigation_warden_only) {
    using namespace runevault;

    RV_CHECK(applyDamageTakenOrdering(10, true, false) == 9);   // 10 - 1
    RV_CHECK(applyDamageTakenOrdering(2, true, false) == 1);    // 2 - 1
}

// Aegis only, non-Warden (Requirement 6.8): max(1, ceil(D / 2)).
RV_TEST(mitigation_aegis_only_rounds_up) {
    using namespace runevault;

    RV_CHECK(applyDamageTakenOrdering(10, false, true) == 5);   // ceil(10/2)=5
    RV_CHECK(applyDamageTakenOrdering(11, false, true) == 6);   // ceil(11/2)=6
    RV_CHECK(applyDamageTakenOrdering(1, false, true) == 1);    // ceil(1/2)=1
}

// Warden + Aegis (Requirement 6.9): max(1, ceil((D - 1) / 2)).
RV_TEST(mitigation_warden_and_aegis) {
    using namespace runevault;

    RV_CHECK(applyDamageTakenOrdering(10, true, true) == 5);    // ceil(9/2)=5
    RV_CHECK(applyDamageTakenOrdering(11, true, true) == 5);    // ceil(10/2)=5
    RV_CHECK(applyDamageTakenOrdering(3, true, true) == 1);     // ceil(2/2)=1
}

// The final floor guarantees at least 1 FP of damage on a resolved hit.
RV_TEST(mitigation_floor_at_one) {
    using namespace runevault;

    RV_CHECK(applyDamageTakenOrdering(1, true, false) == 1);    // max(1, 0)
    RV_CHECK(applyDamageTakenOrdering(1, true, true) == 1);     // max(1, ceil(0/2))
    RV_CHECK(applyDamageTakenOrdering(0, false, false) == 1);   // max(1, 0)
    RV_CHECK(applyDamageTakenOrdering(0, true, true) == 1);     // clamp then floor
}

// Ordering matters: the Warden subtraction happens BEFORE the Aegis halving.
// Assert the spec composite formula 6.9 = max(1, ceil((D - 1) / 2)) holds for a
// range of D, distinguishing it from the wrong "halve then subtract" order.
RV_TEST(mitigation_warden_before_aegis_formula) {
    using namespace runevault;

    for (int d = 1; d <= 100; ++d) {
        int expected = ((d - 1) + 1) / 2; // ceil((d-1)/2) for d-1 >= 0
        if (expected < 1) {
            expected = 1;
        }
        RV_CHECK(applyDamageTakenOrdering(d, true, true) == expected);
    }

    // A concrete case where the two orderings could diverge: D = 4.
    //   correct (warden first): ceil((4-1)/2) = ceil(1.5) = 2
    //   wrong   (aegis first) : ceil(4/2) - 1 = 2 - 1 = 1
    RV_CHECK(applyDamageTakenOrdering(4, true, true) == 2);
}
