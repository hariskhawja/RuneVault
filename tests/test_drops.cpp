// test_drops.cpp
// RuneVault — Spectre defeat drops (task 7.8).
//
// Covers Requirements 6.10/6.11/6.12 via Model::handleAttack:
//   * 6.10 — a non-Vault-Anchor, non-Lich defeat adds 1 Rune Fragment (the base
//     Spectre::runeDrop() default), and the corpse is removed from the level.
//   * 6.11 — a Lich cache adds 4 (simulated by a test-only subclass overriding
//     runeDrop()->4, since the typed Lich subclass is task 10.1).
//   * A Vault-Anchor-like Spectre (runeDrop()->0) awards nothing.
//   * Non-lethal hits award nothing while the Spectre still lives.
//   * 6.12 — defeating the Cipher Gem carrier records a drop on its tile; a
//     non-carrier defeat records no drop.
//
// Uses the tiny assert-based harness (test_util.h); no main() here —
// tests/test_main.cpp drives run_all().

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/combat.h"
#include "runevault/item.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spectre.h"

#include <memory>

using namespace runevault;

namespace {

// Build a Model with an Arcanist placed at `pos`, and an (empty) default Level.
std::unique_ptr<Model> makeModelAt(const Position &pos, int fp, int pwr,
                                   int wr) {
    auto model = std::unique_ptr<Model>(new Model());
    model->setArcanist(std::make_shared<Arcanist>(fp, pwr, wr));
    model->setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model->setArcanistPosition(pos);
    return model;
}

// A living GenericSpectre (base runeDrop()==1) at `pos`.
std::unique_ptr<GenericSpectre> makeSpectre(const Position &pos, int fp,
                                            int pwr, int wr) {
    return std::unique_ptr<GenericSpectre>(new GenericSpectre(
        SpectreType::Wraith, fp, pwr, wr, /*spawnChamber*/ 1, pos, 'W'));
}

// Test-only subclass with a configurable runeDrop, standing in for the typed
// Lich (drop 4) / Vault Anchor (drop 0) subclasses that arrive in task 10.1.
class DropSpectre : public GenericSpectre {
public:
    DropSpectre(const Position &pos, int fp, int pwr, int wr, int drop,
                char symbol)
        : GenericSpectre(SpectreType::Wraith, fp, pwr, wr, /*chamber*/ 1, pos,
                         symbol),
          drop_(drop) {}

    int runeDrop() const override { return drop_; }

private:
    int drop_;
};

} // namespace

// --- 6.10: base defeat awards 1 RF and removes the corpse -----------------

RV_TEST(defeating_generic_spectre_awards_one_rf_and_removes_it) {
    const Position arc{2, 3};
    // pwr=50 vs wr=0 => 50 damage, one-shots a 40-FP Spectre.
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 50, /*wr*/ 0);
    const Position target{2, 4};
    model->level()->addSpectre(makeSpectre(target, /*fp*/ 40, /*pwr*/ 5, /*wr*/ 0));

    RV_CHECK(model->runeFragmentTotal() == 0);
    RV_CHECK(model->handleAttack(Direction::Ea) == true);

    // Requirement 6.10: +1 RF; corpse removed from the level.
    RV_CHECK(model->runeFragmentTotal() == 1);
    RV_CHECK(model->level()->spectreAt(target) == nullptr);
    RV_CHECK(model->cipherGemDropped() == false); // non-carrier: no gem drop
}

// --- 6.11: a Lich cache (runeDrop 4) awards 4 -----------------------------

RV_TEST(defeating_lich_like_spectre_awards_four_rf) {
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 50, /*wr*/ 0);
    const Position target{2, 4};
    model->level()->addSpectre(std::unique_ptr<DropSpectre>(
        new DropSpectre(target, /*fp*/ 40, /*pwr*/ 5, /*wr*/ 0, /*drop*/ 4, 'L')));

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->runeFragmentTotal() == 4);
    RV_CHECK(model->level()->spectreAt(target) == nullptr);
}

// --- Vault-Anchor-like (runeDrop 0) awards nothing ------------------------

