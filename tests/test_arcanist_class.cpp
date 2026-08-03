// test_arcanist_class.cpp
// RuneVault — assert-based tests for the Arcanist class Strategy interface,
// the concrete strategies, and ArcanistFactory::create (task 9.1).
//
// Covers:
//   * Factory: 's'/'h'/'w'/'v' produce Arcanists with the correct base stats
//     (FP/maxFp/Pwr/WR, Requirements 1.2–1.5, 1.8) and correct
//     className()/classKey().
//   * Sage scoreModifier +50%; Hexblade scrollEffectModifier doubles magnitude;
//     Warden damageTakenModifier -1; identity hooks on the non-ability classes.
//   * Voidwalker: seeded negative delta both flips and does not flip across
//     rolls, and a POSITIVE delta is never changed.
//   * ArchmageStrategy: className()=="Archmage" and the +50% scoring bonus.
//   * Factory with an invalid key returns nullptr.

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/arcanist_class.h"
#include "runevault/arcanist_factory.h"
#include "runevault/scroll_type.h"

#include "shuffle.h"

// ── Factory base-stats table (Requirements 1.2–1.5, 1.8) ────────────────────

RV_TEST(factory_creates_sage_with_correct_stats_and_identity) {
    using namespace runevault;
    auto a = ArcanistFactory::create('s');
    RV_CHECK(a != nullptr);
    RV_CHECK(a->fp() == 120);
    RV_CHECK(a->maxFp() == 120); // Requirement 1.8
    RV_CHECK(a->pwr() == 18);
    RV_CHECK(a->wr() == 18);
    RV_CHECK(a->hasStrategy());
    RV_CHECK(a->strategy().className() == "Sage");
    RV_CHECK(a->strategy().classKey() == 's');
}

RV_TEST(factory_creates_hexblade_with_correct_stats_and_identity) {
    using namespace runevault;
    auto a = ArcanistFactory::create('h');
    RV_CHECK(a != nullptr);
    RV_CHECK(a->fp() == 100);
    RV_CHECK(a->maxFp() == 100);
    RV_CHECK(a->pwr() == 24);
    RV_CHECK(a->wr() == 14);
    RV_CHECK(a->strategy().className() == "Hexblade");
    RV_CHECK(a->strategy().classKey() == 'h');
}

RV_TEST(factory_creates_warden_with_correct_stats_and_identity) {
    using namespace runevault;
    auto a = ArcanistFactory::create('w');
    RV_CHECK(a != nullptr);
    RV_CHECK(a->fp() == 150);
    RV_CHECK(a->maxFp() == 150);
    RV_CHECK(a->pwr() == 16);
    RV_CHECK(a->wr() == 22);
    RV_CHECK(a->strategy().className() == "Warden");
    RV_CHECK(a->strategy().classKey() == 'w');
}

RV_TEST(factory_creates_voidwalker_with_correct_stats_and_identity) {
    using namespace runevault;
    auto a = ArcanistFactory::create('v');
    RV_CHECK(a != nullptr);
    RV_CHECK(a->fp() == 110);
    RV_CHECK(a->maxFp() == 110);
    RV_CHECK(a->pwr() == 22);
    RV_CHECK(a->wr() == 12);
    RV_CHECK(a->strategy().className() == "Voidwalker");
    RV_CHECK(a->strategy().classKey() == 'v');
}

RV_TEST(factory_invalid_key_returns_nullptr) {
    using namespace runevault;
    RV_CHECK(ArcanistFactory::create('q') == nullptr);
    RV_CHECK(ArcanistFactory::create('x') == nullptr);
    RV_CHECK(ArcanistFactory::create('\0') == nullptr);
}

// ── scoreModifier (Requirement 13.2) ────────────────────────────────────────

RV_TEST(sage_score_modifier_applies_fifty_percent_bonus) {
    using namespace runevault;
    SageStrategy sage;
    RV_CHECK(sage.scoreModifier(100) == 150);
    RV_CHECK(sage.scoreModifier(0) == 0);
    RV_CHECK(sage.scoreModifier(5) == 7);  // 5 + 5/2 = 7 (truncated)
    RV_CHECK(sage.scoreModifier(10) == 15);
}

RV_TEST(non_sage_score_modifier_is_identity) {
    using namespace runevault;
    HexbladeStrategy hex;
    WardenStrategy warden;
    VoidwalkerStrategy vw;
    RV_CHECK(hex.scoreModifier(100) == 100);
    RV_CHECK(warden.scoreModifier(100) == 100);
    RV_CHECK(vw.scoreModifier(100) == 100);
}

