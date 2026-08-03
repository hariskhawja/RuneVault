// test_spectre_attack.cpp
// RuneVault — Spectre attack resolution, 50% miss, and loss-on-zero-FP
// (task 7.6, Requirements 6.4/6.5/6.6/10.5).
//
// Coverage:
//   * spectreDamageIfHit() pure damage core: computeDamage(spectrePwr,
//     arcanistWr) routed through the damage-taken ordering, including the
//     Aegis Cloak halving (rounded up).
//   * resolveSpectreAttack() on a HIT reduces the Arcanist's FP by exactly the
//     pure-core amount.
//   * loss-on-zero-FP: applying attacks until FP hits 0 sets runState() ==
//     RunState::Lost / isRunLost() and arcanist()->isDead().
//   * a MISS deals no damage (FP unchanged) — verified deterministically by
//     seeding rng and observing both hits and misses across a seeded run.

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/combat.h"
#include "runevault/model.h"
#include "runevault/spectre.h"
#include "shuffle.h"

#include <memory>

using namespace runevault;

namespace {

// Build a Model whose base Arcanist has the given stats.
Model makeModelWithArcanist(int fp, int pwr, int wr) {
    Model m;
    m.setArcanist(std::make_shared<Arcanist>(fp, pwr, wr));
    return m;
}

// A plain hostile Spectre with the given Power (WR/FP irrelevant to its attack).
GenericSpectre makeSpectre(int pwr) {
    return GenericSpectre(SpectreType::Wraith, /*fp=*/10, pwr, /*wr=*/0,
                          /*spawnChamber=*/1, Position{0, 0}, 'W');
}

} // namespace

// --- Pure damage core -------------------------------------------------------

RV_TEST(spectre_damage_if_hit_matches_formula_no_aegis) {
    // Arcanist WR 0, Spectre Pwr 20: ceil(100/100 * 20) = 20; no Warden, no
    // Aegis -> max(1, 20) = 20.
    Model m = makeModelWithArcanist(/*fp=*/500, /*pwr=*/5, /*wr=*/0);
    GenericSpectre s = makeSpectre(20);
    RV_CHECK(m.spectreDamageIfHit(s) == 20);
    // Sanity: equals computeDamage since neither modifier applies.
    RV_CHECK(m.spectreDamageIfHit(s) == computeDamage(20, 0));
}

RV_TEST(spectre_damage_if_hit_respects_arcanist_wr) {
    // Arcanist WR 50, Spectre Pwr 15: ceil(100/150 * 15) = ceil(10) = 10.
    Model m = makeModelWithArcanist(/*fp=*/500, /*pwr=*/5, /*wr=*/50);
    GenericSpectre s = makeSpectre(15);
    RV_CHECK(m.spectreDamageIfHit(s) == 10);
}

RV_TEST(spectre_damage_if_hit_aegis_halves_rounded_up) {
    // Aegis Cloak carried halves the base rounded up (Requirements 6.8/9.8).
    // Even base: WR 0, Pwr 20 -> base 20 -> ceil(20/2) = 10.
    Model even = makeModelWithArcanist(/*fp=*/500, /*pwr=*/5, /*wr=*/0);
    even.setCarryingAegisCloak(true);
    GenericSpectre s20 = makeSpectre(20);
    RV_CHECK(even.spectreDamageIfHit(s20) == 10);

    // Odd base: WR 0, Pwr 15 -> base 15 -> ceil(15/2) = 8 (rounded up).
    Model odd = makeModelWithArcanist(/*fp=*/500, /*pwr=*/5, /*wr=*/0);
    odd.setCarryingAegisCloak(true);
    GenericSpectre s15 = makeSpectre(15);
    RV_CHECK(odd.spectreDamageIfHit(s15) == 8);
}

RV_TEST(spectre_damage_if_hit_zero_without_arcanist) {
    Model m; // no Arcanist set
    GenericSpectre s = makeSpectre(20);
    RV_CHECK(m.spectreDamageIfHit(s) == 0);
}

// --- Applying an attack -----------------------------------------------------

RV_TEST(resolve_spectre_attack_hit_reduces_fp_by_computed_amount) {
    rng::seed(12345);
    Model m = makeModelWithArcanist(/*fp=*/100000, /*pwr=*/5, /*wr=*/0);
    GenericSpectre s = makeSpectre(20);
    const int expected = m.spectreDamageIfHit(s); // 20

    // Loop until a hit occurs; high FP guarantees the Arcanist survives.
    bool sawHit = false;
    for (int i = 0; i < 1000 && !sawHit; ++i) {
        const int before = m.arcanist()->fp();
        const bool hit = m.resolveSpectreAttack(s);
        const int after = m.arcanist()->fp();
        if (hit) {
            RV_CHECK(after == before - expected);
            sawHit = true;
        } else {
            RV_CHECK(after == before); // miss: no damage
        }
    }
    RV_CHECK(sawHit);
}

RV_TEST(resolve_spectre_attack_no_arcanist_returns_false) {
    rng::seed(1);
    Model m; // no Arcanist
    GenericSpectre s = makeSpectre(20);
    RV_CHECK(m.resolveSpectreAttack(s) == false);
    RV_CHECK(m.isRunLost() == false);
}

// --- Loss on zero FP --------------------------------------------------------

RV_TEST(resolve_spectre_attack_ends_run_as_loss_when_fp_hits_zero) {
    rng::seed(999);
    // 1 FP: any single hit (>= 1 damage) is lethal.
    Model m = makeModelWithArcanist(/*fp=*/1, /*pwr=*/5, /*wr=*/0);
    GenericSpectre s = makeSpectre(20);

    RV_CHECK(m.runState() == RunState::Playing);
    RV_CHECK(m.isRunLost() == false);

    // Apply attacks until a hit lands (50% miss). Bounded to guarantee the test
    // terminates even on an unlucky seeded streak of misses.
    bool hit = false;
    for (int i = 0; i < 1000 && !hit; ++i) {
        hit = m.resolveSpectreAttack(s);
    }
    RV_CHECK(hit);
    RV_CHECK(m.arcanist()->isDead());
    RV_CHECK(m.arcanist()->fp() == 0);
    RV_CHECK(m.isRunLost());
    RV_CHECK(m.runState() == RunState::Lost);
}

// --- Miss vs hit are both observable, misses deal no damage -----------------

RV_TEST(spectre_attack_miss_deals_no_damage_over_seeded_run) {
    rng::seed(2024);
    // Large FP so the Arcanist survives the whole run; small per-hit damage.
    Model m = makeModelWithArcanist(/*fp=*/100000, /*pwr=*/5, /*wr=*/0);
    GenericSpectre s = makeSpectre(20);

    int hits = 0;
    int misses = 0;
    for (int i = 0; i < 200; ++i) {
        const int before = m.arcanist()->fp();
        const bool hit = m.resolveSpectreAttack(s);
        const int after = m.arcanist()->fp();
        if (hit) {
            ++hits;
            RV_CHECK(after < before); // a hit always removes at least 1 FP
        } else {
            ++misses;
            RV_CHECK(after == before); // Requirement 6.5: a miss deals no damage
        }
    }
    // A fair 50% coin over 200 seeded rolls yields both outcomes.
    RV_CHECK(hits > 0);
    RV_CHECK(misses > 0);
    // The Arcanist never died, so the run is still Playing.
    RV_CHECK(m.runState() == RunState::Playing);
}