RV_TEST(defeating_anchor_like_spectre_awards_zero_rf) {
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 50, /*wr*/ 0);
    const Position target{2, 4};
    model->level()->addSpectre(std::unique_ptr<DropSpectre>(
        new DropSpectre(target, /*fp*/ 40, /*pwr*/ 5, /*wr*/ 0, /*drop*/ 0, 'A')));

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->runeFragmentTotal() == 0);
    RV_CHECK(model->level()->spectreAt(target) == nullptr);
}

// --- Non-lethal hits award nothing ----------------------------------------

RV_TEST(non_lethal_hit_awards_no_rf_and_leaves_spectre_alive) {
    const Position arc{2, 3};
    // pwr=10 vs wr=0 => 10 damage; a 40-FP Spectre survives.
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 10, /*wr*/ 0);
    const Position target{2, 4};
    model->level()->addSpectre(makeSpectre(target, /*fp*/ 40, /*pwr*/ 5, /*wr*/ 0));

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    // Requirement 6.10 only awards on defeat: still alive => no RF.
    RV_CHECK(model->runeFragmentTotal() == 0);
    Spectre *s = model->level()->spectreAt(target);
    RV_CHECK(s != nullptr);
    RV_CHECK(!s->isDead());
    RV_CHECK(s->fp() == 30);
    RV_CHECK(model->cipherGemDropped() == false);
}

// --- 6.12: defeating the Cipher Gem carrier drops the gem on its tile ------

RV_TEST(defeating_cipher_gem_carrier_records_drop_on_its_tile) {
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 50, /*wr*/ 0);
    const Position target{2, 4};
    auto carrier = makeSpectre(target, /*fp*/ 40, /*pwr*/ 5, /*wr*/ 0);
    carrier->setCarriesCipherGem(true);
    model->level()->addSpectre(std::move(carrier));

    RV_CHECK(model->cipherGemDropped() == false);
    RV_CHECK(model->handleAttack(Direction::Ea) == true);

    // Requirement 6.12: gem dropped on the carrier's tile; corpse removed.
    RV_CHECK(model->cipherGemDropped() == true);
    RV_CHECK(model->cipherGemDropPosition() == target);
    RV_CHECK(model->level()->spectreAt(target) == nullptr);
    // The carrier is still a normal Spectre: its base runeDrop 1 is awarded too.
    RV_CHECK(model->runeFragmentTotal() == 1);
}

RV_TEST(defeating_non_carrier_does_not_record_cipher_gem_drop) {
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 50, /*wr*/ 0);
    const Position target{2, 4};
    model->level()->addSpectre(makeSpectre(target, /*fp*/ 40, /*pwr*/ 5, /*wr*/ 0));

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(model->cipherGemDropped() == false);
}

// --- Q13: a defeated LICH drops its cache as a PHYSICAL item, not auto-credit -

RV_TEST(defeating_lich_drops_physical_cache_not_auto_credited) {
    // Piazza Q13: the Lich cache is NOT added to the total on death. It appears
    // as a physical Rune Fragment (worth 4) on the Lich's tile, collected by
    // walking over it — exactly like the Cipher Gem.
    const Position arc{2, 3};
    auto model = makeModelAt(arc, /*fp*/ 20, /*pwr*/ 50, /*wr*/ 0);
    const Position lichPos{2, 4};
    model->level()->addSpectre(
        std::unique_ptr<Spectre>(new Lich(/*spawnChamber*/ 1, lichPos)));

    RV_CHECK(model->runeFragmentTotal() == 0);
    RV_CHECK(model->handleAttack(Direction::Ea) == true);

    // Q13: nothing is auto-credited; the corpse is removed but a fragment item
    // worth 4 now sits on the Lich's former tile.
    RV_CHECK(model->runeFragmentTotal() == 0);
    RV_CHECK(model->level()->spectreAt(lichPos) == nullptr);
    Item *dropped = model->level()->itemAt(lichPos);
    RV_CHECK(dropped != nullptr);
    RuneFragment *cache = dynamic_cast<RuneFragment *>(dropped);
    RV_CHECK(cache != nullptr);
    RV_CHECK(cache->value() == 4); // Lich cache value

    // Walking onto the cache tile collects it (+4) and removes it (Req 5.4).
    RV_CHECK(model->moveArcanist(Direction::Ea) == true);
    RV_CHECK(model->arcanistPosition() == lichPos);
    RV_CHECK(model->runeFragmentTotal() == 4);
    RV_CHECK(model->level()->itemAt(lichPos) == nullptr);
}
