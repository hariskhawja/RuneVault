// test_hostility.cpp
// RuneVault — Spectre hostility resolution + attack-instead-of-move wiring
// (task 10.5, Requirements 6.4/7.5/7.6/7.7/7.8/7.9).
//
// Coverage:
//   * Lich hostility is driven by the global lichAggroTriggered flag
//     (Requirements 7.7/7.8/7.9): non-hostile while clear, hostile once set.
//     An adjacent Lich does no harm while the flag is clear, but attacks (FP
//     drops over seeded iterations) once the flag is set.
//   * Vault Anchor hostility depends ONLY on the Arcanist being adjacent to the
//     Coffer it guards (Requirements 7.5/7.6): hostile when adjacent, otherwise
//     non-hostile — and a hostile adjacent Anchor attacks even though it is
//     stationary, while a non-hostile Anchor never does.
//   * An ordinary Spectre (Wraith) adjacent to the Arcanist is hostile and
//     attacks instead of moving (Requirement 6.4): FP drops over seeded turns.
//   * A hostile Spectre that is NOT adjacent wanders (task 10.3) and never
//     damages the Arcanist.

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/board.h"
#include "runevault/geometry.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spectre.h"
#include "shuffle.h"

#include <memory>

using namespace runevault;

namespace {

// Build a Model owning a default-layout level, with an Arcanist (given FP,
// plenty by default so seeded runs never accidentally end) placed at `p`.
Model makeModelWithArcanistAt(const Position &p, int fp = 1000000) {
    Model m;
    m.setArcanist(std::make_shared<Arcanist>(fp, /*pwr=*/20, /*wr=*/0));
    m.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    m.setArcanistPosition(p);
    return m;
}

// Run advanceSpectres up to `maxTurns` times and report whether the Arcanist's
// FP ever dropped below its starting value (i.e. a Spectre landed a hit).
bool arcanistTookDamageOver(Model &m, int maxTurns) {
    const int start = m.arcanist()->fp();
    for (int i = 0; i < maxTurns; ++i) {
        m.advanceSpectres();
        if (m.arcanist()->fp() < start) {
            return true;
        }
    }
    return false;
}

} // namespace

// --- Lich hostility is flag-driven (Requirements 7.7/7.8/7.9) ---------------

RV_TEST(lich_hostility_follows_aggro_flag) {
    Model m = makeModelWithArcanistAt(Position{2, 5});
    Lich lich(/*spawnChamber=*/1, Position{2, 6});

    // Flag clear (default): every Lich is non-hostile (Requirement 7.9).
    RV_CHECK(m.lichAggroTriggered() == false);
    RV_CHECK(m.isHostile(lich) == false);

    // Flag set: every Lich becomes hostile (Requirement 7.8).
    m.setLichAggroTriggered(true);
    RV_CHECK(m.isHostile(lich) == true);
}

RV_TEST(dormant_lich_adjacent_does_not_attack) {
    rng::seed(4242);
    // Arcanist at (2,5); a Lich right beside it at (2,6), but aggro flag clear.
    Model m = makeModelWithArcanistAt(Position{2, 5});
    m.level()->addSpectre(std::unique_ptr<Spectre>(new Lich(1, Position{2, 6})));

    // Requirement 7.9: while dormant the Lich never attacks — FP stays full.
    RV_CHECK(arcanistTookDamageOver(m, 200) == false);
    RV_CHECK(m.arcanist()->fp() == 1000000);
    RV_CHECK(m.runState() == RunState::Playing);
}

RV_TEST(aggroed_lich_adjacent_attacks) {
    rng::seed(4242);
    Model m = makeModelWithArcanistAt(Position{2, 5});
    m.level()->addSpectre(std::unique_ptr<Spectre>(new Lich(1, Position{2, 6})));

    // Requirement 7.8: once aggro is triggered an adjacent Lich attacks instead
    // of moving, so the Arcanist takes damage over seeded turns.
    m.setLichAggroTriggered(true);
    RV_CHECK(arcanistTookDamageOver(m, 200) == true);
}

// --- Vault Anchor hostility is Coffer-adjacency driven (Req. 7.5/7.6) -------

