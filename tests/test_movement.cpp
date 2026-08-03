// test_movement.cpp
// RuneVault — Arcanist 8-direction movement with walkability checks (task 6.1).
//
// Exercises Model::moveArcanist against the default layout (level.cpp). Chamber
// 1's floor interior spans rows 1..4, cols 1..9 with a wall perimeter; an
// archway sits at (2,10) and a corridor runs east from (2,11). We verify:
//   * moving onto floor / archway / corridor succeeds and updates position
//     (Requirements 5.1, 5.2);
//   * moving into a wall returns false and leaves the position unchanged
//     (Requirement 5.3);
//   * moving out of the playable region returns false and leaves the position
//     unchanged (Requirement 5.3);
//   * all 8 directions step correctly from an open interior tile.

#include "test_util.h"

#include "runevault/board.h"
#include "runevault/geometry.h"
#include "runevault/level.h"
#include "runevault/model.h"

#include <memory>

namespace {

// Build a Model owning a default-layout level, with the Arcanist placed at `p`.
runevault::Model makeModelAt(const runevault::Position &p) {
    using namespace runevault;
    Model model;
    model.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model.setArcanistPosition(p);
    return model;
}

} // namespace

// --- Requirement 5.1 / 5.2: entering floor updates position ----------------
RV_TEST(move_onto_floor_succeeds_and_updates_position) {
    using namespace runevault;
    Model model = makeModelAt(Position{2, 5}); // open chamber-1 interior floor
    RV_CHECK(model.level()->board().at(Position{2, 6}).type == CellType::Floor);

    RV_CHECK(model.moveArcanist(Direction::Ea) == true);
    RV_CHECK(model.arcanistPosition() == (Position{2, 6}));
}

// --- Requirement 5.2: archway and corridor are walkable --------------------
RV_TEST(move_onto_archway_then_corridor_succeeds) {
    using namespace runevault;
    Model model = makeModelAt(Position{2, 9}); // floor at east edge of chamber 1
    RV_CHECK(model.level()->board().at(Position{2, 10}).type ==
             CellType::Archway);
    RV_CHECK(model.level()->board().at(Position{2, 11}).type ==
             CellType::Corridor);

    // Floor -> archway (Requirement 5.2).
    RV_CHECK(model.moveArcanist(Direction::Ea) == true);
    RV_CHECK(model.arcanistPosition() == (Position{2, 10}));
    // Archway -> corridor (Requirement 5.2).
    RV_CHECK(model.moveArcanist(Direction::Ea) == true);
    RV_CHECK(model.arcanistPosition() == (Position{2, 11}));
}

// --- Requirement 5.3: moving into a wall is rejected, turn not consumed -----
RV_TEST(move_into_wall_is_rejected_and_position_unchanged) {
    using namespace runevault;
    Model model = makeModelAt(Position{1, 1}); // top-left interior corner
    // North of (1,1) is the chamber's top wall (row 0); west is the left wall.
    RV_CHECK(model.level()->board().at(Position{0, 1}).type == CellType::Wall);
    RV_CHECK(model.level()->board().at(Position{1, 0}).type == CellType::Wall);

    RV_CHECK(model.moveArcanist(Direction::No) == false);
    RV_CHECK(model.arcanistPosition() == (Position{1, 1}));
    RV_CHECK(model.moveArcanist(Direction::We) == false);
    RV_CHECK(model.arcanistPosition() == (Position{1, 1}));
}

// --- Requirement 5.3: moving out of bounds is rejected, turn not consumed ---
RV_TEST(move_out_of_bounds_is_rejected_and_position_unchanged) {
    using namespace runevault;
    // Place on the top edge so stepping North leaves the playable region.
    Model model = makeModelAt(Position{0, 5});
    RV_CHECK(model.level()->board().inBounds(Position{-1, 5}) == false);

    RV_CHECK(model.moveArcanist(Direction::No) == false);
    RV_CHECK(model.arcanistPosition() == (Position{0, 5}));
}

// --- Requirement 5.1: all 8 directions step correctly ----------------------
RV_TEST(all_eight_directions_step_one_cell) {
    using namespace runevault;
    const Position origin{2, 5}; // fully surrounded by chamber-1 floor
    const std::array<Direction, 8> dirs = {
        Direction::No, Direction::So, Direction::Ea, Direction::We,
        Direction::Ne, Direction::Nw, Direction::Se, Direction::Sw};

    for (Direction d : dirs) {
        Model model = makeModelAt(origin);
        const Position expected = Geometry::step(origin, d);
        // Every neighbour of (2,5) is interior floor, so each move succeeds.
        RV_CHECK(model.level()->board().at(expected).type == CellType::Floor);
        RV_CHECK(model.moveArcanist(d) == true);
        RV_CHECK(model.arcanistPosition() == expected);
    }
}

// --- Convenience: string-token overload delegates and rejects garbage -------
RV_TEST(string_token_overload_moves_and_rejects_unknown) {
    using namespace runevault;
    Model model = makeModelAt(Position{2, 5});
    RV_CHECK(model.moveArcanist(std::string("ea")) == true);
    RV_CHECK(model.arcanistPosition() == (Position{2, 6}));

    // Unknown token: no move, position unchanged.
    RV_CHECK(model.moveArcanist(std::string("xx")) == false);
    RV_CHECK(model.arcanistPosition() == (Position{2, 6}));
}
