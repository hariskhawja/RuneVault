// test_spawner.cpp
// RuneVault — tests for the spawning pipeline structure (task 15.1).
//
// These assert-based tests (see tests/test_util.h) verify the Spawner's
// structural invariants after populate() on a fresh Model whose Level is a
// Level(makeDefaultBoard()):
//
//   * the Arcanist is placed on a floor tile                 (Requirement 4.9)
//   * the stairway is placed on a floor tile                 (Requirement 4.9)
//   * every placed item and Spectre is on a floor tile       (Requirement 4.9)
//   * no two placed objects share a cell                     (Requirement 4.10)
//   * a Vault Anchor sits within a 1-block radius of the
//     Shard Coffer it guards                                 (Requirement 4.6)
//   * the pipeline runs and leaves the level populated (some scrolls, some
//     fragments including a coffer, some spectres)
//
// EXACT counts/distributions are NOT asserted here — those belong to the spawn
// counts/distribution tasks (15.3/15.4). The rng is seeded for determinism.

#include "test_util.h"

#include "runevault/board.h"
#include "runevault/cell.h"
#include "runevault/geometry.h"
#include "runevault/guarded_item.h"
#include "runevault/item.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spawner.h"
#include "runevault/spectre.h"

#include "shuffle.h"

#include <set>
#include <utility>
#include <vector>

using namespace runevault;

namespace {

// Build a fresh Model with a Level(makeDefaultBoard()) and run the Spawner with
// a fixed seed for determinism.
Model populatedModel(unsigned long seed) {
    rng::seed(seed);
    Model model;
    model.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    Spawner spawner;
    spawner.populate(model);
    return model;
}

// Build a Model on a specific level number with a chosen Aegis level, then run
// the Spawner (task 15.9). The Aegis is placed iff levelNumber == aegisLevel and
// aegisLevel is in 2..4 (and it has not already spawned this run).
Model populatedAegisModel(unsigned long seed, int levelNumber, int aegisLevel) {
    rng::seed(seed);
    Model model;
    model.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model.setAegisLevel(aegisLevel);
    model.setLevelNumber(levelNumber);
    Spawner spawner;
    spawner.populate(model);
    return model;
}

// Count the AegisCloak items on a level.
int aegisCount(const Level &level) {
    int n = 0;
    for (const auto &item : level.items()) {
        if (dynamic_cast<const AegisCloak *>(item.get()) != nullptr) {
            ++n;
        }
    }
    return n;
}

bool isFloor(const Board &board, const Position &p) {
    return board.inBounds(p) && board.at(p).type == CellType::Floor;
}

} // namespace

// The Arcanist and stairway are placed on floor tiles (Requirement 4.9).
RV_TEST(spawner_places_arcanist_and_stairway_on_floor) {
    Model model = populatedModel(1234);
    const Level *level = model.level();
    RV_CHECK(level != nullptr);
    if (level == nullptr) {
        return;
    }
    const Board &board = level->board();
    RV_CHECK(isFloor(board, model.arcanistPosition()));
    RV_CHECK(isFloor(board, level->stairway()));
}

// Requirements 4.2/4.3: the Arcanist and the stairway are placed in DIFFERENT
// chambers, both on floor tiles whose chamber membership is a valid chamber id
// (1..kChamberCount, never kNoChamber). Run across several seeds so the
// separation is not an accident of one RNG stream.
RV_TEST(spawner_arcanist_and_stairway_in_different_chambers) {
    const unsigned long seeds[] = {1u,     42u,     1234u,    9999u,   777777u,
                                   13u,    2024u,   555u,     101010u, 8u};
    for (unsigned long seed : seeds) {
        Model model = populatedModel(seed);
        const Level *level = model.level();
        RV_CHECK(level != nullptr);
        if (level == nullptr) {
            continue;
        }
        const Board &board = level->board();

        Position arcanist = model.arcanistPosition();
        Position stairway = level->stairway();

        // Both remain floor tiles (existing invariant).
        RV_CHECK(isFloor(board, arcanist));
        RV_CHECK(isFloor(board, stairway));

        int arcanistChamber = board.at(arcanist).chamberId;
        int stairwayChamber = board.at(stairway).chamberId;

        // Both chamber ids are valid: a real chamber, never the kNoChamber
        // sentinel, within 1..kChamberCount.
        RV_CHECK(arcanistChamber != kNoChamber);
        RV_CHECK(stairwayChamber != kNoChamber);
        RV_CHECK(arcanistChamber >= 1 && arcanistChamber <= kChamberCount);
        RV_CHECK(stairwayChamber >= 1 && stairwayChamber <= kChamberCount);

        // Requirement 4.3: the two chambers differ.
        RV_CHECK(arcanistChamber != stairwayChamber);
    }
}

