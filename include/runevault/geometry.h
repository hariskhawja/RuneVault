// geometry.h
// RuneVault — 8-direction geometry helpers (task 1.2).
//
// A single `Direction` enum, a `Position` value type, and a `Geometry` utility
// centralize ALL adjacency math so the spec's "1-block radius = 8 adjacent
// cells" rule (Requirement 5.1, design "8-direction geometry helpers") is
// defined in exactly one place. Every later component (movement, combat range,
// scroll targeting, Spectre movement/hostility, guarded placement) routes its
// geometry questions through these helpers rather than duplicating offsets.
//
// Coordinate system (design "Board & Level Model / Geometry"): coordinates are
// (row, col) with the origin at the top-left of the playable region. Moving
// North decreases row; moving South increases row; moving East increases col;
// moving West decreases col. Diagonals combine the two axes.

#ifndef RUNEVAULT_GEOMETRY_H
#define RUNEVAULT_GEOMETRY_H

#include <array>
#include <string>

namespace runevault {

// The 8 compass directions. The command tokens accepted by `Geometry::parse`
// are the two-letter lowercase forms shown in the comments (Requirement 5.1).
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

// A board coordinate. `row` grows downward, `col` grows rightward. Positions
// are plain values; bounds checking against a concrete board belongs to the
// Board component (task 2.x), not here.
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

// The 8 cells forming the 1-block radius around `p`, in `Direction` enum order
// (No, So, Ea, We, Ne, Nw, Se, Sw). The centre cell `p` itself is never
// included.
std::array<Position, 8> neighbours(const Position &p);

// True iff `b` lies within the 1-block radius of `a` — i.e. they are one of the
// 8 adjacent cells (Chebyshev distance exactly 1). A position is NOT adjacent
// to itself, so isAdjacent(a, a) is false.
bool isAdjacent(const Position &a, const Position &b);

// Map a lowercase command token ("no", "so", "ea", "we", "ne", "nw", "se",
// "sw") to its `Direction`. Throws std::invalid_argument for any other token so
// callers (the command interpreter) can treat it as an invalid command.
Direction parse(const std::string &token);

} // namespace Geometry

// Readable, human-facing name for a direction ("north", "south", "east",
// "west", "north-east", "north-west", "south-east", "south-west"). Pure and
// standalone — used by the notification log to phrase movement messages
// naturally.
std::string directionName(Direction d);

} // namespace runevault

#endif // RUNEVAULT_GEOMETRY_H
