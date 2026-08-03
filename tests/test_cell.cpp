// test_cell.cpp
// RuneVault — Cell / CellType semantics (task 2.1).
//
// Covers walkability and host-ability for every CellType (design geometry
// table, Requirements 2.4 and 3.5) plus chamber-membership storage.

#include "test_util.h"

#include "runevault/cell.h"

RV_TEST(cell_walls_are_not_walkable) {
    using namespace runevault;
    RV_CHECK(isWalkable(CellType::Wall) == false);
}

RV_TEST(cell_non_wall_types_are_walkable) {
    using namespace runevault;
    RV_CHECK(isWalkable(CellType::Archway) == true);
    RV_CHECK(isWalkable(CellType::Corridor) == true);
    RV_CHECK(isWalkable(CellType::Floor) == true);
    RV_CHECK(isWalkable(CellType::Stairway) == true);
}

RV_TEST(cell_only_floor_can_host) {
    using namespace runevault;
    RV_CHECK(canHost(CellType::Floor) == true);
    RV_CHECK(canHost(CellType::Wall) == false);
    RV_CHECK(canHost(CellType::Archway) == false);
    RV_CHECK(canHost(CellType::Corridor) == false);
    RV_CHECK(canHost(CellType::Stairway) == false);
}

RV_TEST(cell_transit_types_are_walkable_but_cannot_host) {
    using namespace runevault;
    // Archways, corridors and the stairway are "transit only": walkable yet
    // never host actors/items.
    RV_CHECK(isWalkable(CellType::Archway) && !canHost(CellType::Archway));
    RV_CHECK(isWalkable(CellType::Corridor) && !canHost(CellType::Corridor));
    RV_CHECK(isWalkable(CellType::Stairway) && !canHost(CellType::Stairway));
}

RV_TEST(cell_member_queries_forward_to_type) {
    using namespace runevault;
    Cell floor{CellType::Floor, 3};
    RV_CHECK(floor.isWalkable() == true);
    RV_CHECK(floor.canHost() == true);

    Cell wall{CellType::Wall, kNoChamber};
    RV_CHECK(wall.isWalkable() == false);
    RV_CHECK(wall.canHost() == false);
}

RV_TEST(cell_defaults_to_wall_with_no_chamber) {
    using namespace runevault;
    Cell c;
    RV_CHECK(c.type == CellType::Wall);
    RV_CHECK(c.chamberId == kNoChamber);
}

RV_TEST(cell_stores_chamber_membership) {
    using namespace runevault;
    Cell inChamber{CellType::Floor, 5};
    RV_CHECK(inChamber.chamberId == 5);

    // Corridors / archways belong to no chamber.
    Cell corridor{CellType::Corridor, kNoChamber};
    RV_CHECK(corridor.chamberId == kNoChamber);
    RV_CHECK(kNoChamber == -1);
}

RV_TEST(cell_equality_considers_type_and_chamber) {
    using namespace runevault;
    RV_CHECK((Cell{CellType::Floor, 2} == Cell{CellType::Floor, 2}));
    RV_CHECK((Cell{CellType::Floor, 2} != Cell{CellType::Floor, 3}));
    RV_CHECK((Cell{CellType::Floor, 2} != Cell{CellType::Corridor, 2}));
}