// Every placed item and Spectre is on a floor tile (Requirement 4.9).
RV_TEST(spawner_places_all_objects_on_floor_tiles) {
    Model model = populatedModel(1234);
    const Level *level = model.level();
    RV_CHECK(level != nullptr);
    if (level == nullptr) {
        return;
    }
    const Board &board = level->board();

    for (const auto &item : level->items()) {
        RV_CHECK(isFloor(board, item->position()));
    }
    for (const auto &spectre : level->spectres()) {
        RV_CHECK(isFloor(board, spectre->position()));
    }
}

// No two placed objects share a cell (Requirement 4.10). Collect every occupied
// position — Arcanist, stairway, each item, each Spectre — and assert they are
// all distinct.
RV_TEST(spawner_no_two_objects_share_a_cell) {
    Model model = populatedModel(1234);
    const Level *level = model.level();
    RV_CHECK(level != nullptr);
    if (level == nullptr) {
        return;
    }

    std::vector<Position> occupied;
    occupied.push_back(model.arcanistPosition());
    occupied.push_back(level->stairway());
    for (const auto &item : level->items()) {
        occupied.push_back(item->position());
    }
    for (const auto &spectre : level->spectres()) {
        occupied.push_back(spectre->position());
    }

    std::set<std::pair<int, int>> seen;
    bool allUnique = true;
    for (const Position &p : occupied) {
        auto key = std::make_pair(p.row, p.col);
        if (!seen.insert(key).second) {
            allUnique = false;
        }
    }
    RV_CHECK(allUnique);
    // Sanity: we placed at least the Arcanist + stairway + some objects.
    RV_CHECK(occupied.size() == seen.size());
}

// A Vault Anchor sits within a 1-block radius of the Shard Coffer it guards
// (Requirement 4.6 — the shared guarded-placement helper).
RV_TEST(spawner_vault_anchor_guards_shard_coffer) {
    Model model = populatedModel(1234);
    const Level *level = model.level();
    RV_CHECK(level != nullptr);
    if (level == nullptr) {
        return;
    }

    // Locate the Shard Coffer.
    Position cofferPos{-1, -1};
    bool foundCoffer = false;
    for (const auto &item : level->items()) {
        if (dynamic_cast<const ShardCoffer *>(item.get()) != nullptr) {
            cofferPos = item->position();
            foundCoffer = true;
        }
    }
    RV_CHECK(foundCoffer);
    if (!foundCoffer) {
        return;
    }

    // A Vault Anchor must exist within a 1-block radius of the coffer AND be
    // wired to guard exactly that tile.
    bool anchorAdjacent = false;
    for (const auto &spectre : level->spectres()) {
        if (spectre->type() != SpectreType::VaultAnchor) {
            continue;
        }
        if (Geometry::isAdjacent(spectre->position(), cofferPos) &&
            spectre->guardedItemPosition() == cofferPos) {
            anchorAdjacent = true;
        }
    }
    RV_CHECK(anchorAdjacent);
}

// The pipeline runs without crashing and leaves the level populated: some
// scrolls, some fragments including a coffer, and some spectres.
RV_TEST(spawner_leaves_level_populated) {
    Model model = populatedModel(9999);
    const Level *level = model.level();
    RV_CHECK(level != nullptr);
    if (level == nullptr) {
        return;
    }

    int scrolls = 0;
    int fragments = 0;
    int coffers = 0;
    for (const auto &item : level->items()) {
        if (dynamic_cast<const Scroll *>(item.get()) != nullptr) {
            ++scrolls;
        } else if (dynamic_cast<const ShardCoffer *>(item.get()) != nullptr) {
            ++coffers;
        } else if (dynamic_cast<const RuneFragment *>(item.get()) != nullptr) {
            ++fragments;
        }
    }

    RV_CHECK(scrolls > 0);
    RV_CHECK(fragments > 0);
    RV_CHECK(coffers == 1);
    RV_CHECK(!level->spectres().empty());
}

