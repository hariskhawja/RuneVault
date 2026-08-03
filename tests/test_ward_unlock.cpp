// test_ward_unlock.cpp
// RuneVault — Vault Anchor death unlocks guarded items, Cipher Gem drop/pickup
// lifts the Ward, and the Aegis Cloak halves damage end-to-end (task 13.3).
//
// Covers:
//   * Requirement 9.2/9.5 — defeating the Vault Anchor guarding a Shard Coffer
//     transitions that coffer Locked -> Unlocked, after which it can be
//     collected for its 6 Rune Fragments.
//   * Requirement 9.2/9.3/10.1/10.2 — a defeated Cipher Gem carrier drops a
//     CipherGem Item on its tile; walking onto that tile lifts the Ward
//     (carryingCipherGem) and reveals the stairway, consuming the gem.
//   * Requirement 9.8 — once the (unlocked) Aegis Cloak is collected the
//     Arcanist carries it and a Spectre's hit damage is halved (rounded up).
//   * Guard: a Locked guarded item is NOT collected by walking over it before
//     its Anchor dies.
//
// **Validates: Requirements 9.2, 9.3, 9.8, 10.1, 10.2**
//
// Uses the tiny assert-based harness (test_util.h); no main() here —
// tests/test_main.cpp drives run_all().

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/combat.h"
#include "runevault/guarded_item.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spectre.h"

#include <memory>

using namespace runevault;

namespace {

// Build a Model with an Arcanist at `pos` and an (empty) default-layout Level.
// Chamber 1's floor interior spans rows 1..4, cols 1..9 (see test_movement).
std::unique_ptr<Model> makeModelAt(const Position &pos, int fp, int pwr,
                                   int wr) {
    auto model = std::unique_ptr<Model>(new Model());
    model->setArcanist(std::make_shared<Arcanist>(fp, pwr, wr));
    model->setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model->setArcanistPosition(pos);
    return model;
}

} // namespace

// --- 9.2/9.5: Anchor death unlocks an adjacent Shard Coffer ------------------

RV_TEST(anchor_death_unlocks_adjacent_shard_coffer_and_awards_six_rf) {
    // Arcanist at (2,3); Vault Anchor one step East at (2,4). High Power so the
    // single hit is lethal to the 120-FP Anchor (Anchor WR 18).
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 30, /*pwr*/ 200, /*wr*/ 0);

    const Position anchorPos{2, 4};
    model->level()->addSpectre(
        std::unique_ptr<Spectre>(new VaultAnchor(/*spawnChamber*/ 1, anchorPos)));

    // A locked Shard Coffer within a 1-block radius of the Anchor (3,4).
    const Position cofferPos{3, 4};
    auto coffer = std::unique_ptr<ShardCoffer>(new ShardCoffer());
    coffer->setPosition(cofferPos);
    ShardCoffer *cofferPtr = coffer.get();
    model->level()->addItem(std::move(coffer));

    RV_CHECK(cofferPtr->isLocked() == true);   // guarded before the Anchor dies
    RV_CHECK(model->runeFragmentTotal() == 0);

    // Kill the Anchor: the CombatVisitor fires on the (lethal) attack and
    // unlocks the guarded coffer (Requirement 9.5). Anchor drops 0 RF.
    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->level()->spectreAt(anchorPos) == nullptr); // corpse removed
    RV_CHECK(cofferPtr->isUnlocked() == true);                 // now Unlocked
    RV_CHECK(model->runeFragmentTotal() == 0);                 // not yet collected

    // Collecting the now-Unlocked coffer awards its 6 Rune Fragments (9.5).
    RV_CHECK(cofferPtr->tryCollect(*model) == true);
    RV_CHECK(model->runeFragmentTotal() == 6);
}

// --- 9.2/9.3/10.1/10.2: Cipher Gem drop then walk-over lifts the Ward ---------

RV_TEST(cipher_gem_drop_pickup_reveals_stairway_and_carries_gem) {
    const Position arc{2, 3};
    // pwr 50 vs WR 0 => 50 damage one-shots the 40-FP carrier.
    auto model = makeModelAt(arc, /*fp*/ 30, /*pwr*/ 50, /*wr*/ 0);
    model->level()->setStairway(Position{1, 1});

    const Position carrierPos{2, 4};
    auto carrier = std::unique_ptr<GenericSpectre>(new GenericSpectre(
        SpectreType::Wraith, /*fp*/ 40, /*pwr*/ 5, /*wr*/ 0, /*chamber*/ 1,
        carrierPos, 'W'));
    carrier->setCarriesCipherGem(true);
    model->level()->addSpectre(std::move(carrier));

    RV_CHECK(model->carryingCipherGem() == false);
    RV_CHECK(model->level()->stairwayRevealed() == false);

    // Kill the carrier: a CipherGem Item is placed on its tile (Requirement
    // 6.12/9.2) and the model records the drop.
    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->cipherGemDropped() == true);
    RV_CHECK(model->cipherGemDropPosition() == carrierPos);
    Item *dropped = model->level()->itemAt(carrierPos);
    RV_CHECK(dropped != nullptr);
    RV_CHECK(dynamic_cast<CipherGem *>(dropped) != nullptr);

    // Walk onto the gem's tile: it lifts the Ward (carry) and reveals the
    // stairway (Requirements 9.3/10.1/10.2), and the gem is consumed.
    RV_CHECK(model->moveArcanist(Direction::Ea) == true);
    RV_CHECK(model->arcanistPosition() == carrierPos);
    RV_CHECK(model->carryingCipherGem() == true);
    RV_CHECK(model->level()->stairwayRevealed() == true);
    RV_CHECK(model->level()->itemAt(carrierPos) == nullptr); // gem removed
}

