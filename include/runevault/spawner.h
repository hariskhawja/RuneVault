// spawner.h
// RuneVault — the random spawning pipeline (task 15.1).
//
// The `Spawner` populates a `Level` (and the run-scoped `Model` state that goes
// with it, such as the Arcanist's board position) by executing the spec-ordered
// spawning pipeline using ONLY the provided `rng` utilities so a given seed
// reproduces an identical sequence of placements (Requirement 4.11, exercised
// by task 15.11). See design "Spawning Pipeline".
//
// PIPELINE ORDER (Requirement 4.1, strict — design "Order (authoritative)"):
//   1. Arcanist location
//   2. stairway location
//   3. scrolls
//   4. rune fragments (incl. the guarded Shard Coffer)
//   5. spectres
// Each stage is a private method invoked in exactly this order from populate().
//
// INVARIANTS this task establishes:
//   * Requirement 4.9 — items and Spectres are placed ONLY on floor tiles
//     (Cell::canHost / CellType::Floor), never on archways, corridors, walls, or
//     the stairway tile.
//   * Requirement 4.10 — no two placed objects share a cell at spawn time. A
//     shared `occupied_` set records every claimed Position (Arcanist, stairway,
//     every item, every Spectre); `isFreeFloorTile` rejects any tile that is not
//     a floor tile or is already occupied, so the one-object-per-cell invariant
//     holds. (The runtime "fragments may transiently share the Arcanist's cell
//     during walk-over collection" exception is a MOVEMENT concern, not spawn.)
//   * Requirement 4.6 / 4.8 — the shared `placeGuarded` helper puts a Vault
//     Anchor within a 1-block radius of the item it guards, in the SAME chamber;
//     chamber selection for spawned objects makes every chamber equally likely.
//
// SCOPE (task 15.1): this task builds the pipeline STRUCTURE, the occupancy
// tracking, the free-tile picker, and the shared guarded-placement helper, with
// a working (deliberately minimal) placement in each stage so the pipeline runs
// end-to-end and the occupancy invariant holds. The EXACT counts/distributions
// (task 15.3), the Arcanist/stairway chamber separation (task 15.5), the Cipher
// Gem carrier selection (task 15.7), and the Aegis Cloak placement (task 15.9)
// are SEPARATE later tasks; the API here is shaped so those slot in without
// restructuring. Where a later task refines a stage, the code carries a TODO
// pointing at it.

#ifndef RUNEVAULT_SPAWNER_H
#define RUNEVAULT_SPAWNER_H

#include "runevault/board.h"
#include "runevault/geometry.h"
#include "runevault/guarded_item.h"
#include "runevault/level.h"
#include "runevault/model.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace runevault {

class Spawner {
public:
    // Sentinel `chamberId` for the tile pickers / placeGuarded meaning "any
    // chamber": choose a chamber uniformly at random first, then a free floor
    // tile within it (Requirement 4.8 — every chamber equally likely). Real
    // chamber ids are 1..kChamberCount, so 0 is an unambiguous sentinel.
    static constexpr int kAnyChamber = 0;

    // --- Exact spawn counts (task 15.3) ----------------------------------
    // Requirement 4.4: exactly 10 scrolls per level, each type 1/6.
    static constexpr int kScrollCount = 10;
    // Requirement 4.5: exactly 1 guarded Shard Coffer PLUS exactly 9 extra
    // fragment piles (common 5/7, resonant 2/7).
    static constexpr int kFragmentPileCount = 9;
    // Requirement 4.7: exactly 20 spectres per level via the factory's
    // distribution (never a Vault Anchor).
    static constexpr int kSpectreCount = 20;

    // Populate `model`'s current level per the spec pipeline (Requirement 4.1).
    // Assumes model.level() is already set (a fresh Level(makeDefaultBoard())).
    // Clears any prior occupancy bookkeeping and runs the five stages in order.
    // No-op when model.level() is null.
    //
    // In addition to the five core stages, populate() places the once-per-run
    // Aegis Cloak (task 15.9, Requirements 4.13/9.6) on the run-selected level.
    //
    // (Continued below with the ACTORS-ONLY variant for file-driven runs.)
    // It CONSUMES model.aegisLevel(): the Aegis is placed IF AND ONLY IF
    // aegisLevel() is a chosen level in 2..4, the model's current levelNumber()
    // equals that chosen level, and the Aegis has not already been spawned this
    // run (!aegisSpawnedThisRun()). A default-constructed Model has
    // aegisLevel() == 0 (unset), so no Aegis is placed — leaving pre-existing
    // default-model spawner tests unaffected.
    void populate(Model &model);

