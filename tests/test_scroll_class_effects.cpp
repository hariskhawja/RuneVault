// test_scroll_class_effects.cpp
// RuneVault — class-ability scroll effects routed through the Strategy
// (task 12.4). Model::useScroll routes each scroll's signed base delta through
// the class Strategy's scrollEffectModifier before applying it:
//   * Hexblade (Requirement 8.11): doubles the magnitude, keeping the sign.
//   * Voidwalker (Requirement 8.12): 50% chance to flip a NEGATIVE effect to its
//     positive counterpart; a positive effect is never flipped.
//   * Every other class (here: Sage) uses the base magnitudes (identity path).
//
// Models are built via ArcanistFactory::create so a real class Strategy is
// attached; scrolls are placed one step EAST and consumed with useScroll(Ea).
//
// **Validates: Requirements 8.11, 8.12**

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/arcanist_factory.h"
#include "runevault/item.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/scroll_type.h"

#include "shuffle.h" // rng::seed

#include <memory>

namespace {

using namespace runevault;

// Build a Model with a small all-floor board and an Arcanist of class `key`
// (from the factory, so the real Strategy is attached) standing at (1,1) with
// all 8 neighbours in-bounds walkable floor.
std::unique_ptr<Model> makeClassModel(char key) {
    Board board(3, 3);
    for (int r = 0; r < board.rows(); ++r) {
        for (int c = 0; c < board.cols(); ++c) {
            board.setCell(Position{r, c}, CellType::Floor, 1);
        }
    }
    auto model = std::unique_ptr<Model>(new Model());
    model->setLevel(std::unique_ptr<Level>(new Level(std::move(board))));
    model->setArcanist(ArcanistFactory::create(key));
    model->setArcanistPosition(Position{1, 1});
    return model;
}

// Place a scroll of `type` one step EAST of the Arcanist (the target tile for
// useScroll(Direction::Ea)) and return that model back for chaining.
void placeScrollEast(Model &model, ScrollType type) {
    const Position target =
        Geometry::step(model.arcanistPosition(), Direction::Ea);
    auto scroll = std::unique_ptr<Scroll>(new Scroll(type));
    scroll->setPosition(target);
    model.level()->addItem(std::move(scroll));
}

// Convenience: fresh class model with one EAST scroll of `type`, ready to use.
std::unique_ptr<Model> modelWithScroll(char key, ScrollType type) {
    auto model = makeClassModel(key);
    placeScrollEast(*model, type);
    return model;
}

} // namespace

// ── Hexblade (Requirement 8.11): every effect's magnitude is doubled ────────
// Hexblade base stats: fp/maxFp 100, pwr 24, wr 14.

RV_TEST(hexblade_surge_power_raises_effective_pwr_by_ten) {
    rng::seed(1);
    auto model = modelWithScroll('h', ScrollType::SurgePower);
    RV_CHECK(model->effectivePwr() == 24);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectivePwr() == 34); // +10, not the base +5
    RV_CHECK(model->effectiveWr() == 14);  // WR unaffected
}

RV_TEST(hexblade_fortify_ward_raises_effective_wr_by_ten) {
    rng::seed(1);
    auto model = modelWithScroll('h', ScrollType::FortifyWard);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectiveWr() == 24); // 14 + 10
    RV_CHECK(model->effectivePwr() == 24);
}

RV_TEST(hexblade_attune_focus_heals_twenty_capped_at_max) {
    rng::seed(1);
    auto model = makeClassModel('h');
    model->arcanist()->reduceFp(30); // fp = 70 (maxFp 100)
    placeScrollEast(*model, ScrollType::AttuneFocus);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->arcanist()->fp() == 90); // 70 + 20

    // Near the cap the doubled heal is clamped to maxFp.
    model->arcanist()->reduceFp(5); // fp = 85
    placeScrollEast(*model, ScrollType::AttuneFocus);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->arcanist()->fp() == 100); // 85 + 20 -> capped at 100
}

RV_TEST(hexblade_drain_focus_reduces_twenty) {
    rng::seed(1);
    auto model = modelWithScroll('h', ScrollType::DrainFocus); // fp = 100
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->arcanist()->fp() == 80); // 100 - 20
}

RV_TEST(hexblade_sap_power_lowers_effective_pwr_by_ten) {
    rng::seed(1);
    auto model = modelWithScroll('h', ScrollType::SapPower);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectivePwr() == 14); // max(0, 24 - 10)
}