// --- 9.8: carrying the (unlocked) Aegis Cloak halves incoming damage ---------

RV_TEST(aegis_cloak_collected_halves_incoming_spectre_damage) {
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 100, /*pwr*/ 10, /*wr*/ 0);

    // A hostile Spectre whose base damage we can predict: pwr 40 vs the
    // Arcanist's effective WR 0 => base = ceil(100/100 * 40) = 40.
    GenericSpectre attacker(SpectreType::Wraith, /*fp*/ 40, /*pwr*/ 40, /*wr*/ 0,
                            /*chamber*/ 1, Position{2, 5}, 'W');
    const int base = computeDamage(attacker.pwr(), model->effectiveWr());
    const int unmitigated = applyDamageTakenOrdering(base, /*isWarden*/ false,
                                                     /*carryingAegis*/ false);
    const int halved = applyDamageTakenOrdering(base, /*isWarden*/ false,
                                                /*carryingAegis*/ true);

    // Before collection: no Aegis, damage is unmitigated.
    RV_CHECK(model->carryingAegisCloak() == false);
    RV_CHECK(model->spectreDamageIfHit(attacker) == unmitigated);

    // Unlock (as if the guarding Anchor died) then collect the Aegis Cloak.
    auto cloak = std::unique_ptr<AegisCloak>(new AegisCloak());
    RV_CHECK(cloak->isLocked() == true);
    cloak->onAnchorDeath();
    RV_CHECK(cloak->isUnlocked() == true);
    RV_CHECK(cloak->tryCollect(*model) == true);

    // Requirement 9.8: the Arcanist now carries the Aegis and takes halved
    // damage (rounded up), strictly less than the unmitigated value.
    RV_CHECK(model->carryingAegisCloak() == true);
    RV_CHECK(model->spectreDamageIfHit(attacker) == halved);
    RV_CHECK(halved < unmitigated);
}

// --- Q4 Follow-up: standing ON the Coffer when the Anchor dies collects it ---

RV_TEST(standing_on_coffer_when_anchor_dies_collects_immediately) {
    // Piazza Q4 Follow-up: if the Arcanist is already standing on the Coffer
    // tile when the guarding Anchor is defeated, the Coffer is collected
    // IMMEDIATELY (no need to step off and back on).
    //
    // Arcanist stands ON the Coffer at (2,4); the guarding Anchor is one step
    // East at (2,5) (adjacent to the Coffer, so within its unlock radius).
    const Position cofferPos{2, 4};
    auto model = makeModelAt(cofferPos, /*fp*/ 30, /*pwr*/ 200, /*wr*/ 0);

    auto coffer = std::unique_ptr<ShardCoffer>(new ShardCoffer());
    coffer->setPosition(cofferPos);
    ShardCoffer *cofferPtr = coffer.get();
    model->level()->addItem(std::move(coffer));

    const Position anchorPos{2, 5};
    model->level()->addSpectre(
        std::unique_ptr<Spectre>(new VaultAnchor(/*spawnChamber*/ 1, anchorPos)));

    RV_CHECK(cofferPtr->isLocked() == true);
    RV_CHECK(model->runeFragmentTotal() == 0);

    // Kill the Anchor while standing on the Coffer: the CombatVisitor unlocks
    // the Coffer and, seeing the Arcanist on its tile, collects it in place.
    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->runeFragmentTotal() == 6);              // collected at once
    RV_CHECK(model->level()->itemAt(cofferPos) == nullptr); // item removed
}

// --- Guard: a Locked guarded item is NOT collected by walk-over --------------

RV_TEST(locked_guarded_item_is_not_collected_by_walkover) {
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 30, /*pwr*/ 10, /*wr*/ 0);

    const Position cofferPos{2, 4}; // one step East, chamber-1 floor
    auto coffer = std::unique_ptr<ShardCoffer>(new ShardCoffer());
    coffer->setPosition(cofferPos);
    model->level()->addItem(std::move(coffer));

    // Walking onto the Locked coffer occupies the tile but collects nothing.
    RV_CHECK(model->moveArcanist(Direction::Ea) == true);
    RV_CHECK(model->arcanistPosition() == cofferPos);
    RV_CHECK(model->runeFragmentTotal() == 0);              // no RF awarded

    Item *still = model->level()->itemAt(cofferPos);
    RV_CHECK(still != nullptr);                              // item still present
    ShardCoffer *cofferPtr = dynamic_cast<ShardCoffer *>(still);
    RV_CHECK(cofferPtr != nullptr);
    RV_CHECK(cofferPtr->isLocked() == true);                // still Locked
    RV_CHECK(cofferPtr->collected() == false);
}