    // --- Actors-only pass for file-driven levels (task 19.1, Req 12.1) ----
    // Populate the ACTORS onto a level whose board + items were already built
    // from a layout FILE (LayoutLoader::buildLevel). Unlike populate(), this
    // does NOT place scrolls or rune-fragment piles — the file supplied those.
    // It runs the placement stages that a raw map cannot describe, on FREE floor
    // tiles that are not already occupied by a file item:
    //   1. Arcanist location            (Requirement 4.2)
    //   2. stairway location            (Requirement 4.3, different chamber)
    //   3. a guarding Vault Anchor for EACH file Shard Coffer (Requirement 4.6):
    //      every GuardedItem already on the board gets its own Anchor placed
    //      within a 1-block radius in the same chamber and wired to guard it.
    //   4. the once-per-run Aegis Cloak on the run-chosen level (Requirements
    //      4.13/9.6) — the Aegis is never encoded in a layout file, so it is
    //      still spawned here via the shared guarded placement.
    //   5. exactly 20 spectres          (Requirement 4.7)
    //   6. the Cipher Gem carrier        (Requirement 4.12)
    // Every file item's tile is marked occupied FIRST so no actor ever lands on
    // a file scroll/fragment/coffer (Requirement 4.10). No-op when level()==null.
    void populateActorsOnly(Model &model);

    // --- Aegis Cloak run-start selection (task 15.9, Requirement 4.13) ---
    // Choose the single level (uniform over {2, 3, 4}) on which the Aegis Cloak
    // spawns for a run. Called ONCE at run start by the run lifecycle
    // (tasks 16/19: model.setAegisLevel(Spawner::chooseAegisLevel())); populate()
    // then consumes model.aegisLevel() to place the Aegis on exactly that level.
    // Uses rng::range so a seeded run reproduces the selection (Requirement 4.11).
    static int chooseAegisLevel();

private:
    // --- Occupancy tracking (Requirement 4.10) ---------------------------
    // Every claimed Position (Arcanist, stairway, each item, each Spectre) is
    // recorded here so no two placed objects share a cell. Keyed by (row, col).
    std::set<std::pair<int, int>> occupied_;

    // Reset occupancy at the start of a populate() run.
    void reset();

    bool isOccupied(const Position &p) const;
    void markOccupied(const Position &p);

    // Requirement 4.9 + 4.10: `p` is a legal spawn tile iff it is in bounds, is
    // a FLOOR tile (canHost), and is not already occupied. Archways, corridors,
    // walls, and the stairway tile all fail canHost, so they are excluded here.
    bool isFreeFloorTile(const Board &board, const Position &p) const;

    // Collect every currently-free floor tile of `chamberId`, in row-major
    // order (Board::chamberFloorTiles order minus occupied tiles).
    std::vector<Position> freeFloorTilesInChamber(const Board &board,
                                                  int chamberId) const;

    // Pick a uniformly-random free floor tile (design "Occupancy invariant").
    //   * chamberId == kAnyChamber: choose a chamber uniformly at random from
    //     those that still have a free floor tile (Requirement 4.8), then a free
    //     tile within it uniformly.
    //   * otherwise: a free floor tile within `chamberId` uniformly.
    // Writes the chosen tile to `out` and returns true; returns false (leaving
    // `out` untouched) when no free floor tile is available (should not occur
    // with the fixed layout given the spawn counts — design "Edge cases").
    bool pickFreeFloorTile(const Board &board, int chamberId,
                           Position &out) const;

    // Pick a uniformly-random free floor tile, choosing the CHAMBER uniformly
    // at random among those that still have a free floor tile while EXCLUDING
    // `excludeChamber`, then a free tile within the chosen chamber uniformly
    // (Requirements 4.2/4.3 — chamber uniform, tile uniform, and the chosen
    // chamber differs from `excludeChamber`). Pass `kNoChamber` as
    // `excludeChamber` to exclude nothing (plain chamber-uniform selection).
    // Writes the chosen tile to `out` and the chosen chamber id to
    // `chosenChamber`, returning true; returns false (leaving both untouched)
    // when no eligible chamber has a free floor tile.
    bool pickFreeFloorTileExcludingChamber(const Board &board,
                                           int excludeChamber, Position &out,
                                           int &chosenChamber) const;