// Requirement 4.4: EXACTLY 10 scrolls are placed per level. Run across several
// seeds so the exact count is not an accident of one RNG stream.
RV_TEST(spawner_places_exactly_ten_scrolls) {
    const unsigned long seeds[] = {1u, 42u, 1234u, 9999u, 777777u};
    for (unsigned long seed : seeds) {
        Model model = populatedModel(seed);
        const Level *level = model.level();
        RV_CHECK(level != nullptr);
        if (level == nullptr) {
            continue;
        }
        int scrolls = 0;
        for (const auto &item : level->items()) {
            if (dynamic_cast<const Scroll *>(item.get()) != nullptr) {
                ++scrolls;
            }
        }
        RV_CHECK(scrolls == 10);
    }
}

// Requirement 4.5: EXACTLY 1 Shard Coffer plus EXACTLY 9 additional RuneFragment
// piles are placed. The Shard Coffer is a ShardCoffer (GuardedItem); the 9 piles
// are plain RuneFragments. Every pile's grade must be a valid fragment grade —
// common shard (value 1) or resonant shard (value 2). The exact 5/7 vs 2/7 split
// is random, so only grade VALIDITY is asserted, not an exact split.
RV_TEST(spawner_places_one_coffer_and_nine_fragment_piles) {
    const unsigned long seeds[] = {1u, 42u, 1234u, 9999u, 777777u};
    for (unsigned long seed : seeds) {
        Model model = populatedModel(seed);
        const Level *level = model.level();
        RV_CHECK(level != nullptr);
        if (level == nullptr) {
            continue;
        }
        int coffers = 0;
        int piles = 0;
        bool gradesValid = true;
        for (const auto &item : level->items()) {
            if (dynamic_cast<const ShardCoffer *>(item.get()) != nullptr) {
                ++coffers;
            } else if (const auto *frag =
                           dynamic_cast<const RuneFragment *>(item.get())) {
                ++piles;
                if (frag->value() != 1 && frag->value() != 2) {
                    gradesValid = false;
                }
            }
        }
        RV_CHECK(coffers == 1);
        RV_CHECK(piles == 9);
        RV_CHECK(gradesValid);
    }
}

// Requirement 4.7: EXACTLY 20 non-Vault-Anchor Spectres are placed. At this
// stage the only Vault Anchor on the level is the single one guarding the Shard
// Coffer (the Aegis Cloak's anchor is task 15.9), so the Vault Anchor count is 1
// and the total spectre list is 21.
RV_TEST(spawner_places_exactly_twenty_spectres) {
    const unsigned long seeds[] = {1u, 42u, 1234u, 9999u, 777777u};
    for (unsigned long seed : seeds) {
        Model model = populatedModel(seed);
        const Level *level = model.level();
        RV_CHECK(level != nullptr);
        if (level == nullptr) {
            continue;
        }
        int nonAnchor = 0;
        int anchors = 0;
        for (const auto &spectre : level->spectres()) {
            if (spectre->type() == SpectreType::VaultAnchor) {
                ++anchors;
            } else {
                ++nonAnchor;
            }
        }
        RV_CHECK(nonAnchor == 20);
        RV_CHECK(anchors == 1);
    }
}

// Requirement 4.12: after the spectres are placed, EXACTLY ONE non-Vault-Anchor
// Spectre carries the Cipher Gem. Across several seeds, assert that the count of
// carriers is exactly 1 and that the carrier is never a Vault Anchor.
RV_TEST(spawner_selects_exactly_one_cipher_gem_carrier) {
    const unsigned long seeds[] = {1u,     42u,     1234u,    9999u,   777777u,
                                   13u,    2024u,   555u,     101010u, 8u};
    for (unsigned long seed : seeds) {
        Model model = populatedModel(seed);
        const Level *level = model.level();
        RV_CHECK(level != nullptr);
        if (level == nullptr) {
            continue;
        }

        int carriers = 0;
        bool carrierIsNonAnchor = true;
        for (const auto &spectre : level->spectres()) {
            if (spectre->carriesCipherGem()) {
                ++carriers;
                // Requirement 4.12: the carrier must NOT be a Vault Anchor.
                if (spectre->type() == SpectreType::VaultAnchor) {
                    carrierIsNonAnchor = false;
                }
            }
        }
        RV_CHECK(carriers == 1);
        RV_CHECK(carrierIsNonAnchor);
    }
}

