#ifndef RUNEVAULT_GEOMETRY_H
#define RUNEVAULT_GEOMETRY_H

#include <array>
#include <string>

namespace runevault {

enum class Direction {
    No, // "no" — north      (row - 1)
    So, // "so" — south      (row + 1)
    Ea, // "ea" — east       (col + 1)
    We, // "we" — west       (col - 1)
    Ne, // "ne" — north-east (row - 1, col + 1)
    Nw, // "nw" — north-west (row - 1, col - 1)
    Se, // "se" — south-east (row + 1, col + 1)
    Sw  // "sw" — south-west  (row + 1, col - 1)
};

// The number of distinct directions / the size of a 1-block radius.
constexpr int kDirectionCount = 8;

struct Position {
    int row = 0;
    int col = 0;

    bool operator==(const Position &other) const {
        return row == other.row && col == other.col;
    }
    bool operator!=(const Position &other) const { return !(*this == other); }
};

// All geometry lives in a free-function utility namespace: there is no state to
// carry, and centralizing the offsets here guarantees a single source of truth.
namespace Geometry {

// One cell in direction `d` from `p`. Pure offset arithmetic; the result may be
// off-board and it is the caller's responsibility to validate against a Board.
Position step(const Position &p, Direction d);

std::array<Position, 8> neighbours(const Position &p);

bool isAdjacent(const Position &a, const Position &b);

Direction parse(const std::string &token);

} // namespace Geometry

std::string directionName(Direction d);

} // namespace runevault

#endif // RUNEVAULT_GEOMETRY_H
