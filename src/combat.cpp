// combat.cpp
// RuneVault — combat resolution (task 7.1: the damage formula).
//
// See include/runevault/combat.h for the contract and the rationale behind the
// integer-safe ceiling division.

#include "runevault/combat.h"

namespace runevault {

int computeDamage(int attackerPwr, int defenderWr) {
    // Guard against negative/degenerate inputs so the result stays a
    // non-negative integer (Requirements 6.2, 6.6). Effective Pwr/WR are
    // clamped to >= 0 by the scroll decorators (task 12.1), but we defend here
    // too so this pure function is safe in isolation.
    if (attackerPwr <= 0) {
        return 0;
    }
    if (defenderWr < 0) {
        defenderWr = 0;
    }

    // Exact spec value is ceil( (100 / (100 + WR)) * Pwr ) = ceil( num / den )
    // with num = 100 * Pwr and den = 100 + WR. Integer ceiling of num/den for
    // den > 0 is (num + den - 1) / den. den >= 100 > 0 always holds here.
    const int numerator = 100 * attackerPwr;
    const int denominator = 100 + defenderWr;
    return (numerator + denominator - 1) / denominator;
}

int applyDamageTakenOrdering(int baseDamage, bool isWarden, bool carryingAegis) {
    // Step 1 — Warden modifier (strategy damageTakenModifier: Warden -1).
    // Requirements 6.7 / 6.9. Clamp to >= 0 so the halving step never operates
    // on a negative intermediate (the final max(1, ...) still guarantees >= 1).
    int d1 = baseDamage - (isWarden ? 1 : 0);
    if (d1 < 0) {
        d1 = 0;
    }

    // Step 2 — Aegis Cloak halving, rounded up (Requirements 6.8 / 6.9 / 9.8).
    // Integer ceiling of d1/2 for d1 >= 0 is (d1 + 1) / 2.
    int d2 = carryingAegis ? (d1 + 1) / 2 : d1;

    // Step 3 — floor at 1 so any resolved hit costs at least 1 FP.
    return d2 < 1 ? 1 : d2;
}

} // namespace runevault
