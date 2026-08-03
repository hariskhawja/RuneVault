// test_promotion.cpp
// RuneVault — assert-based tests for the Prestige PROMOTION feature (bonus
// feature; Requirements 14.1/14.2).
//
// Covers:
//   * The three new promotion strategies (Warlock/Bastion/Voidlord): their
//     upgraded ability hooks, identity hooks, className() and classKey().
//     (Archmage already has coverage in test_arcanist_class.cpp.)
//   * PromotionFactory::promote: each base class swaps to its promotion and all
//     three stats (FP/Pwr/WR) strictly increase by the documented buff.
//   * Model trigger: promotion fires ONCE at the 10-RF threshold via
//     addRuneFragments, buffs effective stats, pushes a notification, does not
//     re-fire, and is a safe no-op with no Arcanist.

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/arcanist_class.h"
#include "runevault/arcanist_factory.h"
#include "runevault/model.h"
#include "runevault/notification_log.h"
#include "runevault/promotion_factory.h"
#include "runevault/scroll_type.h"

// ── Promotion strategy behaviour ────────────────────────────────────────────

RV_TEST(warlock_triples_scroll_magnitude_other_hooks_identity) {
    using namespace runevault;
    WarlockStrategy w;
    RV_CHECK(w.scrollEffectModifier(ScrollType::SurgePower, 5) == 15);
    RV_CHECK(w.scrollEffectModifier(ScrollType::DrainFocus, -10) == -30);
    RV_CHECK(w.className() == "Warlock");
    RV_CHECK(w.classKey() == 'h');
    RV_CHECK(w.scoreModifier(100) == 100);      // identity
    RV_CHECK(w.damageTakenModifier(10) == 10);  // identity
}

RV_TEST(bastion_reduces_damage_by_two_other_hooks_identity) {
    using namespace runevault;
    BastionStrategy b;
    RV_CHECK(b.damageTakenModifier(10) == 8);
    RV_CHECK(b.damageTakenModifier(1) == -1); // no max(1,..) floor here (task 7.3)
    RV_CHECK(b.className() == "Bastion");
    RV_CHECK(b.classKey() == 'w');
    RV_CHECK(b.scoreModifier(100) == 100);                                 // identity
    RV_CHECK(b.scrollEffectModifier(ScrollType::SurgePower, 5) == 5);      // identity
}

RV_TEST(voidlord_always_flips_negative_leaves_positive) {
    using namespace runevault;
    VoidlordStrategy v;
    RV_CHECK(v.scrollEffectModifier(ScrollType::DrainFocus, -5) == 5); // always flips
    RV_CHECK(v.scrollEffectModifier(ScrollType::AttuneFocus, 5) == 5); // positive unchanged
    RV_CHECK(v.scrollEffectModifier(ScrollType::SurgePower, 0) == 0);  // zero unchanged
    RV_CHECK(v.className() == "Voidlord");
    RV_CHECK(v.classKey() == 'v');
    RV_CHECK(v.scoreModifier(100) == 100);     // identity
    RV_CHECK(v.damageTakenModifier(10) == 10); // identity
}

// ── PromotionFactory::promote — per-class swap + all-stats buff ─────────────

RV_TEST(promote_sage_becomes_archmage_all_stats_buffed) {
    using namespace runevault;
    auto a = ArcanistFactory::create('s'); // 120 / 18 / 18
    PromotionFactory::promote(*a);
    RV_CHECK(a->strategy().className() == "Archmage");
    RV_CHECK(a->maxFp() == 150); // 120 + 30
    RV_CHECK(a->fp() == 150);    // current FP rose with max
    RV_CHECK(a->pwr() == 24);    // 18 + 6
    RV_CHECK(a->wr() == 24);     // 18 + 6
    // All three stats strictly increased.
    RV_CHECK(a->maxFp() > 120 && a->pwr() > 18 && a->wr() > 18);
}

RV_TEST(promote_hexblade_becomes_warlock_all_stats_buffed) {
    using namespace runevault;
    auto a = ArcanistFactory::create('h'); // 100 / 24 / 14
    PromotionFactory::promote(*a);
    RV_CHECK(a->strategy().className() == "Warlock");
    RV_CHECK(a->maxFp() == 120); // 100 + 20
    RV_CHECK(a->fp() == 120);
    RV_CHECK(a->pwr() == 34);    // 24 + 10
    RV_CHECK(a->wr() == 18);     // 14 + 4
    RV_CHECK(a->maxFp() > 100 && a->pwr() > 24 && a->wr() > 14);
}

