// arcanist.h
// RuneVault — the Arcanist stat container + mutators (task 5.1).
//
// The `Arcanist` is a single **non-abstract** class holding the player's core
// stats — Focus Points (FP), maximum FP, Power (Pwr), and Ward Rating (WR) —
// plus the mutators that combat and scrolls drive them through (design
// "Arcanist: Factory Method + Strategy").
//
// SCOPE NOTE: the base stat container originated in task 5.1. Task 9.1 layers
// the class-ability Strategy onto it: an optional `ArcanistClassStrategy`
// pointer (arcanist_class.h) plus a strategy-aware constructor, a `strategy()`
// accessor, and `setStrategy()` (used by the later Prestige promotion factory).
// The original stat-only constructor is kept working (with a null strategy) so
// existing callers/tests continue to compile; the strategy-routed
// `damageTakenModifier` / Aegis-Cloak damage ordering still lives in the combat
// layer (task 7.3) and is not wired through `applyDamage` here.

#ifndef RUNEVAULT_ARCANIST_H
#define RUNEVAULT_ARCANIST_H

#include <memory>

#include "runevault/arcanist_class.h"

namespace runevault {

// The player character's stat block. Values are plain ints; every mutator
// keeps the invariants "FP in [0, maxFp]" and "pwr_/wr_ >= 0".
class Arcanist {
public:
    // Create an Arcanist with `fp` starting Focus Points, `pwr` Power, and `wr`
    // Ward Rating. Maximum FP is set equal to the starting FP (Requirement
    // 1.8). Negative arguments are floored at 0. This overload leaves the class
    // Strategy unset (null); use it for the stat-only scaffolding/tests that
    // predate the Strategy (task 5.1).
    Arcanist(int fp, int pwr, int wr);

    // Create an Arcanist with the given stats AND an attached class Strategy
    // (task 9.1). The Strategy is normally supplied by `ArcanistFactory::create`
    // (arcanist_factory.h). `strategy` may be null, which behaves like the
    // stat-only overload above.
    Arcanist(int fp, int pwr, int wr,
             std::unique_ptr<ArcanistClassStrategy> strategy);

    // Stat accessors.
    int fp() const { return fp_; }
    int maxFp() const { return maxFp_; }
    int pwr() const { return pwr_; }
    int wr() const { return wr_; }

    // True once Focus Points have been depleted; the run ends in loss when this
    // becomes true (Requirement 1: "FP reaches 0").
    bool isDead() const { return fp_ == 0; }

    // Subtract `amount` Focus Points, flooring at 0. A negative `amount` is a
    // no-op (it never heals).
    void reduceFp(int amount);

    // Apply `rawDamage` to Focus Points, flooring at 0. NOTE: for this task
    // damage is raw; the strategy `damageTakenModifier` hook and the
    // Warden/Aegis-Cloak ordering are wired in tasks 9.1 / 7.3, at which point
    // this gains an `aegis` argument.
    void applyDamage(int rawDamage);

    // Attune Focus (Requirement 8.3): restore up to `amount` FP without
    // exceeding maxFp. Permanent. A negative `amount` is a no-op.
    void healUpTo(int amount);

    // Drain Focus (Requirement 8.4): reduce FP by up to `amount` without
    // falling below 0. Permanent. A negative `amount` is a no-op.
    void loseUpTo(int amount);

    // Unused setter methods by program
    // Used setter methods by tests
    // Stats are floored at 0 so it never goes negative.
    void addPwr(int delta);
    void addWr(int delta);

    // Promotion stat buff (Prestige bonus): raise maximum FP and current FP by
    // fpBonus, and Power / Ward Rating by pwrBonus / wrBonus. Bonuses are clamped
    // to >= 0. Current FP rises with max FP so the promotion is immediately felt;
    // the FP <= maxFp invariant is preserved (both rise by the same amount).
    void applyPromotionBonus(int fpBonus, int pwrBonus, int wrBonus);

    // ---- Class Strategy (task 9.1) ----
    // True when a class Strategy is attached. Callers must check this before
    // dereferencing `strategy()` on an Arcanist built by the stat-only ctor.
    bool hasStrategy() const { return strategy_ != nullptr; }

    // Access the attached class Strategy (used by scoring / scroll / combat).
    // Precondition: `hasStrategy()` is true; calling this with no strategy is
    // undefined behaviour.
    const ArcanistClassStrategy& strategy() const { return *strategy_; }

    // Replace the class Strategy in place. Used by the Prestige promotion
    // factory (Sage -> Archmage) in a later bonus task; provided now so that
    // wiring needs no further change to this class.
    void setStrategy(std::unique_ptr<ArcanistClassStrategy> strategy) {
        strategy_ = std::move(strategy);
    }

private:
    int fp_;
    int maxFp_;
    int pwr_;
    int wr_;
    std::unique_ptr<ArcanistClassStrategy> strategy_; // null until attached
};

} // namespace runevault

#endif // RUNEVAULT_ARCANIST_H
