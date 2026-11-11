#include "runevault/level.h"

#include <utility>

namespace runevault {
namespace {

// Carve one chamber: a solid wall perimeter enclosing a 9×4 floor interior
// tagged with `chamberId`. `(topRow, leftCol)` is the top-left wall corner.
void carveChamber(Board &board, int topRow, int leftCol, int chamberId) {
    const int bottomRow = topRow + 5; // 6 rows tall: topRow .. topRow+5
    const int rightCol = leftCol + 10; // 11 cols wide: leftCol .. leftCol+10

    // Top and bottom walls.
    for (int c = leftCol; c <= rightCol; ++c) {
        board.setCell(Position{topRow, c}, CellType::Wall);
        board.setCell(Position{bottomRow, c}, CellType::Wall);
    }
    // Left and right walls.
    for (int r = topRow + 1; r < bottomRow; ++r) {
        board.setCell(Position{r, leftCol}, CellType::Wall);
        board.setCell(Position{r, rightCol}, CellType::Wall);
    }
    // Floor interior (9 cols × 4 rows), tagged with the chamber id.
    for (int r = topRow + 1; r <= topRow + 4; ++r) {
        for (int c = leftCol + 1; c <= leftCol + 9; ++c) {
            board.setCell(Position{r, c}, CellType::Floor, chamberId);
        }
    }
}

// Place an archway `+` on a chamber wall where a corridor connects.
void carveArchway(Board &board, int row, int col) {
    board.setCell(Position{row, col}, CellType::Archway);
}

// Fill a horizontal run of corridor cells (inclusive column range).
void carveCorridorH(Board &board, int row, int colFrom, int colTo) {
    for (int c = colFrom; c <= colTo; ++c) {
        board.setCell(Position{row, c}, CellType::Corridor);
    }
}

// Fill a vertical run of corridor cells (inclusive row range).
void carveCorridorV(Board &board, int col, int rowFrom, int rowTo) {
    for (int r = rowFrom; r <= rowTo; ++r) {
        board.setCell(Position{r, col}, CellType::Corridor);
    }
}

} // namespace

Board makeDefaultBoard() {
    // Start from an all-wall playable region; carve chambers and corridors in.
    Board board(kPlayableRows, kPlayableCols);

    carveChamber(board, /*top*/ 0, /*left*/ 0, /*id*/ 1);
    carveChamber(board, /*top*/ 0, /*left*/ 19, /*id*/ 2);
    carveChamber(board, /*top*/ 8, /*left*/ 0, /*id*/ 3);
    carveChamber(board, /*top*/ 8, /*left*/ 19, /*id*/ 4);
    carveChamber(board, /*top*/ 16, /*left*/ 19, /*id*/ 5);

    carveArchway(board, 2, 10);
    carveCorridorH(board, 2, 11, 18);
    carveArchway(board, 2, 19);

    // 1-3: vertical along col 3. Archway on chamber 1's bottom wall (row 5) and
    // chamber 3's top wall (row 8); corridor spans rows 6..7.
    carveArchway(board, 5, 3);
    carveCorridorV(board, 3, 6, 7);
    carveArchway(board, 8, 3);

    // 2-4: vertical along col 25. Archway on chamber 2's bottom wall (row 5)
    // and chamber 4's top wall (row 8); corridor spans rows 6..7.
    carveArchway(board, 5, 25);
    carveCorridorV(board, 25, 6, 7);
    carveArchway(board, 8, 25);

    // 3-4: horizontal along row 10. Archway on chamber 3's right wall (col 10)
    // and chamber 4's left wall (col 19); corridor spans cols 11..18.
    carveArchway(board, 10, 10);
    carveCorridorH(board, 10, 11, 18);
    carveArchway(board, 10, 19);

    carveArchway(board, 13, 23);
    carveCorridorV(board, 23, 14, 15);
    carveArchway(board, 16, 23);

    return board;
}

Level::Level(Board board) : board_(std::move(board)) {}

void Level::addItem(std::unique_ptr<Item> item) {
    if (item != nullptr) {
        items_.push_back(std::move(item));
    }
}

Item *Level::itemAt(const Position &p) const {
    for (const auto &item : items_) {
        if (item != nullptr && item->position() == p) {
            return item.get();
        }
    }
    return nullptr;
}

std::unique_ptr<Item> Level::removeItemAt(const Position &p) {
    for (auto it = items_.begin(); it != items_.end(); ++it) {
        if (*it != nullptr && (*it)->position() == p) {
            std::unique_ptr<Item> removed = std::move(*it);
            items_.erase(it);
            return removed;
        }
    }
    return nullptr;
}

void Level::addSpectre(std::unique_ptr<Spectre> s) {
    if (s != nullptr) {
        spectres_.push_back(std::move(s));
    }
}

Spectre *Level::spectreAt(const Position &p) const {
    for (const auto &s : spectres_) {
        if (s != nullptr && s->position() == p) {
            return s.get();
        }
    }
    return nullptr;
}

void Level::removeDeadSpectres() {
    for (auto it = spectres_.begin(); it != spectres_.end();) {
        if (*it != nullptr && (*it)->isDead()) {
            it = spectres_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace runevault
