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
    std::vector<int> chambersWithSpace;
    for (int id : board.chamberIds()) {
        if (id == excludeChamber) {
            continue;
        }
        if (!freeFloorTilesInChamber(board, id).empty()) {
            chambersWithSpace.push_back(id);
        }
    }
    if (chambersWithSpace.empty()) {
        return false;
    }
    chosenChamber = rng::pick(chambersWithSpace);
    std::vector<Position> free = freeFloorTilesInChamber(board, chosenChamber);
    out = rng::pick(free);
    return true;
}

void Spawner::populate(Model &model) {
    Level *level = model.level();
    if (level == nullptr) {
        return;
    }

    reset();

    placeArcanist(model, *level);
    placeStairway(model, *level);
    placeScrolls(model, *level);
    placeFragments(model, *level);
    placeAegisCloak(model, *level);
    placeSpectres(model, *level);

    selectCipherGemCarrier(*level);
}

void Spawner::populateActorsOnly(Model &model) {
    Level *level = model.level();
    if (level == nullptr) {
        return;
    }

    reset();

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

    placeArcanist(model, *level);
    placeStairway(model, *level);

    for (const Position &itemTile : guardedItemTiles) {
        placeAnchorForItem(*level, itemTile);
    }

    placeAegisCloak(model, *level);

    placeSpectres(model, *level);
    selectCipherGemCarrier(*level);
}

void Spawner::placeAnchorForItem(Level &level, const Position &itemTile) {
    const Board &board = level.board();
    const int chamber = board.at(itemTile).chamberId;

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

void Spawner::placeArcanist(Model &model, Level &level) {
    Position tile;
    int chamber = kNoChamber;
    if (pickFreeFloorTileExcludingChamber(level.board(), kNoChamber, tile,
                                          chamber)) {
        model.setArcanistPosition(tile);
        arcanistChamber_ = chamber;
        markOccupied(tile);
    }
}

void Spawner::placeStairway(Model & /*model*/, Level &level) {
    Position tile;
    int chamber = kNoChamber;
    if (pickFreeFloorTileExcludingChamber(level.board(), arcanistChamber_, tile,
                                          chamber)) {
        level.setStairway(tile);
        markOccupied(tile);
    }
}

void Spawner::placeScrolls(Model & /*model*/, Level &level) {
    for (int i = 0; i < kScrollCount; ++i) {
        Position tile;
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

void Spawner::placeFragments(Model & /*model*/, Level &level) {
    placeGuarded(level, std::unique_ptr<GuardedItem>(new ShardCoffer()),
                 kAnyChamber);

    for (int i = 0; i < kFragmentPileCount; ++i) {
        Position tile;
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

int Spawner::chooseAegisLevel() {
    return rng::range(2, 4);
}

void Spawner::placeAegisCloak(Model &model, Level &level) {
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

    GuardedItem *placed =
        placeGuarded(level, std::unique_ptr<GuardedItem>(new AegisCloak()),
                     kAnyChamber);
    if (placed != nullptr) {
        model.setAegisSpawnedThisRun(true);
    }
}

void Spawner::placeSpectres(Model & /*model*/, Level &level) {
    SpectreFactory factory;
    for (int i = 0; i < kSpectreCount; ++i) {
        Position tile;
        if (!pickFreeFloorTile(level.board(), kAnyChamber, tile)) {
            break; // no free floor tile left — stop gracefully (won't occur)
        }
        int chamberId = level.board().at(tile).chamberId;
        level.addSpectre(factory.createRandom(chamberId, tile));
        markOccupied(tile);
    }
}

void Spawner::selectCipherGemCarrier(Level &level) {
    std::vector<Spectre *> eligible;
    for (const std::unique_ptr<Spectre> &spectre : level.spectres()) {
        if (spectre->type() != SpectreType::VaultAnchor) {
            eligible.push_back(spectre.get());
        }
    }
    if (eligible.empty()) {
        return; // no eligible spectre (cannot happen with 20 placed spectres)
    }
    Spectre *carrier = rng::pick(eligible);
    carrier->setCarriesCipherGem(true);
}

GuardedItem *Spawner::placeGuarded(Level &level,
                                   std::unique_ptr<GuardedItem> item,
                                   int chamberId) {
    const Board &board = level.board();

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

        Position anchorTile = rng::pick(anchorTiles);
        SpectreFactory factory;
        std::unique_ptr<Spectre> anchor =
            factory.create(SpectreType::VaultAnchor, chamber, anchorTile);
        anchor->setGuardedItemPosition(itemTile);
        level.addSpectre(std::move(anchor));
        markOccupied(anchorTile);

        return placed;
    }

    return nullptr;
}

} // namespace runevault
