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
    static constexpr int kAnyChamber = 0;

    static constexpr int kScrollCount = 10;
    static constexpr int kFragmentPileCount = 9;
    static constexpr int kSpectreCount = 20;

    void populate(Model &model);

    void populateActorsOnly(Model &model);

    static int chooseAegisLevel();

private:
    std::set<std::pair<int, int>> occupied_;

    // Reset occupancy at the start of a populate() run.
    void reset();

    bool isOccupied(const Position &p) const;
    void markOccupied(const Position &p);

    bool isFreeFloorTile(const Board &board, const Position &p) const;

    // Collect every currently-free floor tile of `chamberId`, in row-major
    // order (Board::chamberFloorTiles order minus occupied tiles).
    std::vector<Position> freeFloorTilesInChamber(const Board &board,
                                                  int chamberId) const;

    bool pickFreeFloorTile(const Board &board, int chamberId,
                           Position &out) const;

    bool pickFreeFloorTileExcludingChamber(const Board &board,
                                           int excludeChamber, Position &out,
                                           int &chosenChamber) const;

    int arcanistChamber_ = kNoChamber;

    void placeArcanist(Model &model, Level &level);
    void placeStairway(Model &model, Level &level);
    void placeScrolls(Model &model, Level &level);
    void placeFragments(Model &model, Level &level);
    void placeSpectres(Model &model, Level &level);

    void placeAegisCloak(Model &model, Level &level);

    void selectCipherGemCarrier(Level &level);

    GuardedItem *placeGuarded(Level &level, std::unique_ptr<GuardedItem> item,
                              int chamberId);

    void placeAnchorForItem(Level &level, const Position &itemTile);
};

} // namespace runevault

#endif // RUNEVAULT_SPAWNER_H
