// test_attack.cpp
// RuneVault — Arcanist attack + the no-op case (task 7.5).
//
// Covers Requirements 6.1/6.2/6.3 via Model::handleAttack and the Level Spectre
// API (addSpectre / spectreAt / removeDeadSpectres). Uses the tiny assert-based
// harness (test_util.h); no main() here — tests/test_main.cpp drives run_all().

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/combat.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spectre.h"

#include <memory>

using namespace runevault;

namespace {

// Build a Model with an Arcanist placed at `pos`, and an (empty) Level. The
// board layout is the default fixed layout; interior floor cells hold Spectres.
std::unique_ptr<Model> makeModelAt(const Position &pos, int fp, int pwr,
                                   int wr) {
    auto model = std::unique_ptr<Model>(new Model());
    model->setArcanist(std::make_shared<Arcanist>(fp, pwr, wr));
    model->setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model->setArcanistPosition(pos);
    return model;
}

// A living GenericSpectre with the given stats at `pos`.
std::unique_ptr<GenericSpectre> makeSpectre(const Position &pos, int fp,
                                            int pwr, int wr) {
    return std::unique_ptr<GenericSpectre>(new GenericSpectre(
        SpectreType::Wraith, fp, pwr, wr, /*spawnChamber*/ 1, pos, 'W'));
}

} // namespace

// --- Level Spectre API round-trip ---------------------------------------

RV_TEST(level_spectre_add_and_lookup_round_trip) {
    Level level(makeDefaultBoard());
    RV_CHECK(level.spectres().empty());

    const Position at{2, 3};
    level.addSpectre(makeSpectre(at, 10, 5, 0));

    RV_CHECK(level.spectres().size() == 1u);
    Spectre *found = level.spectreAt(at);
    RV_CHECK(found != nullptr);
    RV_CHECK(found->position() == at);
    // A tile with no Spectre yields nullptr.
    RV_CHECK(level.spectreAt(Position{9, 9}) == nullptr);
}

RV_TEST(level_remove_dead_spectres_erases_only_corpses) {
    Level level(makeDefaultBoard());
    level.addSpectre(makeSpectre(Position{1, 1}, 1, 5, 0)); // will die
    level.addSpectre(makeSpectre(Position{1, 2}, 5, 5, 0)); // survives

    // Kill the first one.
    level.spectreAt(Position{1, 1})->takeDamage(1);
    RV_CHECK(level.spectreAt(Position{1, 1})->isDead());

    level.removeDeadSpectres();
    RV_CHECK(level.spectres().size() == 1u);
    RV_CHECK(level.spectreAt(Position{1, 1}) == nullptr);
    RV_CHECK(level.spectreAt(Position{1, 2}) != nullptr);
}

// --- Requirement 6.1/6.2: attack lands and reduces FP by the formula -----

RV_TEST(attack_adjacent_spectre_reduces_fp_by_computed_damage) {
    // Arcanist at (2,3); target one step East is (2,4).
    const Position arc{2, 3};
    const int arcPwr = 30;
    const int spWr = 50;
    auto model = makeModelAt(arc, /*fp*/ 20, arcPwr, /*wr*/ 0);

    const Position target{2, 4};
    model->level()->addSpectre(makeSpectre(target, /*fp*/ 100, /*pwr*/ 5, spWr));

    const int before = model->level()->spectreAt(target)->fp();
    const bool consumed = model->handleAttack(Direction::Ea);

    RV_CHECK(consumed == true); // turn consumed
    const int expectedDamage = computeDamage(arcPwr, spWr);
    const int after = model->level()->spectreAt(target)->fp();
    RV_CHECK(after == before - expectedDamage);
}

RV_TEST(attack_exact_damage_value_for_known_stats) {
    // pwr=30, wr=50: ceil( (100/150) * 30 ) = ceil(20) = 20.
    RV_CHECK(computeDamage(30, 50) == 20);

    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 30, /*wr*/ 0);
    const Position target{2, 4};
    model->level()->addSpectre(makeSpectre(target, /*fp*/ 100, /*pwr*/ 5, /*wr*/ 50));

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->level()->spectreAt(target)->fp() == 80); // 100 - 20
}

// --- Requirement 6.3: no adjacent Spectre => no-op, no state change ------

RV_TEST(attack_with_no_spectre_is_noop_and_leaves_state_unchanged) {
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 30, /*wr*/ 0);

    // A Spectre East, but we attack West where there is none.
    const Position east{2, 4};
    model->level()->addSpectre(makeSpectre(east, /*fp*/ 100, /*pwr*/ 5, /*wr*/ 50));
    const int eastBefore = model->level()->spectreAt(east)->fp();

    const bool consumed = model->handleAttack(Direction::We);
    RV_CHECK(consumed == false); // no-op, turn not consumed
    // Unrelated Spectre untouched, Arcanist position unchanged.
    RV_CHECK(model->level()->spectreAt(east)->fp() == eastBefore);
    RV_CHECK(model->arcanistPosition() == arc);
}

// --- Cumulative hits + death (corpse removed + drop awarded, task 7.8) ---

RV_TEST(repeated_attacks_reduce_fp_cumulatively_until_dead) {
    const Position arc{2, 3};
    // pwr=30 vs wr=0 => ceil((100/100)*30) = 30 damage per hit.
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 30, /*wr*/ 0);
    const Position target{2, 4};
    model->level()->addSpectre(makeSpectre(target, /*fp*/ 65, /*pwr*/ 5, /*wr*/ 0));

    RV_CHECK(computeDamage(30, 0) == 30);

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->level()->spectreAt(target)->fp() == 35); // 65 - 30

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->level()->spectreAt(target)->fp() == 5); // 35 - 30

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    // Task 7.8: the lethal hit awards the drop (runeDrop()==1) and removes the
    // corpse from the level, so the tile is now empty.
    RV_CHECK(model->level()->spectreAt(target) == nullptr);
    RV_CHECK(model->runeFragmentTotal() == 1);

    // A further attack on the now-empty tile is a no-op.
    RV_CHECK(model->handleAttack(Direction::Ea) == false);
}

// --- Guards: missing Arcanist / Level => no-op ---------------------------

RV_TEST(attack_without_arcanist_or_level_is_noop) {
    Model empty;
    RV_CHECK(empty.handleAttack(Direction::Ea) == false); // no arcanist, no level

    Model onlyArcanist;
    onlyArcanist.setArcanist(std::make_shared<Arcanist>(20, 30, 0));
    RV_CHECK(onlyArcanist.handleAttack(Direction::Ea) == false); // no level
}