// ── scrollEffectModifier (Requirements 8.11, 8.12) ──────────────────────────

RV_TEST(hexblade_scroll_effect_doubles_magnitude_keeping_sign) {
    using namespace runevault;
    HexbladeStrategy hex;
    RV_CHECK(hex.scrollEffectModifier(ScrollType::AttuneFocus, 5) == 10);
    RV_CHECK(hex.scrollEffectModifier(ScrollType::DrainFocus, -5) == -10);
    RV_CHECK(hex.scrollEffectModifier(ScrollType::SurgePower, 0) == 0);
}

RV_TEST(non_hexblade_scroll_effect_is_identity_for_positive_and_zero) {
    using namespace runevault;
    SageStrategy sage;
    WardenStrategy warden;
    RV_CHECK(sage.scrollEffectModifier(ScrollType::AttuneFocus, 5) == 5);
    RV_CHECK(sage.scrollEffectModifier(ScrollType::DrainFocus, -5) == -5);
    RV_CHECK(warden.scrollEffectModifier(ScrollType::SurgePower, 7) == 7);
    RV_CHECK(warden.scrollEffectModifier(ScrollType::SapPower, -7) == -7);
}

// ── damageTakenModifier (Requirement 6.7) ───────────────────────────────────

RV_TEST(warden_damage_taken_subtracts_one_without_flooring) {
    using namespace runevault;
    WardenStrategy warden;
    RV_CHECK(warden.damageTakenModifier(10) == 9);
    RV_CHECK(warden.damageTakenModifier(1) == 0);  // no max(1,..) floor here
    RV_CHECK(warden.damageTakenModifier(0) == -1); // ordering (task 7.3) floors
}

RV_TEST(non_warden_damage_taken_is_identity) {
    using namespace runevault;
    SageStrategy sage;
    HexbladeStrategy hex;
    VoidwalkerStrategy vw;
    RV_CHECK(sage.damageTakenModifier(10) == 10);
    RV_CHECK(hex.damageTakenModifier(10) == 10);
    RV_CHECK(vw.damageTakenModifier(10) == 10);
}

// ── Voidwalker flip (Requirement 8.12) ──────────────────────────────────────

RV_TEST(voidwalker_negative_delta_sometimes_flips_positive_never_changes) {
    using namespace runevault;
    VoidwalkerStrategy vw;
    rng::seed(2024);

    int flips = 0;      // negative -> positive counterpart
    int unchanged = 0;  // negative stays negative
    const int kRolls = 200;
    for (int i = 0; i < kRolls; ++i) {
        int out = vw.scrollEffectModifier(ScrollType::DrainFocus, -5);
        if (out == 5) {
            ++flips; // positive counterpart of -5, same magnitude
        } else if (out == -5) {
            ++unchanged;
        } else {
            RV_CHECK(false); // must only ever be +5 or -5
        }
    }
    // Over a seeded run both outcomes must occur (50% chance each).
    RV_CHECK(flips > 0);
    RV_CHECK(unchanged > 0);

    // A POSITIVE delta must NEVER be changed, regardless of the rolls.
    for (int i = 0; i < kRolls; ++i) {
        RV_CHECK(vw.scrollEffectModifier(ScrollType::AttuneFocus, 5) == 5);
    }
}

// ── ArchmageStrategy (Requirement 14.1/14.2) ────────────────────────────────

RV_TEST(archmage_reports_name_and_keeps_sage_scoring_bonus) {
    using namespace runevault;
    ArchmageStrategy arch;
    RV_CHECK(arch.className() == "Archmage");
    RV_CHECK(arch.classKey() == 's'); // upgraded Sage keeps the Sage key
    RV_CHECK(arch.scoreModifier(100) == 150);
    RV_CHECK(arch.scoreModifier(20) == 30);
    // The upgraded ability leaves the other hooks as identity.
    RV_CHECK(arch.scrollEffectModifier(ScrollType::DrainFocus, -5) == -5);
    RV_CHECK(arch.damageTakenModifier(10) == 10);
}

// ── setStrategy swap (promotion hook) ───────────────────────────────────────

RV_TEST(set_strategy_swaps_class_ability_in_place) {
    using namespace runevault;
    auto a = ArcanistFactory::create('s');
    RV_CHECK(a->strategy().className() == "Sage");
    a->setStrategy(
        std::unique_ptr<ArcanistClassStrategy>(new ArchmageStrategy()));
    RV_CHECK(a->strategy().className() == "Archmage");
    RV_CHECK(a->strategy().scoreModifier(100) == 150);
}
