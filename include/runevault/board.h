#ifndef RUNEVAULT_BOARD_H
#define RUNEVAULT_BOARD_H

#include "runevault/cell.h"
#include "runevault/geometry.h"

#include <vector>

namespace runevault {

constexpr int kPlayableRows = 25;
constexpr int kPlayableCols = 79;

class Board {
public:
    explicit Board(int rows = kPlayableRows, int cols = kPlayableCols);

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    // True iff `p` lies inside the grid (0 <= row < rows, 0 <= col < cols).
    bool inBounds(const Position &p) const;

    const Cell &at(const Position &p) const;

    void setCell(const Position &p, const Cell &cell);
    void setCell(const Position &p, CellType type, int chamberId = kNoChamber);

    // The chamber ids present on the board, sorted ascending. Excludes the
    // `kNoChamber` sentinel (corridors / archways / walls).
    std::vector<int> chamberIds() const;

    std::vector<Position> chamberFloorTiles(int chamberId) const;

    // The in-bounds, walkable cells within the 1-block radius of `p` (delegates
    // to Geometry::neighbours). Walls and off-board neighbours are excluded.
    std::vector<Position> walkableNeighbours(const Position &p) const;

    std::vector<Position> floorNeighboursInChamber(const Position &p,
                                                    int chamberId) const;

private:
    int rows_;
    int cols_;
    std::vector<std::vector<Cell>> grid_; // grid_[row][col]
};

} // namespace runevault

#endif // RUNEVAULT_BOARD_H