RV_TEST(promote_warden_becomes_bastion_all_stats_buffed) {
    using namespace runevault;
    auto a = ArcanistFactory::create('w'); // 150 / 16 / 22
    PromotionFactory::promote(*a);
    RV_CHECK(a->strategy().className() == "Bastion");
    RV_CHECK(a->maxFp() == 190); // 150 + 40
    RV_CHECK(a->fp() == 190);
    RV_CHECK(a->pwr() == 20);    // 16 + 4
    RV_CHECK(a->wr() == 30);     // 22 + 8
    RV_CHECK(a->maxFp() > 150 && a->pwr() > 16 && a->wr() > 22);
}

RV_TEST(promote_voidwalker_becomes_voidlord_all_stats_buffed) {
    using namespace runevault;
    auto a = ArcanistFactory::create('v'); // 110 / 22 / 12
    PromotionFactory::promote(*a);
    RV_CHECK(a->strategy().className() == "Voidlord");
    RV_CHECK(a->maxFp() == 130); // 110 + 20
    RV_CHECK(a->fp() == 130);
    RV_CHECK(a->pwr() == 32);    // 22 + 10
    RV_CHECK(a->wr() == 16);     // 12 + 4
    RV_CHECK(a->maxFp() > 110 && a->pwr() > 22 && a->wr() > 12);
}

// ── Model trigger — threshold, once-only, guard ─────────────────────────────

RV_TEST(model_promotes_once_at_threshold_via_add_rune_fragments) {
    using namespace runevault;
    NotificationLog::instance().clear();

    Model model;
    model.setBonusEnabled(true); // promotion is a BONUS feature, gated OFF by default
    model.setArcanist(ArcanistFactory::create('h')); // Hexblade, base pwr 24
    RV_CHECK(model.promoted() == false);

    // Below the threshold: no promotion yet.
    model.addRuneFragments(9);
    RV_CHECK(model.runeFragmentTotal() == 9);
    RV_CHECK(model.promoted() == false);
    RV_CHECK(model.arcanist()->strategy().className() == "Hexblade");

    // Crossing the threshold (total 10): promote once.
    model.addRuneFragments(1);
    RV_CHECK(model.runeFragmentTotal() == 10);
    RV_CHECK(model.promoted() == true);
    RV_CHECK(model.arcanist()->strategy().className() == "Warlock");
    // Effective Power reflects the buff immediately (24 -> 34).
    RV_CHECK(model.effectivePwr() == 34);

    // A promotion notification was pushed.
    bool sawPromotion = false;
    for (const std::string &m : NotificationLog::instance().messages()) {
        if (m.find("ascends") != std::string::npos &&
            m.find("Warlock") != std::string::npos) {
            sawPromotion = true;
        }
    }
    RV_CHECK(sawPromotion);
}

RV_TEST(model_promotion_happens_only_once) {
    using namespace runevault;
    NotificationLog::instance().clear();

    Model model;
    model.setBonusEnabled(true); // promotion is a BONUS feature, gated OFF by default
    model.setArcanist(ArcanistFactory::create('h')); // Hexblade
    model.addRuneFragments(10); // promotes -> Warlock, pwr 34
    RV_CHECK(model.promoted() == true);
    RV_CHECK(model.effectivePwr() == 34);

    // Gaining more RF must NOT promote again (no second buff, no name change).
    model.addRuneFragments(10);
    RV_CHECK(model.runeFragmentTotal() == 20);
    RV_CHECK(model.arcanist()->strategy().className() == "Warlock");
    RV_CHECK(model.effectivePwr() == 34); // stays 34, not 44
}

RV_TEST(model_without_arcanist_does_not_crash_or_promote) {
    using namespace runevault;
    Model model; // no Arcanist set
    model.addRuneFragments(20);
    RV_CHECK(model.runeFragmentTotal() == 20);
    RV_CHECK(model.promoted() == false);
}

// --- Bonus gate: with bonus DISABLED (default), reaching 10 RF does NOT promote
// The Prestige promotion is a BONUS feature (project spec: base game must play
// WITHOUT it, no recompile). A default Model has bonus OFF, so crossing the
// 10-RF threshold must leave the class unchanged and promoted() == false.
RV_TEST(model_does_not_promote_when_bonus_disabled) {
    using namespace runevault;
    NotificationLog::instance().clear();

    Model model; // bonus defaults OFF
    model.setArcanist(ArcanistFactory::create('h')); // Hexblade, base pwr 24
    RV_CHECK(model.bonusEnabled() == false);

    // Reaching (and exceeding) the threshold must NOT promote.
    model.addRuneFragments(20);
    RV_CHECK(model.runeFragmentTotal() == 20);
    RV_CHECK(model.promoted() == false);
    RV_CHECK(model.arcanist()->strategy().className() == "Hexblade"); // base class
    RV_CHECK(model.effectivePwr() == 24); // no stat buff applied

    // No promotion notification was pushed.
    bool sawPromotion = false;
    for (const std::string &m : NotificationLog::instance().messages()) {
        if (m.find("ascends") != std::string::npos) {
            sawPromotion = true;
        }
    }
    RV_CHECK(!sawPromotion);
}
