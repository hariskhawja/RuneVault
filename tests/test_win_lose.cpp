// test_win_lose.cpp
// RuneVault — win and lose conditions (task 16.3, Requirements 10.1/10.2/10.4/10.5).
//
// Exercises the stairway walk-over descent/win trigger and the loss-on-zero-FP
// path wired onto the Model:
//   * 10.1/10.2: walking onto the stairway WITHOUT the Cipher Gem has no effect
//     — no descent, no win, run stays Playing, level number unchanged.
//   * 10.3: stepping onto the stairway WITH the gem on levels 1..4 descends
//     (level number advances; run stays Playing).
//   * 10.4: stepping onto the stairway WITH the gem on level 5 ends the run as a
//     WIN and does NOT build a level 6.
//   * 10.5: the Arcanist's FP reaching 0 (via Spectre attacks) ends the run as a
//     loss (runState() == RunState::Lost / isRunLost()).
//
// Uses the tiny assert-based harness (test_util.h); no main() here —
// tests/test_main.cpp drives run_all(). rng is seeded where a level is
// repopulated (descend) or a 50% roll is taken (Spectre attack).

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/arcanist_factory.h"
#include "runevault/board.h"
#include "runevault/geometry.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spectre.h"
#include "shuffle.h" // rng::seed

#include <memory>

namespace {

using namespace runevault;

// Build a Model owning a default-layout level with the Arcanist at `p` and the
// stairway placed one step East at `p + Ea` (an interior floor tile in chamber
// 1 when `p` is a central interior tile), so a single East move lands the
// Arcanist exactly on the stairway tile.
Model makeStairwayModel(const Position &p) {
    Model model;
    model.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model.setArcanistPosition(p);
    model.level()->setStairway(Geometry::step(p, Direction::Ea));
    return model;
}

// A plain hostile Spectre with the given Power (used to drive the loss path).
GenericSpectre makeSpectre(int pwr) {
    return GenericSpectre(SpectreType::Wraith, /*fp=*/10, pwr, /*wr=*/0,
                          /*spawnChamber=*/1, Position{0, 0}, 'W');
}

} // namespace

// --- Requirements 10.1/10.2: stairway walk-over without the gem is a no-op ---
RV_TEST(walk_onto_stairway_without_gem_has_no_effect) {
    const Position start{2, 5};
    Model m = makeStairwayModel(start);
    RV_CHECK(!m.carryingCipherGem());
    RV_CHECK(m.levelNumber() == 1);

    // Step East onto the stairway tile. The move itself succeeds (walkable
    // floor), but with no gem there is no descent and no win.
    RV_CHECK(m.moveArcanist(Direction::Ea) == true);
    RV_CHECK(m.arcanistPosition() == Geometry::step(start, Direction::Ea));
    RV_CHECK(m.onStairway());                          // standing on the stairway
    RV_CHECK(m.levelNumber() == 1);                    // 10.1: no descent
    RV_CHECK(m.runState() == RunState::Playing);       // 10.2: no effect
}

// --- Requirement 10.2 (primitive): tryDescend is a no-op without the gem -----
RV_TEST(try_descend_without_gem_returns_false) {
    Model m = makeStairwayModel(Position{2, 5});
    // Place the Arcanist directly on the stairway but withhold the gem.
    m.setArcanistPosition(m.level()->stairway());
    RV_CHECK(m.onStairway());
    RV_CHECK(!m.carryingCipherGem());

    RV_CHECK(m.tryDescend() == false);
    RV_CHECK(m.levelNumber() == 1);
    RV_CHECK(m.runState() == RunState::Playing);
}

// --- Requirement 10.1 (primitive): tryDescend is a no-op off the stairway ----
RV_TEST(try_descend_off_stairway_returns_false) {
    Model m = makeStairwayModel(Position{2, 5});
    // Arcanist is at (2,5); stairway is at (2,6) — not on the stairway.
    m.setCarryingCipherGem(true);
    RV_CHECK(!m.onStairway());

    RV_CHECK(m.tryDescend() == false);
    RV_CHECK(m.levelNumber() == 1);
    RV_CHECK(m.runState() == RunState::Playing);
}

