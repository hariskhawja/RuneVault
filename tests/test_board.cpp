// test_board.cpp
// RuneVault — Board grid, chamber membership, and adjacency tests (task 2.2).
//
// Uses the assert-based harness (test_util.h): each RV_TEST auto-registers and
// the shared runner in test_main.cpp drives them. Covers board dimensions,
// in-bounds vs out-of-bounds, setting/reading cells, chamber floor-tile
// enumeration for a small hand-built board, and adjacency / walkable-neighbour
// queries (Requirements 2.1, 2.2, 3.1, 3.2, 3.7, 3.8).

#include "test_util.h"

#include "runevault/board.h"

#include <algorithm>

using namespace runevault;

namespace {

// True iff `positions` contains `p` (order-independent membership check).
bool contains(const std::vector<Position> &positions, const Position &p) {
    return std::find(positions.begin(), positions.end(), p) != positions.end();
}

} // namespace

RV_TEST(board_default_dimensions_match_playable_region) {
    Board b;
    // 79 columns × 25 rows playable region (Requirements 2.1, 2.2, 3.8 / DD-5).
    RV_CHECK(b.rows() == 25);
    RV_CHECK(b.cols() == 79);
    RV_CHECK(kPlayableRows == 25);
    RV_CHECK(kPlayableCols == 79);
}

RV_TEST(board_starts_all_wall_and_layout_agnostic) {
    Board b(4, 5);
    RV_CHECK(b.rows() == 4);
    RV_CHECK(b.cols() == 5);
    // Every cell defaults to a Wall in no chamber until a layout is populated.
    for (int r = 0; r < b.rows(); ++r) {
        for (int c = 0; c < b.cols(); ++c) {
            const Cell &cell = b.at(Position{r, c});
            RV_CHECK(cell.type == CellType::Wall);
            RV_CHECK(cell.chamberId == kNoChamber);
        }
    }
    // No chambers present on an all-wall board.
    RV_CHECK(b.chamberIds().empty());
}

RV_TEST(board_in_bounds_vs_out_of_bounds) {
    Board b(3, 4); // rows 0..2, cols 0..3

    RV_CHECK(b.inBounds(Position{0, 0}));
    RV_CHECK(b.inBounds(Position{2, 3}));
    RV_CHECK(b.inBounds(Position{1, 2}));

    RV_CHECK(!b.inBounds(Position{-1, 0}));
    RV_CHECK(!b.inBounds(Position{0, -1}));
    RV_CHECK(!b.inBounds(Position{3, 0}));  // row past last
    RV_CHECK(!b.inBounds(Position{0, 4}));  // col past last
}

RV_TEST(board_at_throws_out_of_range_off_board) {
    Board b(2, 2);
    bool threw = false;
    try {
        (void)b.at(Position{5, 5});
    } catch (const std::out_of_range &) {
        threw = true;
    }
    RV_CHECK(threw);
}

RV_TEST(board_set_and_read_cells) {
    Board b(3, 3);

    b.setCell(Position{1, 1}, CellType::Floor, 2);
    const Cell &floor = b.at(Position{1, 1});
    RV_CHECK(floor.type == CellType::Floor);
    RV_CHECK(floor.chamberId == 2);
    RV_CHECK(floor.canHost());
    RV_CHECK(floor.isWalkable());

    // Cell-overload setter and corridor semantics (walkable but no chamber).
    b.setCell(Position{0, 0}, Cell{CellType::Corridor, kNoChamber});
    const Cell &corridor = b.at(Position{0, 0});
    RV_CHECK(corridor.type == CellType::Corridor);
    RV_CHECK(corridor.isWalkable());
    RV_CHECK(!corridor.canHost());
}

RV_TEST(board_set_cell_off_board_throws) {
    Board b(2, 2);
    bool threw = false;
    try {
        b.setCell(Position{-1, 0}, CellType::Floor, 1);
    } catch (const std::out_of_range &) {
        threw = true;
    }
    RV_CHECK(threw);
}

