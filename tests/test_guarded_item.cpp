// test_guarded_item.cpp
// RuneVault — guarded-item State machine and concrete items (task 13.2).
//
// Covers the collection-gating behaviour of the State pattern (Locked ->
// Unlocked) and the three concrete guarded items:
//   * Requirement 9.4: while Locked, collection is prevented (no effect).
//   * Requirement 9.5: once the guarding Anchor dies (onAnchorDeath), the Shard
//     Coffer unlocks and collecting it awards 6 Rune Fragments; a second
//     collect is a no-op (no double-award).
//   * Requirement 9.7: the Aegis Cloak is gated the same way and sets the carry
//     flag once Unlocked.
//   * Requirement 9.3: a dropped Cipher Gem starts Unlocked and is collectible
//     immediately, marking it carried and revealing the stairway.
//   * DD-4: walking onto a guarded tile does nothing while Locked and collects
//     once Unlocked (onWalkOver mirrors tryCollect).
//   * DD-7 glyphs: Shard Coffer `$`, Aegis Cloak `G`, Cipher Gem `C`.
//
// **Validates: Requirements 9.3, 9.4, 9.5, 9.7**

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/guarded_item.h"
#include "runevault/level.h"
#include "runevault/model.h"

#include <memory>

namespace {

using namespace runevault;

// Build a Model with an Arcanist and a default-board Level, so effects that
// touch the level (CipherGem stairway reveal) have somewhere to act.
Model makeModel() {
    Model m;
    m.setArcanist(std::make_shared<Arcanist>(/*fp*/ 20, /*pwr*/ 10, /*wr*/ 5));
    m.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    m.setArcanistPosition(Position{3, 6}); // a chamber-1 floor tile
    return m;
}

} // namespace

// --- Shard Coffer -----------------------------------------------------------

// Requirement 9.4/9.5: locked coffer can't be collected; after onAnchorDeath it
// can, awarding 6 RF; a second collect is a no-op.
RV_TEST(guarded_shard_coffer_locked_then_unlocked_awards_six) {
    Model m = makeModel();
    ShardCoffer coffer;

    RV_CHECK(coffer.isLocked() == true);
    RV_CHECK(coffer.isUnlocked() == false);
    RV_CHECK(coffer.value() == 6);

    const int before = m.runeFragmentTotal();

    // Requirement 9.4: locked -> collection prevented, total unchanged.
    RV_CHECK(coffer.tryCollect(m) == false);
    RV_CHECK(m.runeFragmentTotal() == before);
    RV_CHECK(coffer.collected() == false);

    // Requirement 9.5: unlock, then collect adds 6.
    coffer.onAnchorDeath();
    RV_CHECK(coffer.isUnlocked() == true);
    RV_CHECK(coffer.tryCollect(m) == true);
    RV_CHECK(m.runeFragmentTotal() == before + 6);
    RV_CHECK(coffer.collected() == true);

    // Second collect: no-op, no double-add.
    RV_CHECK(coffer.tryCollect(m) == false);
    RV_CHECK(m.runeFragmentTotal() == before + 6);
}

RV_TEST(guarded_shard_coffer_symbol_is_dollar) {
    ShardCoffer coffer;
    RV_CHECK(coffer.symbol() == '$');
}

// --- Aegis Cloak ------------------------------------------------------------

// Requirement 9.7: locked cloak can't be collected and does not set the carry
// flag; after onAnchorDeath collection sets carryingAegisCloak.
RV_TEST(guarded_aegis_cloak_locked_then_unlocked_sets_carry_flag) {
    Model m = makeModel();
    AegisCloak cloak;

    RV_CHECK(cloak.isLocked() == true);
    RV_CHECK(m.carryingAegisCloak() == false);

    RV_CHECK(cloak.tryCollect(m) == false);
    RV_CHECK(m.carryingAegisCloak() == false);

    cloak.onAnchorDeath();
    RV_CHECK(cloak.tryCollect(m) == true);
    RV_CHECK(m.carryingAegisCloak() == true);
}

RV_TEST(guarded_aegis_cloak_symbol_is_g) {
    AegisCloak cloak;
    RV_CHECK(cloak.symbol() == 'G');
}

// --- Cipher Gem -------------------------------------------------------------

// Requirement 9.3: a dropped Cipher Gem starts Unlocked and is collectible
// immediately, marking it carried and revealing the stairway.
RV_TEST(guarded_cipher_gem_starts_unlocked_and_collects) {
    Model m = makeModel();
    m.level()->setStairway(Position{3, 8}); // some floor tile
    RV_CHECK(m.level()->stairwayRevealed() == false);

    CipherGem gem;
    RV_CHECK(gem.isUnlocked() == true); // dropped gem is immediately collectible

    RV_CHECK(m.carryingCipherGem() == false);
    RV_CHECK(gem.tryCollect(m) == true);
    RV_CHECK(m.carryingCipherGem() == true);
    RV_CHECK(m.level()->stairwayRevealed() == true); // Ward lifted / stairway revealed

    // Second collect is a no-op.
    RV_CHECK(gem.tryCollect(m) == false);
}

RV_TEST(guarded_cipher_gem_symbol_is_c) {
    CipherGem gem;
    RV_CHECK(gem.symbol() == 'C');
}

// --- Walk-over semantics (DD-4) ---------------------------------------------

// While Locked, walking onto a guarded item does nothing; after onAnchorDeath,
// walking over collects it (onWalkOver mirrors tryCollect).
RV_TEST(guarded_walkover_locked_noop_then_collects_after_unlock) {
    Model m = makeModel();
    ShardCoffer coffer;
    const int before = m.runeFragmentTotal();

    // Locked: walk-over does nothing.
    coffer.onWalkOver(m);
    RV_CHECK(m.runeFragmentTotal() == before);
    RV_CHECK(coffer.collected() == false);

    // Unlocked: walk-over collects.
    coffer.onAnchorDeath();
    coffer.onWalkOver(m);
    RV_CHECK(m.runeFragmentTotal() == before + 6);
    RV_CHECK(coffer.collected() == true);
}