// --- Requirement 10.3: descend on levels 1..4 with the gem advances a level --
RV_TEST(walk_onto_stairway_with_gem_descends_on_level_one) {
    rng::seed(4242); // descend() repopulates the next level via the Spawner
    const Position start{2, 5};
    Model m = makeStairwayModel(start);
    m.setArcanist(std::make_shared<Arcanist>(30, 5, 3));
    m.setCarryingCipherGem(true);
    RV_CHECK(m.levelNumber() == 1);

    // Step East onto the stairway while carrying the gem -> descent.
    RV_CHECK(m.moveArcanist(Direction::Ea) == true);
    RV_CHECK(m.levelNumber() == 2);                 // advanced to level 2
    RV_CHECK(m.runState() == RunState::Playing);    // not a win yet
    // 10.8: the carried-gem state is reset for the new level.
    RV_CHECK(!m.carryingCipherGem());
}

// tryDescend on levels 1..4 (direct call) advances the level too.
RV_TEST(try_descend_on_level_three_advances_to_level_four) {
    rng::seed(777);
    Model m = makeStairwayModel(Position{2, 5});
    m.setArcanist(std::make_shared<Arcanist>(30, 5, 3));
    m.setLevelNumber(3);
    m.setArcanistPosition(m.level()->stairway());
    m.setCarryingCipherGem(true);

    RV_CHECK(m.tryDescend() == true);
    RV_CHECK(m.levelNumber() == 4);
    RV_CHECK(m.runState() == RunState::Playing);
}

// --- Requirement 10.4: descend on level 5 with the gem WINS the run ----------
RV_TEST(walk_onto_stairway_with_gem_on_level_five_wins) {
    const Position start{2, 5};
    Model m = makeStairwayModel(start);
    m.setArcanist(ArcanistFactory::create('s')); // Sage (for a scored win)
    m.setLevelNumber(kChamberCount);             // level 5
    m.setCarryingCipherGem(true);
    m.setRuneFragmentTotal(100);

    // Step East onto the stairway on level 5 -> WIN (no level 6 built).
    RV_CHECK(m.moveArcanist(Direction::Ea) == true);
    RV_CHECK(m.runState() == RunState::Won);
    RV_CHECK(m.levelNumber() == kChamberCount);  // still level 5, no level 6
    // A won run has a score (Sage: 100 + 50% = 150).
    RV_CHECK(m.hasScore());
    RV_CHECK(m.finalScore() == 150);
}

// tryDescend on level 5 (direct call) wins without building a level 6.
RV_TEST(try_descend_on_level_five_sets_won) {
    Model m = makeStairwayModel(Position{2, 5});
    m.setArcanist(std::make_shared<Arcanist>(30, 5, 3));
    m.setLevelNumber(kChamberCount);
    m.setArcanistPosition(m.level()->stairway());
    m.setCarryingCipherGem(true);

    RV_CHECK(m.tryDescend() == true);
    RV_CHECK(m.runState() == RunState::Won);
    RV_CHECK(m.levelNumber() == kChamberCount);
}

// --- Requirement 10.5: FP reaching 0 ends the run as a loss ------------------
RV_TEST(fp_reaching_zero_ends_run_as_loss) {
    rng::seed(999);
    // 1 FP: any single landed hit (>= 1 damage) is lethal.
    Model m;
    m.setArcanist(std::make_shared<Arcanist>(/*fp=*/1, /*pwr=*/5, /*wr=*/0));
    GenericSpectre s = makeSpectre(20);

    RV_CHECK(m.runState() == RunState::Playing);
    RV_CHECK(!m.isRunLost());

    // Apply attacks until a hit lands (50% miss). Bounded so the test always
    // terminates even on an unlucky seeded streak of misses.
    bool hit = false;
    for (int i = 0; i < 1000 && !hit; ++i) {
        hit = m.resolveSpectreAttack(s);
    }
    RV_CHECK(hit);
    RV_CHECK(m.arcanist()->isDead());
    RV_CHECK(m.runState() == RunState::Lost);
    RV_CHECK(m.isRunLost());
}
