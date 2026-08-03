// test_descent.cpp
// RuneVault — tests for descent / level transition (task 16.1).
//
// These assert-based tests (see tests/test_util.h) exercise Model::descend(),
// the level-transition primitive (Requirements 10.3/10.6/10.7/10.8):
//
//   * descend() with the Cipher Gem NOT carried is a no-op returning false and
//     never changes the level number (Requirement 10.1 gating, 10.3).
//   * a valid descent from level 1 -> 2 (Requirement 10.3): the level number
//     increments, the temporary scroll Decorator chain tears down so effective
//     Pwr returns to base (10.6), permanent FP is preserved (10.7), the carried
//     Cipher Gem state resets while the Aegis Cloak is retained (10.8), and the
//     new level is built and populated (spectres + items present).
//   * descend() from level 5 returns false and does not build a level 6 (the
//     win is handled elsewhere, task 16.3).
//
// The rng is seeded before each descend() since descend() repopulates a level.

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/arcanist_component.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spawner.h"

#include "shuffle.h"

#include <memory>

using namespace runevault;

namespace {

// Build a Model on level 1 with a base Arcanist and a populated Level, ready to
// carry the Cipher Gem and descend. The rng is seeded so the initial populate
// is deterministic.
Model levelOneModel(unsigned long seed) {
    rng::seed(seed);
    Model model;
    model.setArcanist(std::make_shared<Arcanist>(100, 20, 10));
    model.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    Spawner spawner;
    spawner.populate(model);
    return model;
}

} // namespace

// Requirement 10.1/10.3: without the Cipher Gem, descend() is a no-op.
RV_TEST(descent_without_gem_is_noop) {
    Model model = levelOneModel(1234);
    RV_CHECK(!model.carryingCipherGem());
    const int before = model.levelNumber();

    rng::seed(42);
    const bool descended = model.descend();

    RV_CHECK(!descended);
    RV_CHECK(model.levelNumber() == before); // level unchanged
}

// Requirements 10.3/10.6/10.7/10.8: a valid descent from level 1 -> 2.
RV_TEST(descent_transition_teardown_and_preservation) {
    Model model = levelOneModel(2024);

    // Carry the current level's Cipher Gem so descent is allowed.
    model.setCarryingCipherGem(true);
    // Retain-across-descent check: the Aegis Cloak was collected earlier.
    model.setCarryingAegisCloak(true);

    // Requirement 10.7 setup: make a PERMANENT FP change on the base Arcanist
    // (a Drain-style reduction) and remember the resulting FP.
    model.arcanist()->reduceFp(30); // 100 -> 70 permanent
    const int fpBefore = model.arcanist()->fp();
    RV_CHECK(fpBefore == 70);

    // Requirement 10.6 setup: apply a TEMPORARY scroll effect (Surge Power)
    // by wrapping the active Decorator chain, so effective Pwr exceeds base.
    const int basePwr = model.arcanist()->pwr(); // 20
    model.setActiveArcanist(
        std::make_shared<SurgePower>(model.activeArcanistPtr(), 5));
    RV_CHECK(model.effectivePwr() == basePwr + 5); // temporary buff active

    const int levelBefore = model.levelNumber(); // 1

    // Descend. Seed rng first since descend() repopulates the next level.
    rng::seed(7);
    const bool descended = model.descend();

    RV_CHECK(descended);
    // Requirement 10.3: advanced to the next level.
    RV_CHECK(model.levelNumber() == levelBefore + 1); // 2
    // Requirement 10.6: temporary scroll effect torn down — effective Pwr back
    // to base.
    RV_CHECK(model.effectivePwr() == basePwr);
    // Requirement 10.7: permanent FP preserved (descend did not touch it).
    RV_CHECK(model.arcanist()->fp() == fpBefore);
    // Requirement 10.8: carried Cipher Gem reset; Aegis Cloak retained.
    RV_CHECK(!model.carryingCipherGem());
    RV_CHECK(model.carryingAegisCloak());
    RV_CHECK(!model.cipherGemDropped());

    // A new level exists and was populated by the Spawner: stairway hidden,
    // spectres and items present.
    RV_CHECK(model.level() != nullptr);
    RV_CHECK(!model.level()->stairwayRevealed());
    RV_CHECK(!model.level()->spectres().empty());
    RV_CHECK(!model.level()->items().empty());
}

// Requirement 10.4 boundary (win handled elsewhere): descend() from level 5 is
// refused and does not build a level 6.
RV_TEST(descent_from_level_five_returns_false) {
    Model model = levelOneModel(99);
    model.setCarryingCipherGem(true);
    model.setLevelNumber(5);

    rng::seed(3);
    const bool descended = model.descend();

    RV_CHECK(!descended);
    RV_CHECK(model.levelNumber() == 5); // no level 6 built
}
