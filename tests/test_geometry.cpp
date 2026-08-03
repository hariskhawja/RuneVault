// test_geometry.cpp
// RuneVault — tests for the 8-direction geometry helpers (task 1.2).
//
// Verifies step() for all 8 directions, neighbours() contents/ordering,
// isAdjacent() boundary cases, and parse() for every valid token plus an
// invalid token. Validates Requirement 5.1.

#include "test_util.h"

#include "runevault/geometry.h"

#include <stdexcept>

// ---- step: all 8 directions -------------------------------------------------

RV_TEST(geometry_step_north_decreases_row) {
    using namespace runevault;
    Position r = Geometry::step(Position{5, 5}, Direction::No);
    RV_CHECK(r == (Position{4, 5}));
}

RV_TEST(geometry_step_south_increases_row) {
    using namespace runevault;
    Position r = Geometry::step(Position{5, 5}, Direction::So);
    RV_CHECK(r == (Position{6, 5}));
}

RV_TEST(geometry_step_east_increases_col) {
    using namespace runevault;
    Position r = Geometry::step(Position{5, 5}, Direction::Ea);
    RV_CHECK(r == (Position{5, 6}));
}

RV_TEST(geometry_step_west_decreases_col) {
    using namespace runevault;
    Position r = Geometry::step(Position{5, 5}, Direction::We);
    RV_CHECK(r == (Position{5, 4}));
}

RV_TEST(geometry_step_northeast) {
    using namespace runevault;
    Position r = Geometry::step(Position{5, 5}, Direction::Ne);
    RV_CHECK(r == (Position{4, 6}));
}

RV_TEST(geometry_step_northwest) {
    using namespace runevault;
    Position r = Geometry::step(Position{5, 5}, Direction::Nw);
    RV_CHECK(r == (Position{4, 4}));
}

RV_TEST(geometry_step_southeast) {
    using namespace runevault;
    Position r = Geometry::step(Position{5, 5}, Direction::Se);
    RV_CHECK(r == (Position{6, 6}));
}

RV_TEST(geometry_step_southwest) {
    using namespace runevault;
    Position r = Geometry::step(Position{5, 5}, Direction::Sw);
    RV_CHECK(r == (Position{6, 4}));
}

// ---- neighbours: contents and ordering -------------------------------------

RV_TEST(geometry_neighbours_are_in_direction_enum_order) {
    using namespace runevault;
    Position p{5, 5};
    auto n = Geometry::neighbours(p);
    RV_CHECK(n[0] == Geometry::step(p, Direction::No));
    RV_CHECK(n[1] == Geometry::step(p, Direction::So));
    RV_CHECK(n[2] == Geometry::step(p, Direction::Ea));
    RV_CHECK(n[3] == Geometry::step(p, Direction::We));
    RV_CHECK(n[4] == Geometry::step(p, Direction::Ne));
    RV_CHECK(n[5] == Geometry::step(p, Direction::Nw));
    RV_CHECK(n[6] == Geometry::step(p, Direction::Se));
    RV_CHECK(n[7] == Geometry::step(p, Direction::Sw));
}

RV_TEST(geometry_neighbours_never_include_centre) {
    using namespace runevault;
    Position p{5, 5};
    auto n = Geometry::neighbours(p);
    for (const auto &cell : n) {
        RV_CHECK(cell != p);
        RV_CHECK(Geometry::isAdjacent(p, cell));
    }
}

// ---- isAdjacent: boundary cases --------------------------------------------

RV_TEST(geometry_is_adjacent_orthogonal_neighbours) {
    using namespace runevault;
    Position p{5, 5};
    RV_CHECK(Geometry::isAdjacent(p, Position{4, 5}));
    RV_CHECK(Geometry::isAdjacent(p, Position{6, 5}));
    RV_CHECK(Geometry::isAdjacent(p, Position{5, 6}));
    RV_CHECK(Geometry::isAdjacent(p, Position{5, 4}));
}

RV_TEST(geometry_is_adjacent_diagonal_neighbours) {
    using namespace runevault;
    Position p{5, 5};
    RV_CHECK(Geometry::isAdjacent(p, Position{4, 6}));
    RV_CHECK(Geometry::isAdjacent(p, Position{4, 4}));
    RV_CHECK(Geometry::isAdjacent(p, Position{6, 6}));
    RV_CHECK(Geometry::isAdjacent(p, Position{6, 4}));
}

RV_TEST(geometry_is_not_adjacent_to_itself) {
    using namespace runevault;
    Position p{5, 5};
    RV_CHECK(!Geometry::isAdjacent(p, p));
}

RV_TEST(geometry_is_not_adjacent_when_two_cells_away) {
    using namespace runevault;
    Position p{5, 5};
    RV_CHECK(!Geometry::isAdjacent(p, Position{5, 7}));
    RV_CHECK(!Geometry::isAdjacent(p, Position{7, 5}));
    RV_CHECK(!Geometry::isAdjacent(p, Position{3, 3}));
    RV_CHECK(!Geometry::isAdjacent(p, Position{7, 7}));
}

RV_TEST(geometry_is_adjacent_is_symmetric) {
    using namespace runevault;
    Position a{2, 2};
    Position b{3, 3};
    RV_CHECK(Geometry::isAdjacent(a, b) == Geometry::isAdjacent(b, a));
}

// ---- parse: all valid tokens plus an invalid one ---------------------------

RV_TEST(geometry_parse_all_valid_tokens) {
    using namespace runevault;
    RV_CHECK(Geometry::parse("no") == Direction::No);
    RV_CHECK(Geometry::parse("so") == Direction::So);
    RV_CHECK(Geometry::parse("ea") == Direction::Ea);
    RV_CHECK(Geometry::parse("we") == Direction::We);
    RV_CHECK(Geometry::parse("ne") == Direction::Ne);
    RV_CHECK(Geometry::parse("nw") == Direction::Nw);
    RV_CHECK(Geometry::parse("se") == Direction::Se);
    RV_CHECK(Geometry::parse("sw") == Direction::Sw);
}

RV_TEST(geometry_parse_round_trips_through_step) {
    using namespace runevault;
    Position p{5, 5};
    RV_CHECK(Geometry::step(p, Geometry::parse("ne")) == (Position{4, 6}));
}

RV_TEST(geometry_parse_invalid_token_throws) {
    using namespace runevault;
    bool threw = false;
    try {
        Geometry::parse("xx");
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    RV_CHECK(threw);
}

RV_TEST(geometry_parse_uppercase_token_throws) {
    using namespace runevault;
    bool threw = false;
    try {
        Geometry::parse("NO");
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    RV_CHECK(threw);
}
