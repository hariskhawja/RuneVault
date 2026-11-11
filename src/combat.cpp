#include "runevault/combat.h"

namespace runevault {

int computeDamage(int attackerPwr, int defenderWr) {
    if (attackerPwr <= 0) {
        return 0;
    }
    if (defenderWr < 0) {
        defenderWr = 0;
    }

    // Integer ceiling of 100 * power / (100 + ward).
    const int numerator = 100 * attackerPwr;
    const int denominator = 100 + defenderWr;
    return (numerator + denominator - 1) / denominator;
}

int applyDamageTakenOrdering(int baseDamage, bool isWarden, bool carryingAegis) {
    // Apply class reduction before cloak halving, then enforce minimum damage.
    int d1 = baseDamage - (isWarden ? 1 : 0);
    if (d1 < 0) {
        d1 = 0;
    }

    int d2 = carryingAegis ? (d1 + 1) / 2 : d1;

    // Step 3 — floor at 1 so any resolved hit costs at least 1 FP.
    return d2 < 1 ? 1 : d2;
}

} // namespace runevault