// Requirement 4.13 (task 15.9): chooseAegisLevel() always returns a level in
// {2, 3, 4}. Run across many seeded calls so the range holds across the RNG
// stream, not just one draw.
RV_TEST(spawner_choose_aegis_level_is_in_two_to_four) {
    const unsigned long seeds[] = {1u,     42u,     1234u,    9999u,   777777u,
                                   13u,    2024u,   555u,     101010u, 8u};
    for (unsigned long seed : seeds) {
        rng::seed(seed);
        for (int i = 0; i < 50; ++i) {
            int chosen = Spawner::chooseAegisLevel();
            RV_CHECK(chosen >= 2 && chosen <= 4);
        }
    }
}

// Requirements 4.13/9.6 (task 15.9): when the model's current level IS the
// chosen Aegis level, EXACTLY ONE Aegis Cloak is placed, guarded by a Vault
// Anchor within a 1-block radius (same rules as the coffer), and
// aegisSpawnedThisRun() is set. That level now has TWO Vault Anchors (coffer +
// Aegis), so we assert the AegisCloak count is 1 and it is guarded — not a
// total-anchor count. Run across several seeds.
RV_TEST(spawner_places_aegis_on_chosen_level_guarded) {
    const unsigned long seeds[] = {1u, 42u, 1234u, 9999u, 777777u};
    for (unsigned long seed : seeds) {
        Model model = populatedAegisModel(seed, /*levelNumber=*/3,
                                          /*aegisLevel=*/3);
        const Level *level = model.level();
        RV_CHECK(level != nullptr);
        if (level == nullptr) {
            continue;
        }

        // Exactly one Aegis Cloak on the chosen level, and the run flag is set.
        RV_CHECK(aegisCount(*level) == 1);
        RV_CHECK(model.aegisSpawnedThisRun());

        // Locate the Aegis Cloak's tile.
        Position aegisPos{-1, -1};
        bool foundAegis = false;
        for (const auto &item : level->items()) {
            if (dynamic_cast<const AegisCloak *>(item.get()) != nullptr) {
                aegisPos = item->position();
                foundAegis = true;
            }
        }
        RV_CHECK(foundAegis);
        if (!foundAegis) {
            continue;
        }

        // A Vault Anchor must sit within a 1-block radius of the Aegis AND be
        // wired to guard exactly that tile (Requirement 4.6, reused helper).
        bool anchorGuardsAegis = false;
        for (const auto &spectre : level->spectres()) {
            if (spectre->type() != SpectreType::VaultAnchor) {
                continue;
            }
            if (Geometry::isAdjacent(spectre->position(), aegisPos) &&
                spectre->guardedItemPosition() == aegisPos) {
                anchorGuardsAegis = true;
            }
        }
        RV_CHECK(anchorGuardsAegis);
    }
}

// Requirements 4.13/9.6 (task 15.9): when the current level is NOT the chosen
// Aegis level, NO Aegis Cloak is placed and aegisSpawnedThisRun() stays false.
RV_TEST(spawner_places_no_aegis_off_chosen_level) {
    const unsigned long seeds[] = {1u, 42u, 1234u, 9999u, 777777u};
    for (unsigned long seed : seeds) {
        Model model = populatedAegisModel(seed, /*levelNumber=*/2,
                                          /*aegisLevel=*/3);
        const Level *level = model.level();
        RV_CHECK(level != nullptr);
        if (level == nullptr) {
            continue;
        }
        RV_CHECK(aegisCount(*level) == 0);
        RV_CHECK(!model.aegisSpawnedThisRun());
    }
}

// Requirement 9.6 (task 15.9): a default-constructed Model (levelNumber 1,
// aegisLevel 0 = unset) never receives an Aegis Cloak, so the existing
// default-model spawner tests remain valid.
RV_TEST(spawner_default_model_places_no_aegis) {
    const unsigned long seeds[] = {1u, 42u, 1234u, 9999u, 777777u};
    for (unsigned long seed : seeds) {
        Model model = populatedModel(seed);
        const Level *level = model.level();
        RV_CHECK(level != nullptr);
        if (level == nullptr) {
            continue;
        }
        RV_CHECK(aegisCount(*level) == 0);
        RV_CHECK(!model.aegisSpawnedThisRun());
    }
}
