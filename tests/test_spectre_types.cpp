// test_spectre_types.cpp
// RuneVault — tests for the concrete Spectre subclasses + SpectreFactory
// (task 10.1).
//
// Validates: Requirements 2.5 (per-type symbols & stats) and 4.7 (spawn
// distribution — createRandom excludes Vault Anchor and only produces the six
// spawnable types).
//
// Covers, against the design "Stats & drops table":
//   * each concrete subclass reports the right symbol/fp/pwr/wr and the correct
//     isMobile / runeDrop / isDefaultHostile (including the Lich cache=4 and the
//     stationary, dropless, passive Vault Anchor);
//   * SpectreFactory::create returns the matching concrete type with matching
//     stats;
//   * SpectreFactory::createRandom NEVER yields a Vault Anchor, only the six
//     expected types appear, and each appears at least once over enough seeded
//     draws.

#include "test_util.h"

#include "runevault/spectre.h"
#include "runevault/spectre_factory.h"
#include "runevault/view.h" // glyphs
#include "shuffle.h"        // rng::seed

#include <map>

// ---- Concrete subclass stats/policy (Requirement 2.5, design table) ---------

RV_TEST(spectre_types_wraith_stats_and_policy) {
    using namespace runevault;
    Wraith s(1, Position{2, 3});
    RV_CHECK(s.type() == SpectreType::Wraith);
    RV_CHECK(s.symbol() == glyphs::kWraith); // 'W'
    RV_CHECK(s.fp() == 60);
    RV_CHECK(s.pwr() == 20);
    RV_CHECK(s.wr() == 10);
    RV_CHECK(s.isMobile() == true);
    RV_CHECK(s.runeDrop() == 1);
    RV_CHECK(s.isDefaultHostile() == true);
    RV_CHECK(s.spawnChamber() == 1);
    RV_CHECK(s.position() == (Position{2, 3}));
}

RV_TEST(spectre_types_banshee_stats_and_policy) {
    using namespace runevault;
    Banshee s(0, Position{0, 0});
    RV_CHECK(s.type() == SpectreType::Banshee);
    RV_CHECK(s.symbol() == glyphs::kBanshee); // 'B'
    RV_CHECK(s.fp() == 40);
    RV_CHECK(s.pwr() == 30);
    RV_CHECK(s.wr() == 5);
    RV_CHECK(s.isMobile() == true);
    RV_CHECK(s.runeDrop() == 1);
    RV_CHECK(s.isDefaultHostile() == true);
}

RV_TEST(spectre_types_revenant_stats_and_policy) {
    using namespace runevault;
    Revenant s(0, Position{0, 0});
    RV_CHECK(s.type() == SpectreType::Revenant);
    RV_CHECK(s.symbol() == glyphs::kRevenant); // 'R'
    RV_CHECK(s.fp() == 100);
    RV_CHECK(s.pwr() == 18);
    RV_CHECK(s.wr() == 20);
    RV_CHECK(s.isMobile() == true);
    RV_CHECK(s.runeDrop() == 1);
    RV_CHECK(s.isDefaultHostile() == true);
}

RV_TEST(spectre_types_shade_stats_and_policy) {
    using namespace runevault;
    Shade s(0, Position{0, 0});
    RV_CHECK(s.type() == SpectreType::Shade);
    RV_CHECK(s.symbol() == glyphs::kShade); // 'S'
    RV_CHECK(s.fp() == 50);
    RV_CHECK(s.pwr() == 8);
    RV_CHECK(s.wr() == 8);
    RV_CHECK(s.isMobile() == true);
    RV_CHECK(s.runeDrop() == 1);
    RV_CHECK(s.isDefaultHostile() == true);
}

RV_TEST(spectre_types_lich_cache_and_non_hostile) {
    using namespace runevault;
    Lich s(0, Position{0, 0});
    RV_CHECK(s.type() == SpectreType::Lich);
    RV_CHECK(s.symbol() == glyphs::kLich); // 'L'
    RV_CHECK(s.fp() == 30);
    RV_CHECK(s.pwr() == 40);
    RV_CHECK(s.wr() == 5);
    RV_CHECK(s.isMobile() == true);
    RV_CHECK(s.runeDrop() == 4);              // Lich cache
    RV_CHECK(s.isDefaultHostile() == false);  // dormant until attacked
}

