// geometry.cpp
// RuneVault — implementation of the 8-direction geometry helpers (task 1.2).
//
// See include/runevault/geometry.h for the contract. All adjacency math lives
// here so the spec's "1-block radius = 8 adjacent cells" rule (Requirement 5.1)
// is defined in exactly one place.

#include "runevault/geometry.h"

#include <cstdlib>
#include <stdexcept>

namespace runevault {
namespace Geometry {

Position step(const Position &p, Direction d) {
    switch (d) {
    case Direction::No:
        return Position{p.row - 1, p.col};
    case Direction::So:
        return Position{p.row + 1, p.col};
    case Direction::Ea:
        return Position{p.row, p.col + 1};
    case Direction::We:
        return Position{p.row, p.col - 1};
    case Direction::Ne:
        return Position{p.row - 1, p.col + 1};
    case Direction::Nw:
        return Position{p.row - 1, p.col - 1};
    case Direction::Se:
        return Position{p.row + 1, p.col + 1};
    case Direction::Sw:
        return Position{p.row + 1, p.col - 1};
    }
    // Unreachable for a valid Direction; keeps the compiler happy about the
    // non-void return path.
    return p;
}

std::array<Position, 8> neighbours(const Position &p) {
    return {{
        step(p, Direction::No),
        step(p, Direction::So),
        step(p, Direction::Ea),
        step(p, Direction::We),
        step(p, Direction::Ne),
        step(p, Direction::Nw),
        step(p, Direction::Se),
        step(p, Direction::Sw),
    }};
}

bool isAdjacent(const Position &a, const Position &b) {
    const int dr = std::abs(a.row - b.row);
    const int dc = std::abs(a.col - b.col);
    // Chebyshev distance exactly 1: within the 1-block radius but not the same
    // cell.
    return dr <= 1 && dc <= 1 && (dr != 0 || dc != 0);
}

Direction parse(const std::string &token) {
    if (token == "no")
        return Direction::No;
    if (token == "so")
        return Direction::So;
    if (token == "ea")
        return Direction::Ea;
    if (token == "we")
        return Direction::We;
    if (token == "ne")
        return Direction::Ne;
    if (token == "nw")
        return Direction::Nw;
    if (token == "se")
        return Direction::Se;
    if (token == "sw")
        return Direction::Sw;
    throw std::invalid_argument("invalid direction token: " + token);
}

} // namespace Geometry

std::string directionName(Direction d) {
    switch (d) {
    case Direction::No:
        return "north";
    case Direction::So:
        return "south";
    case Direction::Ea:
        return "east";
    case Direction::We:
        return "west";
    case Direction::Ne:
        return "north-east";
    case Direction::Nw:
        return "north-west";
    case Direction::Se:
        return "south-east";
    case Direction::Sw:
        return "south-west";
    }
    // Unreachable for a valid Direction; keeps the compiler happy.
    return "unknown";
}

} // namespace runevault
