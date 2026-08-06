// combat.h
// RuneVault — combat resolution (task 7.1: the damage formula).
//
// This module owns the core combat math. It will grow in later tasks:
//   * Warden/Aegis damage-taken ordering (task 7.3),
//   * Arcanist attack resolution and the no-op case (task 7.5),
//   * Spectre attack, 50% miss, and loss-on-zero-FP (task 7.6).
//
// Damage formula (design "Combat Resolution", Requirements 6.2 and 6.6):
//
//     Damage = ceil( (100 / (100 + WR_defender)) * Pwr_attacker )
//
// The same formula computes both the Arcanist's outgoing damage (Requirement
// 6.2) and a Spectre's base damage against the Arcanist (Requirement 6.6); only
// the attacker/defender terms differ. Callers pass the EFFECTIVE Power and Ward
// Rating — i.e. the scroll-modified values produced by the Decorator component
// (task 12.1). This function itself is unaware of decorators, classes, Warden
// mitigation, or the Aegis Cloak; those adjustments layer on top later.

#ifndef RUNEVAULT_COMBAT_H
#define RUNEVAULT_COMBAT_H

namespace runevault {

// Compute base combat damage for one hit.
//
//   attackerPwr  effective Power (Pwr) of the attacker
//   defenderWr   effective Ward Rating (WR) of the defender
//
// Returns ceil( (100.0 / (100 + defenderWr)) * attackerPwr ) as a non-negative
// integer.
//
// Implementation uses integer-safe ceiling division to avoid floating-point
// rounding surprises. Because inputs are non-negative, the exact rational value
// is (100 * attackerPwr) / (100 + defenderWr); the integer ceiling of a/b for
// b > 0 is (a + b - 1) / b. Since defenderWr >= 0 the denominator (100 + WR) is
// always >= 100 > 0, so the division is always well defined. If attackerPwr is
// 0 the numerator is 0 and the result is 0; the result is never negative.
int computeDamage(int attackerPwr, int defenderWr);

// Apply the strict Warden/Aegis damage-taken ordering (task 7.3).
//
// Given a base damage value D already computed by computeDamage() for a
// Spectre's attack against the Arcanist (Requirement 6.6), this reduces it to
// the final FP loss by applying, IN THIS EXACT ORDER (DD-ordering):
//
//   1. Warden modifier: if the Arcanist is a Warden, subtract 1
//        D1 = D - (isWarden ? 1 : 0)
//      This mirrors the class Strategy's `damageTakenModifier` (Warden -1).
//      D1 is clamped to >= 0 so the halving step below never sees a negative
//      intermediate (the final max(1, ...) already guarantees a >= 1 result,
//      but we avoid a nonsensical negative before ceiling).
//   2. Aegis Cloak halving: if carrying the Aegis Cloak, halve rounded up
//        D2 = ceil(D1 / 2) = (D1 + 1) / 2   (valid for D1 >= 0)
//      Not carrying: D2 = D1. (Requirement 9.8: while carrying the Aegis
//      Cloak, halve all damage taken, rounded up, subject to this ordering.)
//   3. Floor: result = max(1, D2)
//
// The four composite cases required by the acceptance criteria fall out of
// this single ordering:
//   * Warden, no Aegis      (6.7): max(1, D - 1)
//   * Aegis, non-Warden     (6.8): max(1, ceil(D / 2))
//   * Warden + Aegis        (6.9): max(1, ceil((D - 1) / 2))
//   * Neither                    : max(1, D)
//
// This is a pure function parameterized by booleans so task 9.1 can wire the
// class Strategy into step 1 (supplying `isWarden` via
// strategy.damageTakenModifier) and task 7.6 can call it with the Spectre's
// base damage together with the model's Warden / carryingAegisCloak state.
int applyDamageTakenOrdering(int baseDamage, bool isWarden, bool carryingAegis);

} // namespace runevault

#endif // RUNEVAULT_COMBAT_H