RV_TEST(vault_anchor_hostile_only_when_arcanist_adjacent_to_coffer) {
    const Position coffer{2, 5};
    VaultAnchor anchor(/*spawnChamber=*/1, Position{2, 6});
    anchor.setGuardedItemPosition(coffer);

    // Arcanist adjacent to the guarded Coffer -> hostile (Requirement 7.5).
    Model near = makeModelWithArcanistAt(Position{2, 6}); // adjacent to (2,5)
    RV_CHECK(Geometry::isAdjacent(near.arcanistPosition(), coffer));
    RV_CHECK(near.isHostile(anchor) == true);

    // Arcanist NOT adjacent to the Coffer -> non-hostile (Requirement 7.6).
    Model far = makeModelWithArcanistAt(Position{4, 9}); // far from (2,5)
    RV_CHECK(Geometry::isAdjacent(far.arcanistPosition(), coffer) == false);
    RV_CHECK(far.isHostile(anchor) == false);
}

RV_TEST(vault_anchor_defaults_to_guarding_own_tile) {
    // With no explicit Coffer wired, an Anchor guards its own spawn tile.
    VaultAnchor anchor(/*spawnChamber=*/1, Position{2, 6});
    RV_CHECK(anchor.guardedItemPosition() == (Position{2, 6}));

    Model beside = makeModelWithArcanistAt(Position{2, 5}); // adjacent to (2,6)
    RV_CHECK(beside.isHostile(anchor) == true);

    Model away = makeModelWithArcanistAt(Position{4, 9});
    RV_CHECK(away.isHostile(anchor) == false);
}

RV_TEST(hostile_vault_anchor_attacks_despite_being_stationary) {
    rng::seed(77);
    const Position coffer{2, 5};
    // Arcanist at (2,6): adjacent to the Coffer (2,5) AND to the Anchor (2,7).
    Model m = makeModelWithArcanistAt(Position{2, 6});
    auto anchor = std::unique_ptr<VaultAnchor>(new VaultAnchor(1, Position{2, 7}));
    anchor->setGuardedItemPosition(coffer);
    m.level()->addSpectre(std::move(anchor));

    // Requirement 7.5: the Anchor is hostile (Arcanist adjacent to its Coffer)
    // and, though stationary, attacks — FP drops over seeded turns.
    RV_CHECK(arcanistTookDamageOver(m, 200) == true);
}

RV_TEST(non_hostile_vault_anchor_never_attacks) {
    rng::seed(77);
    const Position coffer{2, 1};
    // Arcanist at (2,6) is adjacent to the Anchor (2,7) but NOT to the Coffer
    // (2,1), so the Anchor is non-hostile (Requirement 7.6) and stays passive.
    Model m = makeModelWithArcanistAt(Position{2, 6});
    auto anchor = std::unique_ptr<VaultAnchor>(new VaultAnchor(1, Position{2, 7}));
    anchor->setGuardedItemPosition(coffer);
    m.level()->addSpectre(std::move(anchor));

    RV_CHECK(arcanistTookDamageOver(m, 200) == false);
    RV_CHECK(m.arcanist()->fp() == 1000000);
}

// --- Ordinary Spectre: hostile, attacks instead of moving (Req. 6.4) --------

RV_TEST(adjacent_wraith_attacks_instead_of_moving) {
    rng::seed(31337);
    Model m = makeModelWithArcanistAt(Position{2, 5});
    // A Wraith is always hostile; place it adjacent to the Arcanist.
    Wraith wraith(1, Position{2, 6});
    RV_CHECK(m.isHostile(wraith) == true);

    m.level()->addSpectre(std::unique_ptr<Spectre>(new Wraith(1, Position{2, 6})));
    RV_CHECK(arcanistTookDamageOver(m, 200) == true);
}

RV_TEST(distant_hostile_wraith_wanders_and_deals_no_damage) {
    rng::seed(31337);
    // Arcanist sits in chamber 2 (interior cols 20..28); the Wraith is confined
    // to chamber 1 (interior cols 1..9) and can NEVER leave its spawn chamber
    // (task 10.3), so it can never become adjacent to the Arcanist. It wanders
    // instead of attacking, and the Arcanist takes no damage.
    Model m = makeModelWithArcanistAt(Position{2, 24}); // chamber 2 interior
    m.level()->addSpectre(std::unique_ptr<Spectre>(new Wraith(1, Position{2, 5})));

    // Track whether the Wraith actually moves (proving it wanders, not freezes).
    const Position start = m.level()->spectres().front()->position();
    RV_CHECK(arcanistTookDamageOver(m, 200) == false);
    RV_CHECK(m.arcanist()->fp() == 1000000);

    bool wandered = false;
    for (int i = 0; i < 50 && !wandered; ++i) {
        m.advanceSpectres();
        if (m.level()->spectres().front()->position() != start) {
            wandered = true;
        }
    }
    RV_CHECK(wandered); // the distant hostile Wraith wanders (Requirement 7.2)
}
