// arcanist_class.cpp
// RuneVault — concrete Arcanist class strategies (task 9.1).
//
// See include/runevault/arcanist_class.h for the interface and the per-class
// ability summary. All randomness (Voidwalker) routes through the provided rng
// utilities (shuffle.h).

#include "runevault/arcanist_class.h"

#include "shuffle.h"

namespace runevault {

namespace {
// Sage/Archmage scoring bonus: +50%, computed as raw + raw/2 with integer
// truncation (e.g. 100 -> 150, 5 -> 7). Documented rounding: the half is
// truncated toward zero. Negative totals cannot occur (fragment totals are
// non-negative) but the formula is monotonic for them regardless.
int applyFiftyPercentBonus(int raw) { return raw + raw / 2; }
} // namespace

// ── SageStrategy ────────────────────────────────────────────────────────────
int SageStrategy::scoreModifier(int rawFragmentTotal) const {
    return applyFiftyPercentBonus(rawFragmentTotal);
}
int SageStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta; // identity
}
int SageStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage; // identity
}
std::string SageStrategy::className() const { return "Sage"; }
char SageStrategy::classKey() const { return 's'; }

// ── HexbladeStrategy ────────────────────────────────────────────────────────
int HexbladeStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal; // identity
}
int HexbladeStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    // Double the magnitude while preserving the sign (Requirement 8.11).
    return baseDelta * 2;
}
int HexbladeStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage; // identity
}
std::string HexbladeStrategy::className() const { return "Hexblade"; }
char HexbladeStrategy::classKey() const { return 'h'; }

// ── WardenStrategy ──────────────────────────────────────────────────────────
int WardenStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal; // identity
}
int WardenStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta; // identity
}
int WardenStrategy::damageTakenModifier(int incomingDamage) const {
    // Reduce incoming damage by 1 (Requirement 6.7). The max(1, ...) floor is
    // applied later by the combat damage ordering (task 7.3), so this hook may
    // return 0 or a negative value; that is intentional.
    return incomingDamage - 1;
}
std::string WardenStrategy::className() const { return "Warden"; }
char WardenStrategy::classKey() const { return 'w'; }

// ── VoidwalkerStrategy ──────────────────────────────────────────────────────
int VoidwalkerStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal; // identity
}
int VoidwalkerStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    // Requirement 8.12: for a negative (harmful) scroll, a 50% chance to
    // produce the positive counterpart of that effect instead. We flip the sign
    // of a negative delta with probability 0.5; non-negative deltas are never
    // changed.
    if (baseDelta < 0 && rng::chance(0.5)) {
        return -baseDelta; // positive counterpart, same magnitude
    }
    return baseDelta;
}
int VoidwalkerStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage; // identity
}
std::string VoidwalkerStrategy::className() const { return "Voidwalker"; }
char VoidwalkerStrategy::classKey() const { return 'v'; }

// ── ArchmageStrategy (Prestige promotion) ───────────────────────────────────
int ArchmageStrategy::scoreModifier(int rawFragmentTotal) const {
    // Upgraded Sage: keeps the +50% scoring bonus (Requirement 14.2 preserves
    // core scoring behaviour).
    return applyFiftyPercentBonus(rawFragmentTotal);
}
int ArchmageStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta; // identity
}
int ArchmageStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage; // identity
}
std::string ArchmageStrategy::className() const { return "Archmage"; }
char ArchmageStrategy::classKey() const { return 's'; } // upgraded Sage

// ── WarlockStrategy (Prestige promotion of the Hexblade) ────────────────────
int WarlockStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal; // identity
}
int WarlockStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    // Upgraded Hexblade: triple the magnitude while preserving the sign
    // (Requirement 14.2 upgrades the Hexblade ×2 to ×3).
    return baseDelta * 3;
}
int WarlockStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage; // identity
}
std::string WarlockStrategy::className() const { return "Warlock"; }
char WarlockStrategy::classKey() const { return 'h'; } // upgraded Hexblade

// ── BastionStrategy (Prestige promotion of the Warden) ──────────────────────
int BastionStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal; // identity
}
int BastionStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta; // identity
}
int BastionStrategy::damageTakenModifier(int incomingDamage) const {
    // Upgraded Warden: reduce incoming damage by 2 (Requirement 14.2 upgrades
    // the Warden -1 to -2). The max(1, ...) floor is applied later by the
    // combat damage ordering (task 7.3), so this hook may return 0 or a
    // negative value; that is intentional (mirrors WardenStrategy).
    return incomingDamage - 2;
}
std::string BastionStrategy::className() const { return "Bastion"; }
char BastionStrategy::classKey() const { return 'w'; } // upgraded Warden

// ── VoidlordStrategy (Prestige promotion of the Voidwalker) ─────────────────
int VoidlordStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal; // identity
}
int VoidlordStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    // Upgraded Voidwalker: a negative (harmful) delta is ALWAYS flipped to its
    // positive counterpart (Requirement 14.2 upgrades the 50% chance to a
    // guaranteed flip — no rng needed). A non-negative delta is never changed.
    if (baseDelta < 0) {
        return -baseDelta; // positive counterpart, same magnitude
    }
    return baseDelta;
}
int VoidlordStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage; // identity
}
std::string VoidlordStrategy::className() const { return "Voidlord"; }
char VoidlordStrategy::classKey() const { return 'v'; } // upgraded Voidwalker

} // namespace runevault
