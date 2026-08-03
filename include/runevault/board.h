// board.h
// RuneVault — the playable tile grid with chamber membership (task 2.2).
//
// A `Board` owns the 79-column × 25-row playable region (design "Board & Level
// Model / Geometry" and DD-5: the bottom 5 of the 30 total board rows are the
// status area rendered by the View and are NOT part of the Board). It stores a
// dense grid of `Cell` (from cell.h) — each carrying a `CellType` and its
// **chamber membership** — and exposes the queries later components rely on:
//
//   * bounds / cell access          — inBounds / at
//   * layout population              — setCell (task 2.3 fills the default map)
//   * chamber enumeration           — chamberIds / chamberFloorTiles
//                                      (used for spawning and Spectre
//                                       containment, Requirements 3.1/3.2)
//   * adjacency / geometry queries   — walkableNeighbours /
//                                      floorNeighboursInChamber, which delegate
//                                      ALL 8-direction math to runevault::
//                                      Geometry (Requirement 5.1) so the offsets
//                                      live in exactly one place.
//
// The Board is deliberately **layout-agnostic**: a freshly constructed board is
// all `Wall`; the hardcoded 5-chamber default layout is populated by task 2.3
// via `setCell`. Coordinates are `(row, col)` with the origin at the top-left
// of the playable region (see geometry.h).

#ifndef RUNEVAULT_BOARD_H
#define RUNEVAULT_BOARD_H

#include "runevault/cell.h"
#include "runevault/geometry.h"

#include <vector>

namespace runevault {

// Playable-region dimensions (design "Geometry" / DD-5). The full terminal
// frame is 79×30; the bottom 5 rows are the status area and never belong to the
// Board, leaving a 79-column × 25-row playable grid.
constexpr int kPlayableRows = 25;
constexpr int kPlayableCols = 79;

// The tile grid for a single vault level. Owns a rows×cols matrix of `Cell`s
// and answers bounds / chamber / adjacency queries. All adjacency arithmetic is
// delegated to `Geometry`; the Board only adds bounds and cell-content filters.
class Board {
public:
    // Construct an all-`Wall` board of the given dimensions (defaults to the
    // playable region). Task 2.3 populates the real layout through `setCell`.
    explicit Board(int rows = kPlayableRows, int cols = kPlayableCols);

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    // True iff `p` lies inside the grid (0 <= row < rows, 0 <= col < cols).
    bool inBounds(const Position &p) const;

    // Read-only access to the cell at `p`. Throws std::out_of_range if `p` is
    // outside the grid; callers guard with `inBounds` when the position may be
    // off-board (e.g. after a `Geometry::step`).
    const Cell &at(const Position &p) const;

    // Layout population (task 2.3). Both overloads throw std::out_of_range for
    // an off-board position.
    void setCell(const Position &p, const Cell &cell);
    void setCell(const Position &p, CellType type, int chamberId = kNoChamber);

    // The chamber ids present on the board, sorted ascending. Excludes the
    // `kNoChamber` sentinel (corridors / archways / walls).
    std::vector<int> chamberIds() const;

    // Every FLOOR tile whose chamber membership equals `chamberId`, in
    // row-major order. Used for spawning (Requirement 3.2) and to constrain
    // Spectre movement to its spawn chamber (Requirement 7.2).
    std::vector<Position> chamberFloorTiles(int chamberId) const;

    // The in-bounds, walkable cells within the 1-block radius of `p` (delegates
    // to Geometry::neighbours). Walls and off-board neighbours are excluded.
    std::vector<Position> walkableNeighbours(const Position &p) const;

    // The in-bounds FLOOR neighbours of `p` that belong to `chamberId`. This is
    // the movement-candidate set for a Spectre confined to its spawn chamber
    // (Requirement 7.2 / 7.3): same-chamber floor tiles within the 1-block
    // radius. Occupancy is layered on by the movement component, not here.
    std::vector<Position> floorNeighboursInChamber(const Position &p,
                                                    int chamberId) const;

private:
    int rows_;
    int cols_;
    std::vector<std::vector<Cell>> grid_; // grid_[row][col]
};

} // namespace runevault

#endif // RUNEVAULT_BOARD_H
