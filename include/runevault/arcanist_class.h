// arcanist_class.h
// RuneVault — the Arcanist class Strategy interface + concrete strategies
// (task 9.1, design "Arcanist: Factory Method + Strategy").
//
// The `Arcanist` is a single non-abstract stat container (arcanist.h); its one
// class ability is encapsulated behind an `ArcanistClassStrategy` so that no
// `switch(classType)` conditionals leak into combat, scoring, or scroll code
// (design DQ1). Each hook has an identity default meaning "no ability here", so
// a class with no ability in a given hook simply returns its input unchanged.
//
// The three ability hooks (design Strategy interface):
//   * scoreModifier(raw)                — Sage/Archmage +50%; others identity.
//   * scrollEffectModifier(type, delta) — Hexblade ×2 magnitude;
//                                         Voidwalker 50% negative->positive;
//                                         others identity.
//   * damageTakenModifier(incoming)     — Warden -1 (the combat ordering, task
//                                         7.3, applies the max(1,..) floor;
//                                         this hook does NOT floor); others
//                                         identity.
//
// Plus identity hooks className()/classKey() used by the View status area and
// the scoring/selection code.

#ifndef RUNEVAULT_ARCANIST_CLASS_H
#define RUNEVAULT_ARCANIST_CLASS_H

#include <string>

#include "runevault/scroll_type.h"

namespace runevault {

// Strategy interface: exactly the hook points the four class abilities (plus
// the Prestige promotions) need. Pure virtual — every concrete class
// supplies all five.
class ArcanistClassStrategy {
public:
    virtual ~ArcanistClassStrategy() = default;

    // End-game scoring hook (Requirement 13.2). Sage/Archmage apply a +50%
    // bonus; all others return `rawFragmentTotal` unchanged. The scoring wiring
    // itself is task 9.2.
    virtual int scoreModifier(int rawFragmentTotal) const = 0;

    // Scroll-effect hook applied to a scroll's signed `baseDelta`
    // (magnitude + sign) BEFORE it is applied to the Arcanist's stats/FP
    // (Requirements 8.11, 8.12). Hexblade doubles the magnitude; Voidwalker has
    // a 50% chance to flip a negative (harmful) delta to its positive
    // counterpart; all others return `baseDelta` unchanged. The scroll USE
    // wiring is task 12.x.
    virtual int scrollEffectModifier(ScrollType type, int baseDelta) const = 0;

    // Incoming-damage hook (Requirement 6.7). Warden subtracts 1; all others
    // return `incomingDamage` unchanged. NOTE: the max(1, ...) floor and the
    // Aegis halving are applied by the combat damage ordering (task 7.3), NOT
    // here.
    virtual int damageTakenModifier(int incomingDamage) const = 0;

    // Display name shown in the status area (Requirement 2.8).
    virtual std::string className() const = 0;

    // Selection key: 's' Sage, 'h' Hexblade, 'w' Warden, 'v' Voidwalker
    // (Requirements 1.2–1.5). E.g. Archmage keeps the Sage key 's' (it is an
    // upgraded Sage — see ArchmageStrategy).
    virtual char classKey() const = 0;
};

// ── Concrete strategies ─────────────────────────────────────────────────────

// Sage (Requirement 1.2 / 13.2): +50% final score. Every other hook is the
// identity transform.
class SageStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

// Hexblade (Requirement 1.3 / 8.11): doubles the magnitude of every scroll
// effect. Every other hook is the identity transform.
class HexbladeStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

// Warden (Requirement 1.4 / 6.7): reduces incoming damage by 1 (the min-1 floor
// is applied by the combat ordering, task 7.3). Every other hook is identity.
class WardenStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

// Voidwalker (Requirement 1.5 / 8.12): when a negative (harmful) scroll delta
// is used, a 50% chance (rng::chance(0.5)) flips it to its positive
// counterpart. Every other hook is the identity transform.
class VoidwalkerStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

// Archmage — the Prestige promotion strategy (Requirement 14.1). The design
// specifies only a Sage -> Archmage swap, so Archmage is modelled as an
// upgraded Sage: it keeps the Sage +50% scoring bonus and the Sage selection
// key 's' (so scoring/selection code still treats it as a Sage-family scorer),
// but reports className() == "Archmage". All other hooks are identity. The
// promotion factory that swaps this in is a later bonus task (task 14/16);
// task 9.1 only provides the strategy.
class ArchmageStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

// Warlock — the Prestige promotion of the Hexblade (Requirements 14.1/14.2).
// An upgraded Hexblade: its scroll-effect ability is strengthened from ×2 to
// ×3 magnitude (baseDelta * 3, sign preserved), while every other hook stays
// identity. It keeps the Hexblade selection key 'h' (so scroll code still
// treats it as a Hexblade-family caster) but reports className() == "Warlock".
class WarlockStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

// Bastion — the Prestige promotion of the Warden (Requirements 14.1/14.2). An
// upgraded Warden: its damage-taken ability is strengthened from -1 to -2
// (incomingDamage - 2; the max(1, ...) floor is still applied by the combat
// ordering, task 7.3, NOT here), while every other hook stays identity. It
// keeps the Warden selection key 'w' but reports className() == "Bastion".
class BastionStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

// Voidlord — the Prestige promotion of the Voidwalker (Requirements 14.1/14.2).
// An upgraded Voidwalker: its scroll-effect ability is strengthened from a 50%
// chance to flip a negative delta to a GUARANTEED flip (a negative baseDelta
// ALWAYS becomes its positive counterpart, -baseDelta, with no rng); a
// non-negative delta is never changed. Every other hook stays identity. It
// keeps the Voidwalker selection key 'v' but reports className() == "Voidlord".
class VoidlordStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

} // namespace runevault

#endif // RUNEVAULT_ARCANIST_CLASS_H