    // The chamber the Arcanist was placed in during placeArcanist, recorded so
    // placeStairway can exclude it (Requirement 4.3 — the stairway's chamber
    // must differ from the Arcanist's). Reset to kNoChamber each populate() run;
    // valid once placeArcanist has run (which precedes placeStairway per the
    // Requirement 4.1 pipeline order).
    int arcanistChamber_ = kNoChamber;

    // --- Pipeline stages (Requirement 4.1 order) -------------------------
    // Each stage places its objects on free floor tiles and marks them occupied.
    void placeArcanist(Model &model, Level &level);
    void placeStairway(Model &model, Level &level);
    void placeScrolls(Model &model, Level &level);
    void placeFragments(Model &model, Level &level);
    void placeSpectres(Model &model, Level &level);

    // --- Aegis Cloak guarded placement (task 15.9, Requirements 4.13/9.6) --
    // Place the once-per-run Aegis Cloak, guarded by its own Vault Anchor, via
    // the shared placeGuarded helper (kAnyChamber) — the SAME guarded-placement
    // rules as the Shard Coffer. This runs as an additional guarded placement
    // right after placeFragments (so the Aegis's Anchor is set before spectres
    // are counted); it does NOT alter the Requirement 4.1 order of the five core
    // stages. The Aegis's Vault Anchor is SEPARATE from the coffer's and is NOT
    // one of the 20 createRandom spectres (consistent with task 15.3).
    //
    // Placement happens IF AND ONLY IF (Requirements 4.13/9.6):
    //   * model.aegisLevel() is a chosen level in 2..4, AND
    //   * model.levelNumber() == model.aegisLevel() (this IS the chosen level), AND
    //   * !model.aegisSpawnedThisRun() (not already placed this run).
    // On a successful placeGuarded, model.setAegisSpawnedThisRun(true) is set so
    // the Aegis appears EXACTLY ONCE across the whole run (Requirement 9.6) and
    // only on the chosen level (Requirement 4.13). No-op otherwise — in
    // particular when aegisLevel() == 0 (unset), so default-constructed models
    // never receive an Aegis.
    void placeAegisCloak(Model &model, Level &level);

    // --- Cipher Gem carrier selection (task 15.7, Requirement 4.12) ------
    // After the spectres have been placed, choose EXACTLY ONE non-Vault-Anchor
    // Spectre on the level uniformly at random and mark it as the carrier of
    // that level's Cipher Gem (setCarriesCipherGem(true)). The Vault Anchor(s)
    // placed by placeGuarded are excluded from the eligible set — only the
    // createRandom spectres are eligible. Runs immediately after placeSpectres
    // in the populate() pipeline ("when a level's spectres have been placed").
    // No-op if there is no eligible spectre (cannot happen with 20 spectres).
    void selectCipherGemCarrier(Level &level);

    // --- Shared guarded-placement helper (Requirement 4.6, design DQ5) ---
    // Place a guarded `item` on a free floor tile and a Vault Anchor guarding it
    // within a 1-block radius, on a free floor tile in the SAME chamber. Wires
    // the Anchor to the item's tile (anchor->setGuardedItemPosition(itemTile))
    // and marks both cells occupied. Reused by the Shard Coffer (task 15.3) and
    // the Aegis Cloak (task 15.9) so the guarded-placement logic lives in ONE
    // place.
    //
    //   * chamberId == kAnyChamber: choose a chamber uniformly at random first.
    //   * The item tile is chosen so that it has at least one free floor
    //     neighbour in the same chamber for the Anchor; candidate item tiles are
    //     tried in random order until one satisfies that (a sensible fallback if
    //     a particular tile is boxed in — design "Edge cases").
    //
    // Returns a non-owning pointer to the placed GuardedItem (now owned by the
    // Level), or nullptr if no valid item/anchor tile pair could be found.
    GuardedItem *placeGuarded(Level &level, std::unique_ptr<GuardedItem> item,
                              int chamberId);

    // --- Anchor for a pre-placed (file) guarded item (task 19.1) ---------
    // Place a single guarding Vault Anchor for a GuardedItem that ALREADY sits
    // on `itemTile` (e.g. a Shard Coffer read from a layout file), on a free
    // floor tile within a 1-block radius in the SAME chamber, wired to guard the
    // item (Requirement 4.6). Marks the Anchor's tile occupied. No-op (the item
    // is simply left unguarded) if the item is boxed in with no free adjacent
    // floor tile in its chamber — a documented fallback matching placeGuarded's
    // edge-case handling. Used only by populateActorsOnly.
    void placeAnchorForItem(Level &level, const Position &itemTile);
};

} // namespace runevault

#endif // RUNEVAULT_SPAWNER_H
