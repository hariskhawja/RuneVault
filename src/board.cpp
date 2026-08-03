// board.cpp
// RuneVault — Board implementation (task 2.2).
//
// See board.h for the component contract. All 8-direction adjacency math is
// delegated to runevault::Geometry (Requirement 5.1) so the offsets are never
// duplicated here; the Board only layers bounds checks and cell-content filters
// on top of the geometry.

#include "runevault/board.h"

#include <set>
#include <stdexcept>

namespace runevault {

Board::Board(int rows, int cols)
    : rows_(rows), cols_(cols),
      grid_(static_cast<std::size_t>(rows > 0 ? rows : 0),
            std::vector<Cell>(static_cast<std::size_t>(cols > 0 ? cols : 0))) {
    // A default-constructed Cell is a Wall in kNoChamber, so the whole board
    // starts as solid wall until task 2.3 populates the layout.
}

bool Board::inBounds(const Position &p) const {
    return p.row >= 0 && p.row < rows_ && p.col >= 0 && p.col < cols_;
}

const Cell &Board::at(const Position &p) const {
    if (!inBounds(p)) {
        throw std::out_of_range("Board::at: position out of bounds");
    }
    return grid_[static_cast<std::size_t>(p.row)][static_cast<std::size_t>(p.col)];
}

void Board::setCell(const Position &p, const Cell &cell) {
    if (!inBounds(p)) {
        throw std::out_of_range("Board::setCell: position out of bounds");
    }
    grid_[static_cast<std::size_t>(p.row)][static_cast<std::size_t>(p.col)] = cell;
}

void Board::setCell(const Position &p, CellType type, int chamberId) {
    setCell(p, Cell{type, chamberId});
}

std::vector<int> Board::chamberIds() const {
    std::set<int> ids; // sorted + unique
    for (const auto &rowCells : grid_) {
        for (const auto &cell : rowCells) {
            if (cell.chamberId != kNoChamber) {
                ids.insert(cell.chamberId);
            }
        }
    }
    return std::vector<int>(ids.begin(), ids.end());
}

std::vector<Position> Board::chamberFloorTiles(int chamberId) const {
    std::vector<Position> tiles;
    if (chamberId == kNoChamber) {
        return tiles; // corridors/archways/walls are never "chamber floor"
    }
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            const Cell &cell = grid_[static_cast<std::size_t>(r)]
                                    [static_cast<std::size_t>(c)];
            if (cell.type == CellType::Floor && cell.chamberId == chamberId) {
                tiles.push_back(Position{r, c});
            }
        }
    }
    return tiles;
}

std::vector<Position> Board::walkableNeighbours(const Position &p) const {
    std::vector<Position> result;
    for (const Position &n : Geometry::neighbours(p)) {
        if (inBounds(n) && at(n).isWalkable()) {
            result.push_back(n);
        }
    }
    return result;
}

std::vector<Position>
Board::floorNeighboursInChamber(const Position &p, int chamberId) const {
    std::vector<Position> result;
    if (chamberId == kNoChamber) {
        return result;
    }
    for (const Position &n : Geometry::neighbours(p)) {
        if (!inBounds(n)) {
            continue;
        }
        const Cell &cell = at(n);
        if (cell.type == CellType::Floor && cell.chamberId == chamberId) {
            result.push_back(n);
        }
    }
    return result;
}

} // namespace runevault
