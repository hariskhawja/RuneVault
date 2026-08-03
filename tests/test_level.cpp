// test_level.cpp
// RuneVault — tests for the default fixed layout and Level container (task 2.3).
//
// These assert-based tests (see tests/test_util.h) verify the built-in default
// layout against the spec's Vault Level Structure requirements:
//
//   * exactly 5 chambers with ids {1,2,3,4,5}            (Requirement 3.1)
//   * each chamber interior is 9×4 = 36 floor tiles       (Requirement 3.2)
//   * an archway `+` sits where each corridor meets a wall (Requirement 3.5)
//   * connectivity 1-2, 1-3, 2-4, 3-4, 4-5 with full
//     reachability between all chambers (BFS over walkable) (Req. 3.3 / 3.4)
//   * the whole layout fits the 79×25 playable region      (Requirement 3.8)
//
// plus that `Level` stores and returns the stairway location and revealed
// state.

#include "test_util.h"

#include "runevault/board.h"
#include "runevault/cell.h"
#include "runevault/geometry.h"
#include "runevault/level.h"

#include <algorithm>
#include <queue>
#include <set>
#include <vector>

using namespace runevault;

// Exactly 5 chambers, ids {1,2,3,4,5} (Requirement 3.1).
RV_TEST(default_board_has_five_chambers) {
    Board board = makeDefaultBoard();
    std::vector<int> ids = board.chamberIds();
    std::vector<int> expected{1, 2, 3, 4, 5};
    RV_CHECK(ids == expected);
}

// Each chamber has a 9×4 = 36 floor-tile interior (Requirement 3.2).
RV_TEST(default_board_each_chamber_has_36_floor_tiles) {
    Board board = makeDefaultBoard();
    for (int id = 1; id <= kChamberCount; ++id) {
        RV_CHECK(static_cast<int>(board.chamberFloorTiles(id).size()) == 36);
    }
}

// The layout fits within the 79×25 playable region (Requirement 3.8), and every
// chamber floor tile lies inside those bounds.
RV_TEST(default_board_fits_playable_region) {
    Board board = makeDefaultBoard();
    RV_CHECK(board.rows() == kPlayableRows);
    RV_CHECK(board.cols() == kPlayableCols);
    RV_CHECK(kPlayableRows == 25);
    RV_CHECK(kPlayableCols == 79);

    for (int id = 1; id <= kChamberCount; ++id) {
        for (const Position &p : board.chamberFloorTiles(id)) {
            RV_CHECK(board.inBounds(p));
        }
    }
}

// An archway `+` sits on the chamber wall wherever a corridor connects
// (Requirement 3.5). Check both endpoints of all five corridors.
RV_TEST(default_board_has_archways_at_corridor_junctions) {
    Board board = makeDefaultBoard();

    const Position archways[] = {
        {2, 10}, {2, 19},   // 1-2
        {5, 3},  {8, 3},    // 1-3
        {5, 25}, {8, 25},   // 2-4
        {10, 10}, {10, 19}, // 3-4
        {13, 23}, {16, 23}  // 4-5
    };
    for (const Position &p : archways) {
        RV_CHECK(board.at(p).type == CellType::Archway);
    }
}

// Every chamber is reachable from every other (Requirement 3.4). Flood-fill
// from a floor tile in chamber 1 over all walkable cells (floor / archway /
// corridor / stairway) and assert all 5 chambers are reached.
RV_TEST(default_board_all_chambers_reachable) {
    Board board = makeDefaultBoard();

    std::vector<Position> start = board.chamberFloorTiles(1);
    RV_CHECK(!start.empty());
    if (start.empty()) {
        return;
    }

    std::set<std::pair<int, int>> visited;
    std::queue<Position> frontier;
    auto push = [&](const Position &p) {
        auto key = std::make_pair(p.row, p.col);
        if (visited.find(key) == visited.end()) {
            visited.insert(key);
            frontier.push(p);
        }
    };

    push(start.front());
    std::set<int> reachedChambers;
    while (!frontier.empty()) {
        Position cur = frontier.front();
        frontier.pop();
        const Cell &cell = board.at(cur);
        if (cell.chamberId != kNoChamber) {
            reachedChambers.insert(cell.chamberId);
        }
        // Walkable neighbours already excludes walls and off-board cells.
        for (const Position &n : board.walkableNeighbours(cur)) {
            push(n);
        }
    }

    std::set<int> expected{1, 2, 3, 4, 5};
    RV_CHECK(reachedChambers == expected);
}

// Level stores and returns the stairway location and revealed state.
RV_TEST(level_stores_stairway_and_revealed_state) {
    Level level(makeDefaultBoard());

    // Defaults: hidden stairway.
    RV_CHECK(level.stairwayRevealed() == false);

    // The owned board is the default layout (5 chambers).
    RV_CHECK(static_cast<int>(level.board().chamberIds().size()) == 5);

    Position stair{3, 5};
    level.setStairway(stair);
    RV_CHECK(level.stairway() == stair);

    level.setStairwayRevealed(true);
    RV_CHECK(level.stairwayRevealed() == true);
}
