// test_combat_visitor.cpp
// RuneVault — CombatVisitor double-dispatch + Lich aggro trigger (task 10.2).
//
// Covers Requirement 7.7 (attacking any Lich trips the run-scoped
// lichAggroTriggered flag, DD-2) and the Requirement 6.12 Vault Anchor unlock
// HOOK (a documented no-op until task 13.3). Exercises both the wired path
// (Model::handleAttack) and direct visitor dispatch (spectre->accept(visitor)).
//
// Uses the tiny assert-based harness (test_util.h); no main() here —
// tests/test_main.cpp drives run_all().

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/combat_visitor.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spectre.h"

#include <memory>

using namespace runevault;

namespace {

// Build a Model with an Arcanist at `pos` and an (empty) default-layout Level.
std::unique_ptr<Model> makeModelAt(const Position &pos, int fp, int pwr,
                                   int wr) {
    auto model = std::unique_ptr<Model>(new Model());
    model->setArcanist(std::make_shared<Arcanist>(fp, pwr, wr));
    model->setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model->setArcanistPosition(pos);
    return model;
}

} // namespace

// --- Requirement 7.7: attacking a Lich trips the global aggro flag -----------

RV_TEST(attacking_lich_trips_lich_aggro_flag) {
    // Arcanist at (2,3); target one step East is (2,4). Give the Arcanist low
    // Power so the hit is NON-LETHAL (Lich has fp 30) — aggro must trip anyway.
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 30, /*pwr*/ 1, /*wr*/ 0);

    const Position target{2, 4};
    model->level()->addSpectre(
        std::unique_ptr<Spectre>(new Lich(/*spawnChamber*/ 1, target)));

    RV_CHECK(model->lichAggroTriggered() == false); // false before the attack

    const bool consumed = model->handleAttack(Direction::Ea);
    RV_CHECK(consumed == true);

    // The Lich survived a non-lethal hit but aggro is now permanently set.
    RV_CHECK(model->level()->spectreAt(target) != nullptr);
    RV_CHECK(model->level()->spectreAt(target)->isDead() == false);
    RV_CHECK(model->lichAggroTriggered() == true);
}

// --- Attacking a non-Lich does NOT set the aggro flag ------------------------

RV_TEST(attacking_non_lich_does_not_trip_lich_aggro_flag) {
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 30, /*pwr*/ 1, /*wr*/ 0);

    const Position target{2, 4};
    model->level()->addSpectre(
        std::unique_ptr<Spectre>(new Wraith(/*spawnChamber*/ 1, target)));

    RV_CHECK(model->lichAggroTriggered() == false);
    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->lichAggroTriggered() == false); // unchanged after a Wraith hit
}

// --- Direct visitor dispatch: Lich sets the flag, Wraith leaves it alone -----

RV_TEST(direct_visitor_dispatch_lich_sets_flag_wraith_unchanged) {
    auto model = makeModelAt(Position{2, 3}, /*fp*/ 30, /*pwr*/ 10, /*wr*/ 0);
    CombatVisitor visitor(*model);

    Wraith wraith(/*spawnChamber*/ 1, Position{2, 4});
    wraith.accept(visitor);
    RV_CHECK(model->lichAggroTriggered() == false); // Wraith visit is a no-op

    Lich lich(/*spawnChamber*/ 1, Position{2, 4});
    lich.accept(visitor);
    RV_CHECK(model->lichAggroTriggered() == true); // Lich visit trips the flag
}

// --- Requirement 6.12 hook: VaultAnchor visit is a safe no-op for now --------
// TODO(task 13.3): this will transition the Anchor's guarded item(s) to
// Unlocked. Until then accept() on a VaultAnchor must be crash-free and have no
// observable effect.

RV_TEST(vault_anchor_visit_is_safe_noop_for_now) {
    auto model = makeModelAt(Position{2, 3}, /*fp*/ 30, /*pwr*/ 10, /*wr*/ 0);
    CombatVisitor visitor(*model);

    VaultAnchor anchor(/*spawnChamber*/ 1, Position{2, 4});
    anchor.accept(visitor); // must not crash and must not change run flags

    RV_CHECK(model->lichAggroTriggered() == false);
    RV_CHECK(model->cipherGemDropped() == false);
}
