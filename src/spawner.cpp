// spawner.cpp
// RuneVault — the random spawning pipeline implementation (task 15.1).
//
// See spawner.h for the full contract. This translation unit implements the
// spec-ordered pipeline (Requirement 4.1), the occupancy invariant machinery
// (Requirement 4.10), floor-tile-only placement (Requirement 4.9), and the
// shared guarded-placement helper that positions a Vault Anchor within a
// 1-block radius of the item it guards (Requirement 4.6). All randomness routes
// through the provided `rng` utilities so a seeded run is reproducible
// (Requirement 4.11, verified later by task 15.11).

#include "runevault/spawner.h"

#include "runevault/cell.h"
#include "runevault/item.h"
#include "runevault/scroll_type.h"
#include "runevault/spectre.h"
#include "runevault/spectre_factory.h"

#include "shuffle.h"

#include <algorithm>
#include <vector>

namespace runevault {

// ── Occupancy tracking (Requirement 4.10) ───────────────────────────────────

void Spawner::reset() {
    occupied_.clear();
    arcanistChamber_ = kNoChamber;
}

bool Spawner::isOccupied(const Position &p) const {
    return occupied_.count(std::make_pair(p.row, p.col)) != 0;
}

void Spawner::markOccupied(const Position &p) {
    occupied_.insert(std::make_pair(p.row, p.col));
}

bool Spawner::isFreeFloorTile(const Board &board, const Position &p) const {
    // Requirement 4.9: only floor tiles may host actors/items — canHost() is
    // true for CellType::Floor only, so archways, corridors, walls, and the
    // stairway tile are all rejected here. Requirement 4.10: reject any tile
    // already claimed by a previously placed object.
    if (!board.inBounds(p)) {
        return false;
    }
    if (!board.at(p).canHost()) {
        return false;
    }
    return !isOccupied(p);
}

std::vector<Position>
Spawner::freeFloorTilesInChamber(const Board &board, int chamberId) const {
    std::vector<Position> free;
    for (const Position &tile : board.chamberFloorTiles(chamberId)) {
        if (!isOccupied(tile)) {
            free.push_back(tile);
        }
    }
    return free;
}

bool Spawner::pickFreeFloorTile(const Board &board, int chamberId,
                                Position &out) const {
    if (chamberId != kAnyChamber) {
        std::vector<Position> free = freeFloorTilesInChamber(board, chamberId);
        if (free.empty()) {
            return false;
        }
        out = rng::pick(free); // uniform within the chamber
        return true;
    }

    // kAnyChamber: choose a chamber uniformly at random from those that still
    // have a free floor tile (Requirement 4.8 — every chamber equally likely),
    // then a free tile within it uniformly.
    std::vector<int> chambersWithSpace;
    for (int id : board.chamberIds()) {
        if (!freeFloorTilesInChamber(board, id).empty()) {
            chambersWithSpace.push_back(id);
        }
    }
    if (chambersWithSpace.empty()) {
        return false;
    }
    int chamber = rng::pick(chambersWithSpace);
    std::vector<Position> free = freeFloorTilesInChamber(board, chamber);
    out = rng::pick(free);
    return true;
}

bool Spawner::pickFreeFloorTileExcludingChamber(const Board &board,
                                                int excludeChamber,
                                                Position &out,
                                                int &chosenChamber) const {
    // Requirements 4.2/4.3: choose the CHAMBER uniformly at random among those
    // that still have a free floor tile, skipping `excludeChamber`, then a free
    // floor tile within the chosen chamber uniformly. This is how the Arcanist
    // (exclude nothing) and the stairway (exclude the Arcanist's chamber) are
    // placed so their chambers necessarily differ.
    std::vector<int> chambersWithSpace;
    for (int id : board.chamberIds()) {
        if (id == excludeChamber) {
            continue; // Requirement 4.3: never reuse the excluded chamber
        }
        if (!freeFloorTilesInChamber(board, id).empty()) {
            chambersWithSpace.push_back(id);
        }
    }
    if (chambersWithSpace.empty()) {
        return false;
    }
    chosenChamber = rng::pick(chambersWithSpace); // chamber uniform (Req 4.2)
    std::vector<Position> free = freeFloorTilesInChamber(board, chosenChamber);
    out = rng::pick(free); // tile uniform within the chamber (Req 4.2)
    return true;
}

// ── Pipeline entry point (Requirement 4.1 order) ────────────────────────────

void Spawner::populate(Model &model) {
    Level *level = model.level();
    if (level == nullptr) {
        return;
    }

    reset();

    // Requirement 4.1: strict order — Arcanist, stairway, scrolls, rune
    // fragments, spectres. Each stage is invoked exactly once, in this order.
    placeArcanist(model, *level);
    placeStairway(model, *level);
    placeScrolls(model, *level);
    placeFragments(model, *level);
    // Requirements 4.13/9.6: the once-per-run Aegis Cloak is an ADDITIONAL
    // guarded placement, done right after the fragments (the coffer) so its
    // Vault Anchor is set before the spectres are placed/counted. It is a no-op
    // on every level except the run-selected aegisLevel() in 2..4, and only the
    // first time that level is populated (see placeAegisCloak). This does not
    // change the Requirement 4.1 order of the five core stages.
    placeAegisCloak(model, *level);
    placeSpectres(model, *level);

    // Requirement 4.12: once the spectres are placed, pick exactly one
    // non-Vault-Anchor Spectre uniformly at random to carry the Cipher Gem.
    // This is a post-spectre selection, not a sixth pipeline stage, and so runs
    // immediately after placeSpectres while leaving the placement order intact.
    selectCipherGemCarrier(*level);
}

// ── Actors-only pass for file-driven levels (task 19.1, Requirement 12.1) ───

void Spawner::populateActorsOnly(Model &model) {
    Level *level = model.level();
    if (level == nullptr) {
        return;
    }

    reset();

    // Requirement 4.10: the file already placed its scrolls / fragments / Shard
    // Coffers on floor tiles. Mark every one occupied FIRST so the actors placed
    // below never land on top of a file item (one object per cell). Also collect
    // the guarded-item (Shard Coffer) tiles so each can be given a guarding Vault
    // Anchor after the Arcanist/stairway are placed.
    std::vector<Position> guardedItemTiles;
    for (const std::unique_ptr<Item> &item : level->items()) {
        if (item == nullptr) {
            continue;
        }
        markOccupied(item->position());
        if (dynamic_cast<GuardedItem *>(item.get()) != nullptr) {
            guardedItemTiles.push_back(item->position());
        }
    }

    // Requirement 4.1 order (adapted — scrolls/fragments come from the file):
    // Arcanist, then stairway (different chamber), then the guards for the file
    // coffers, then the Aegis (chosen level only), then spectres, then carrier.
    placeArcanist(model, *level);
    placeStairway(model, *level);

    // Requirement 4.6: give each file Shard Coffer its own guarding Vault Anchor
    // within a 1-block radius in the same chamber, wired to the coffer's tile.
    for (const Position &itemTile : guardedItemTiles) {
        placeAnchorForItem(*level, itemTile);
    }

    // Requirements 4.13/9.6: the Aegis Cloak is never encoded in a layout file,
    // so it is still spawned here (guarded) on the run-chosen level in 2..4.
    placeAegisCloak(model, *level);

    // Requirement 4.7: 20 spectres via the factory distribution. Requirement
    // 4.12: then choose the single Cipher Gem carrier among the non-Anchors.
    placeSpectres(model, *level);
    selectCipherGemCarrier(*level);
}

void Spawner::placeAnchorForItem(Level &level, const Position &itemTile) {
    const Board &board = level.board();
    const int chamber = board.at(itemTile).chamberId;

    // Requirement 4.6: the Anchor must sit within a 1-block radius of the item
    // it guards, in the SAME chamber. floorNeighboursInChamber restricts to both
    // constraints; filter to still-free tiles.
    std::vector<Position> anchorTiles;
    for (const Position &n : board.floorNeighboursInChamber(itemTile, chamber)) {
        if (!isOccupied(n)) {
            anchorTiles.push_back(n);
        }
    }
    if (anchorTiles.empty()) {
        return; // boxed in — leave this coffer unguarded (documented fallback).
    }

    const Position anchorTile = rng::pick(anchorTiles);
    SpectreFactory factory;
    std::unique_ptr<Spectre> anchor =
        factory.create(SpectreType::VaultAnchor, chamber, anchorTile);
    anchor->setGuardedItemPosition(itemTile); // hostile only near this coffer
    level.addSpectre(std::move(anchor));
    markOccupied(anchorTile);
}

// ── Stage 1: Arcanist location ──────────────────────────────────────────────

void Spawner::placeArcanist(Model &model, Level &level) {
    // Requirement 4.2: place the Arcanist on a floor tile, choosing the chamber
    // uniformly at random (among chambers with a free floor tile) and then a
    // free floor tile within it uniformly. Pass kNoChamber as the excluded
    // chamber (exclude nothing) and record the chosen chamber in
    // arcanistChamber_ so placeStairway can keep the stairway in a DIFFERENT
    // chamber (Requirement 4.3).
    Position tile;
    int chamber = kNoChamber;
    if (pickFreeFloorTileExcludingChamber(level.board(), kNoChamber, tile,
                                          chamber)) {
        model.setArcanistPosition(tile);
        arcanistChamber_ = chamber;
        markOccupied(tile);
    }
}

// ── Stage 2: stairway location ──────────────────────────────────────────────

void Spawner::placeStairway(Model & /*model*/, Level &level) {
    // Requirement 4.3: place the stairway on a floor tile in a chamber that is
    // DIFFERENT from the Arcanist's chamber, choosing that chamber uniformly at
    // random and the tile within it uniformly (Requirement 4.2). placeArcanist
    // ran first (Requirement 4.1 order), so arcanistChamber_ is set; excluding
    // it guarantees the stairway lands in another chamber. With 5 chambers and
    // only one excluded, an eligible chamber always exists. Marking the tile
    // occupied keeps later stages off the stairway (Requirement 4.9 — items and
    // spectres never spawn on the stairway).
    Position tile;
    int chamber = kNoChamber;
    if (pickFreeFloorTileExcludingChamber(level.board(), arcanistChamber_, tile,
                                          chamber)) {
        level.setStairway(tile);
        markOccupied(tile);
    }
}

// ── Stage 3: scrolls ────────────────────────────────────────────────────────

void Spawner::placeScrolls(Model & /*model*/, Level &level) {
    // Requirement 4.4: place EXACTLY 10 scrolls, each on a chamber-uniform free
    // floor tile (Requirement 4.8 via kAnyChamber). Each scroll's TYPE is chosen
    // from the 6 ScrollTypes with equal probability 1/6. Guard each placement
    // against the (here impossible) case of running out of free floor tiles.
    for (int i = 0; i < kScrollCount; ++i) {
        Position tile;
        // writes random position into tile
        if (!pickFreeFloorTile(level.board(), kAnyChamber, tile)) {
            break; // no free floor tile left — stop gracefully (won't occur)
        }
        // 6 scroll types, enumerators 0..5 (scroll_type.h). rng::range is
        // inclusive, so [0, 5] selects each type with probability 1/6.
        auto type = static_cast<ScrollType>(rng::range(0, 5));
        auto scroll = std::unique_ptr<Scroll>(new Scroll(type));
        scroll->setPosition(tile);
        level.addItem(std::move(scroll));
        markOccupied(tile);
    }
}

// ── Stage 4: rune fragments (incl. the guarded Shard Coffer) ────────────────

void Spawner::placeFragments(Model & /*model*/, Level &level) {
    // Requirement 4.5: place EXACTLY 1 Shard Coffer guarded by a Vault Anchor
    // (via the shared placeGuarded helper, which also positions and wires the
    // Anchor within a 1-block radius — Requirement 4.6), PLUS EXACTLY 9
    // additional fragment piles.
    placeGuarded(level, std::unique_ptr<GuardedItem>(new ShardCoffer()),
                 kAnyChamber);

    // Requirement 4.5: each of the 9 piles is a common shard with probability
    // 5/7 or a resonant shard with probability 2/7. rng::weighted({5, 2})
    // returns index 0 with probability 5/7 and index 1 with probability 2/7.
    // Each pile is placed on a chamber-uniform free floor tile (Requirement
    // 4.8), guarded against running out of tiles (won't occur at these counts).
    for (int i = 0; i < kFragmentPileCount; ++i) {
        Position tile;
        // writes random position into tile
        if (!pickFreeFloorTile(level.board(), kAnyChamber, tile)) {
            break; // no free floor tile left — stop gracefully (won't occur)
        }
        FragmentGrade grade = (rng::weighted({5.0, 2.0}) == 0)
                                  ? FragmentGrade::CommonShard
                                  : FragmentGrade::ResonantShard;
        auto frag = std::unique_ptr<RuneFragment>(new RuneFragment(grade));
        frag->setPosition(tile);
        level.addItem(std::move(frag));
        markOccupied(tile);
    }
}

// ── Aegis Cloak: run-start selection + guarded placement (task 15.9) ────────

int Spawner::chooseAegisLevel() {
    // Requirement 4.13: the Aegis Cloak spawns on ONE randomly-selected level in
    // the range 2..4. rng::range is inclusive, so [2, 4] is uniform over
    // {2, 3, 4}. Routing through rng keeps the selection reproducible for a
    // given seed (Requirement 4.11). Called once at run start by the run
    // lifecycle (tasks 16/19), which stores it via model.setAegisLevel(...).
    return rng::range(2, 4);
}

void Spawner::placeAegisCloak(Model &model, Level &level) {
    // Requirements 4.13/9.6: place the Aegis Cloak EXACTLY ONCE PER RUN, on the
    // run-selected level in 2..4, guarded by its own Vault Anchor via the shared
    // placeGuarded helper (same rules as the Shard Coffer). Place IF AND ONLY IF:
    //   * a level was chosen and it is in 2..4 (aegisLevel() == 0 means unset), AND
    //   * this IS that chosen level (levelNumber() == aegisLevel()), AND
    //   * the Aegis has not already been spawned this run.
    // The three-way guard guarantees "only on the chosen level" (Req 4.13) and,
    // together with the aegisSpawnedThisRun latch set below, "exactly once per
    // run" (Req 9.6). A default-constructed Model has aegisLevel() == 0, so this
    // is a no-op there (existing default-model tests are unaffected).
    int aegisLevel = model.aegisLevel();
    if (aegisLevel < 2 || aegisLevel > 4) {
        return; // unset (0) or out of range — no Aegis this level
    }
    if (model.levelNumber() != aegisLevel) {
        return; // not the chosen level
    }
    if (model.aegisSpawnedThisRun()) {
        return; // already placed this run — never place a second Aegis
    }

    // Reuse the shared guarded-placement helper so the Aegis's Vault Anchor lands
    // within a 1-block radius in the same chamber (Requirement 4.6), exactly like
    // the coffer. On success, latch the once-per-run flag (Requirement 9.6).
    GuardedItem *placed =
        placeGuarded(level, std::unique_ptr<GuardedItem>(new AegisCloak()),
                     kAnyChamber);
    if (placed != nullptr) {
        model.setAegisSpawnedThisRun(true);
    }
}

// ── Stage 5: spectres ───────────────────────────────────────────────────────

void Spawner::placeSpectres(Model & /*model*/, Level &level) {
    // Requirement 4.7: place EXACTLY 20 Spectres via SpectreFactory::createRandom,
    // which already applies the spec distribution over the six Spectre kinds and
    // never yields a Vault Anchor. The Vault Anchor(s) placed by placeGuarded (the
    // Shard Coffer's guard, and later the Aegis Cloak's in task 15.9) are SEPARATE
    // and are NOT counted among these 20.
    // The Cipher Gem carrier (Requirement 4.12) is chosen AFTER this stage by
    // selectCipherGemCarrier, invoked from populate() once all 20 spectres are
    // placed ("when a level's spectres have been placed").
    // The Aegis Cloak's guarded placement on the run-selected level in 2..4
    // (Requirement 4.13) is handled by placeAegisCloak, invoked from populate()
    // BEFORE this stage (right after placeFragments), reusing placeGuarded. Its
    // Vault Anchor is separate from these 20 createRandom spectres.
    SpectreFactory factory;
    for (int i = 0; i < kSpectreCount; ++i) {
        // Requirement 4.8: choose a chamber uniformly, then a free tile within
        // it uniformly. pickFreeFloorTile(kAnyChamber) does exactly that and
        // also reports the chamber via the tile's cell membership.
        Position tile;
        // writes random position into tile
        if (!pickFreeFloorTile(level.board(), kAnyChamber, tile)) {
            break; // no free floor tile left — stop gracefully (won't occur)
        }
        int chamberId = level.board().at(tile).chamberId;
        level.addSpectre(factory.createRandom(chamberId, tile));
        markOccupied(tile);
    }
}

// ── Cipher Gem carrier selection (task 15.7, Requirement 4.12) ──────────────

void Spawner::selectCipherGemCarrier(Level &level) {
    // Requirement 4.12: "when a level's spectres have been placed, choose
    // exactly one non-Vault-Anchor spectre uniformly at random to carry the
    // Cipher Gem for that level." Collect pointers to every eligible Spectre —
    // i.e. every Spectre that is NOT a Vault Anchor (the Anchor(s) guarding
    // coffers via placeGuarded are excluded) — then pick one uniformly and flag
    // it as the carrier. The 20 createRandom spectres are all non-anchors, so
    // the eligible set is non-empty in practice; guard the (impossible) empty
    // case so nothing is picked from an empty range.
    std::vector<Spectre *> eligible;
    for (const std::unique_ptr<Spectre> &spectre : level.spectres()) {
        if (spectre->type() != SpectreType::VaultAnchor) {
            eligible.push_back(spectre.get());
        }
    }
    if (eligible.empty()) {
        return; // no eligible spectre (cannot happen with 20 placed spectres)
    }
    // Uniform pick over the eligible spectres, then mark exactly this one as the
    // Cipher Gem carrier — every other spectre keeps the default (false), so the
    // level has exactly one carrier.
    Spectre *carrier = rng::pick(eligible);
    carrier->setCarriesCipherGem(true);
}

// ── Shared guarded-placement helper (Requirement 4.6, design DQ5) ───────────

GuardedItem *Spawner::placeGuarded(Level &level,
                                   std::unique_ptr<GuardedItem> item,
                                   int chamberId) {
    const Board &board = level.board();

    // Resolve the target chamber. For kAnyChamber, choose a chamber uniformly
    // at random from those with a free floor tile (Requirement 4.8).
    int chamber = chamberId;
    if (chamber == kAnyChamber) {
        std::vector<int> chambersWithSpace;
        for (int id : board.chamberIds()) {
            if (!freeFloorTilesInChamber(board, id).empty()) {
                chambersWithSpace.push_back(id);
            }
        }
        if (chambersWithSpace.empty()) {
            return nullptr;
        }
        chamber = rng::pick(chambersWithSpace);
    }

    // Candidate item tiles: every free floor tile of the chamber, tried in
    // random order until one has a free floor NEIGHBOUR in the same chamber for
    // the Anchor (a sensible fallback when a tile is boxed in — design "Edge
    // cases"). floorNeighboursInChamber restricts to the 1-block radius AND the
    // same chamber, so the Anchor is guaranteed within a 1-block radius of the
    // item (Requirement 4.6).
    std::vector<Position> candidates = freeFloorTilesInChamber(board, chamber);
    rng::shuffle(candidates.begin(), candidates.end());

    for (const Position &itemTile : candidates) {
        std::vector<Position> anchorTiles;
        for (const Position &n :
             board.floorNeighboursInChamber(itemTile, chamber)) {
            if (!isOccupied(n)) {
                anchorTiles.push_back(n);
            }
        }
        if (anchorTiles.empty()) {
            continue; // no adjacent free tile — try another item tile
        }

        // Place the item on itemTile.
        item->setPosition(itemTile);
        GuardedItem *placed = item.get();
        level.addItem(std::move(item));
        markOccupied(itemTile);

        // Place the guarding Vault Anchor on a random adjacent free tile in the
        // same chamber and wire it to guard the item (Requirement 4.6). The
        // Anchor's hostility keys off guardedItemPosition() (task 10.5).
        Position anchorTile = rng::pick(anchorTiles);
        SpectreFactory factory;
        std::unique_ptr<Spectre> anchor =
            factory.create(SpectreType::VaultAnchor, chamber, anchorTile);
        anchor->setGuardedItemPosition(itemTile);
        level.addSpectre(std::move(anchor));
        markOccupied(anchorTile);

        return placed;
    }

    // No item tile had a free adjacent tile for the Anchor. The caller keeps
    // ownership implicitly dropped (item was never moved out on this path only
    // if we reached here without moving — but since we move inside the loop,
    // reaching here means `item` is still held and is destroyed on return).
    return nullptr;
}

} // namespace runevault