RV_TEST(hexblade_erode_ward_lowers_effective_wr_by_ten) {
    rng::seed(1);
    auto model = modelWithScroll('h', ScrollType::ErodeWard);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectiveWr() == 4); // max(0, 14 - 10)
}

// ── Voidwalker (Requirement 8.12): negative effects may flip to positive ────
// Voidwalker base stats: fp/maxFp 110, pwr 22, wr 12.

// A negative Power scroll (Sap -5) must produce BOTH outcomes across seeded
// trials: sometimes the effective Power DROPS (no flip -> Sap 5 -> 17) and
// sometimes it RISES (flip -> Surge 5 -> 27). Seeded once for reproducibility;
// the global engine advances across the fresh-model trials, giving a mix.
RV_TEST(voidwalker_sap_power_flips_and_does_not_flip_across_seeded_trials) {
    rng::seed(12345);
    bool sawFlip = false;   // Sap -5 became Surge +5 -> effectivePwr 27
    bool sawNoFlip = false; // Sap -5 stayed a debuff       -> effectivePwr 17
    const int kTrials = 60;
    for (int i = 0; i < kTrials; ++i) {
        auto model = modelWithScroll('v', ScrollType::SapPower);
        RV_CHECK(model->useScroll(Direction::Ea) == true);
        const int pwr = model->effectivePwr();
        if (pwr == 27) {
            sawFlip = true;
        } else if (pwr == 17) {
            sawNoFlip = true;
        } else {
            RV_CHECK(false); // effective Power must be exactly 17 or 27
        }
    }
    RV_CHECK(sawFlip);   // at least one 50% flip to a Power BUFF occurred
    RV_CHECK(sawNoFlip); // at least one non-flip (real debuff) occurred
}

// A negative FP scroll (Drain -10) can flip to a heal (+10). Reduce FP first so
// both directions are observable: no flip -> 60 - 10 = 50; flip -> 60 + 10 = 70.
RV_TEST(voidwalker_drain_focus_can_flip_to_a_heal) {
    rng::seed(2024);
    bool sawHeal = false; // flip: loseUpTo -> healUpTo, fp rises to 70
    bool sawDrain = false; // no flip: fp drops to 50
    const int kTrials = 60;
    for (int i = 0; i < kTrials; ++i) {
        auto model = makeClassModel('v');
        model->arcanist()->reduceFp(50); // fp = 60
        placeScrollEast(*model, ScrollType::DrainFocus);
        RV_CHECK(model->useScroll(Direction::Ea) == true);
        const int fp = model->arcanist()->fp();
        if (fp == 70) {
            sawHeal = true;
        } else if (fp == 50) {
            sawDrain = true;
        } else {
            RV_CHECK(false); // fp must be exactly 50 or 70
        }
    }
    RV_CHECK(sawHeal);
    RV_CHECK(sawDrain);
}

// A POSITIVE scroll is NEVER flipped for a Voidwalker: Surge Power always adds
// the base +5, regardless of the rolls.
RV_TEST(voidwalker_positive_scroll_is_never_flipped) {
    rng::seed(999);
    const int kTrials = 60;
    for (int i = 0; i < kTrials; ++i) {
        auto model = modelWithScroll('v', ScrollType::SurgePower);
        RV_CHECK(model->useScroll(Direction::Ea) == true);
        RV_CHECK(model->effectivePwr() == 27); // 22 + 5, always
    }
}

// ── Sanity: an identity-path class (Sage) uses the base magnitudes ──────────
// Sage base stats: fp/maxFp 120, pwr 18, wr 18. scrollEffectModifier is
// identity, so effects apply at their base ±10 / ±5.
RV_TEST(sage_uses_base_scroll_magnitudes_identity_path) {
    rng::seed(7);
    auto surge = modelWithScroll('s', ScrollType::SurgePower);
    RV_CHECK(surge->useScroll(Direction::Ea) == true);
    RV_CHECK(surge->effectivePwr() == 23); // 18 + 5 (not doubled)

    auto attune = makeClassModel('s');
    attune->arcanist()->reduceFp(30); // fp = 90
    placeScrollEast(*attune, ScrollType::AttuneFocus);
    RV_CHECK(attune->useScroll(Direction::Ea) == true);
    RV_CHECK(attune->arcanist()->fp() == 100); // 90 + 10 (not +20)
}