RV_TEST(spectre_types_vaultanchor_stationary_dropless_passive) {
    using namespace runevault;
    VaultAnchor s(0, Position{0, 0});
    RV_CHECK(s.type() == SpectreType::VaultAnchor);
    RV_CHECK(s.symbol() == glyphs::kVaultAnchor); // 'A'
    RV_CHECK(s.fp() == 120);
    RV_CHECK(s.pwr() == 22);
    RV_CHECK(s.wr() == 18);
    RV_CHECK(s.isMobile() == false);          // never moves
    RV_CHECK(s.runeDrop() == 0);              // no drop
    RV_CHECK(s.isDefaultHostile() == false);  // guards passively
}

RV_TEST(spectre_types_specterlord_stats_and_policy) {
    using namespace runevault;
    SpecterLord s(0, Position{0, 0});
    RV_CHECK(s.type() == SpectreType::SpecterLord);
    RV_CHECK(s.symbol() == glyphs::kSpecterLord); // 'X'
    RV_CHECK(s.fp() == 80);
    RV_CHECK(s.pwr() == 28);
    RV_CHECK(s.wr() == 15);
    RV_CHECK(s.isMobile() == true);
    RV_CHECK(s.runeDrop() == 1);
    RV_CHECK(s.isDefaultHostile() == true);
}

// ---- SpectreFactory::create (explicit) --------------------------------------

RV_TEST(spectre_factory_create_returns_matching_type_and_stats) {
    using namespace runevault;
    SpectreFactory factory;

    auto wraith = factory.create(SpectreType::Wraith, 2, Position{4, 5});
    RV_CHECK(wraith->type() == SpectreType::Wraith);
    RV_CHECK(wraith->symbol() == glyphs::kWraith);
    RV_CHECK(wraith->fp() == 60);
    RV_CHECK(wraith->spawnChamber() == 2);
    RV_CHECK(wraith->position() == (Position{4, 5}));

    auto lich = factory.create(SpectreType::Lich, 1, Position{0, 0});
    RV_CHECK(lich->type() == SpectreType::Lich);
    RV_CHECK(lich->runeDrop() == 4);
    RV_CHECK(lich->isDefaultHostile() == false);

    // VaultAnchor is available for EXPLICIT placement.
    auto anchor = factory.create(SpectreType::VaultAnchor, 3, Position{1, 1});
    RV_CHECK(anchor->type() == SpectreType::VaultAnchor);
    RV_CHECK(anchor->isMobile() == false);
    RV_CHECK(anchor->runeDrop() == 0);

    auto lord = factory.create(SpectreType::SpecterLord, 0, Position{0, 0});
    RV_CHECK(lord->symbol() == glyphs::kSpecterLord);
    RV_CHECK(lord->pwr() == 28);
}

// ---- SpectreFactory::createRandom (distribution, Requirement 4.7) -----------

RV_TEST(spectre_factory_create_random_excludes_vaultanchor_and_covers_six) {
    using namespace runevault;
    SpectreFactory factory;
    rng::seed(12345u); // deterministic sequence for a reproducible assertion

    std::map<SpectreType, int> counts;
    const int kDraws = 5000;
    for (int i = 0; i < kDraws; ++i) {
        auto s = factory.createRandom(1, Position{0, 0});
        // NEVER a Vault Anchor (Anchors are placed explicitly).
        RV_CHECK(s->type() != SpectreType::VaultAnchor);
        counts[s->type()]++;
    }

    // Only the six spawnable types appear, and each appears at least once.
    RV_CHECK(counts.count(SpectreType::VaultAnchor) == 0);
    RV_CHECK(counts[SpectreType::Wraith] > 0);
    RV_CHECK(counts[SpectreType::Banshee] > 0);
    RV_CHECK(counts[SpectreType::Shade] > 0);
    RV_CHECK(counts[SpectreType::Revenant] > 0);
    RV_CHECK(counts[SpectreType::SpecterLord] > 0);
    RV_CHECK(counts[SpectreType::Lich] > 0);
    RV_CHECK(static_cast<int>(counts.size()) == 6);

    // Loose distribution sanity check: Shade (weight 5) should out-appear
    // Revenant (weight 2) over enough draws. Kept tolerant — this is a sanity
    // check, not a strict statistical test.
    RV_CHECK(counts[SpectreType::Shade] > counts[SpectreType::Revenant]);
}