// Build a tiny board with two chambers to exercise chamber enumeration and the
// chamber-scoped adjacency queries.
//
//   col:   0        1        2        3
//   row 0: floor(1) floor(1) corridor floor(2)
//   row 1: floor(1) wall     archway  floor(2)
RV_TEST(board_chamber_enumeration_and_floor_tiles) {
    Board b(2, 4);
    b.setCell(Position{0, 0}, CellType::Floor, 1);
    b.setCell(Position{0, 1}, CellType::Floor, 1);
    b.setCell(Position{0, 2}, CellType::Corridor, kNoChamber);
    b.setCell(Position{0, 3}, CellType::Floor, 2);
    b.setCell(Position{1, 0}, CellType::Floor, 1);
    b.setCell(Position{1, 1}, CellType::Wall, kNoChamber);
    b.setCell(Position{1, 2}, CellType::Archway, kNoChamber);
    b.setCell(Position{1, 3}, CellType::Floor, 2);

    std::vector<int> ids = b.chamberIds();
    RV_CHECK(ids.size() == 2);
    RV_CHECK(ids[0] == 1); // sorted ascending
    RV_CHECK(ids[1] == 2);

    std::vector<Position> chamber1 = b.chamberFloorTiles(1);
    RV_CHECK(chamber1.size() == 3);
    RV_CHECK(contains(chamber1, Position{0, 0}));
    RV_CHECK(contains(chamber1, Position{0, 1}));
    RV_CHECK(contains(chamber1, Position{1, 0}));

    std::vector<Position> chamber2 = b.chamberFloorTiles(2);
    RV_CHECK(chamber2.size() == 2);
    RV_CHECK(contains(chamber2, Position{0, 3}));
    RV_CHECK(contains(chamber2, Position{1, 3}));

    // The corridor/archway tiles belong to no chamber.
    RV_CHECK(b.chamberFloorTiles(kNoChamber).empty());
    RV_CHECK(b.chamberFloorTiles(3).empty()); // absent chamber id
}

RV_TEST(board_walkable_neighbours_excludes_walls_and_off_board) {
    // Center is a floor tile; surround it with a mix of walls, floors, and a
    // corridor, and place the tile near the top-left so some neighbours are
    // off-board.
    Board b(3, 3);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            b.setCell(Position{r, c}, CellType::Wall, kNoChamber);
        }
    }
    b.setCell(Position{1, 1}, CellType::Floor, 1);
    b.setCell(Position{0, 1}, CellType::Floor, 1);     // north — walkable
    b.setCell(Position{1, 2}, CellType::Corridor, kNoChamber); // east — walkable
    // remaining neighbours stay Wall (not walkable)

    std::vector<Position> walkable = b.walkableNeighbours(Position{1, 1});
    RV_CHECK(walkable.size() == 2);
    RV_CHECK(contains(walkable, Position{0, 1}));
    RV_CHECK(contains(walkable, Position{1, 2}));
    RV_CHECK(!contains(walkable, Position{2, 2})); // wall neighbour excluded

    // A corner position: 5 of its 8 neighbours are off-board and pruned.
    std::vector<Position> cornerWalkable = b.walkableNeighbours(Position{0, 0});
    for (const Position &p : cornerWalkable) {
        RV_CHECK(b.inBounds(p));
    }
    RV_CHECK(contains(cornerWalkable, Position{0, 1})); // floor to the east
}

RV_TEST(board_floor_neighbours_in_chamber_confines_to_chamber) {
    // A floor cell in chamber 1 with neighbours in chamber 1, chamber 2, a
    // corridor, and a wall. Only same-chamber floor neighbours qualify.
    Board b(3, 3);
    b.setCell(Position{1, 1}, CellType::Floor, 1); // center
    b.setCell(Position{0, 1}, CellType::Floor, 1); // north  — chamber 1 floor
    b.setCell(Position{1, 0}, CellType::Floor, 1); // west   — chamber 1 floor
    b.setCell(Position{1, 2}, CellType::Floor, 2); // east   — different chamber
    b.setCell(Position{0, 0}, CellType::Corridor, kNoChamber); // nw — corridor
    b.setCell(Position{2, 2}, CellType::Wall, kNoChamber);     // se — wall
    // (unset neighbours remain default Wall)

    std::vector<Position> candidates =
        b.floorNeighboursInChamber(Position{1, 1}, 1);
    RV_CHECK(candidates.size() == 2);
    RV_CHECK(contains(candidates, Position{0, 1}));
    RV_CHECK(contains(candidates, Position{1, 0}));
    RV_CHECK(!contains(candidates, Position{1, 2})); // chamber 2 excluded
    RV_CHECK(!contains(candidates, Position{0, 0})); // corridor excluded

    // Querying chamber 2 from the same center yields only the eastern tile.
    std::vector<Position> chamber2 =
        b.floorNeighboursInChamber(Position{1, 1}, 2);
    RV_CHECK(chamber2.size() == 1);
    RV_CHECK(contains(chamber2, Position{1, 2}));

    RV_CHECK(b.floorNeighboursInChamber(Position{1, 1}, kNoChamber).empty());
}
