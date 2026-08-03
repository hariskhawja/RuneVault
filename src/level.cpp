// level.cpp
// RuneVault — default fixed layout builder and Level container (task 2.3).
//
// See level.h for the component contract. `makeDefaultBoard` transcribes the
// design's "Default Fixed Layout" tile-for-tile at fixed dimensions (DD-5); the
// concrete coordinates chosen here place the five chambers on a 2×2 grid with a
// fifth chamber below the lower-right one, exactly reproducing the spec
// connectivity 1-2, 1-3, 2-4, 3-4, 4-5.
//
// Chamber geometry (each chamber is an 11-col × 6-row box: a 1-cell wall border
// around a 9-col × 4-row floor interior, Requirement 3.2). `(topRow, leftCol)`
// is the top-left wall corner of the box; the interior spans rows
// topRow+1..topRow+4 and cols leftCol+1..leftCol+9.
//
//   Chamber 1 : (row  0, col  0)      Chamber 2 : (row  0, col 19)
//   Chamber 3 : (row  8, col  0)      Chamber 4 : (row  8, col 19)
//                                     Chamber 5 : (row 16, col 19)
//
// Corridors (each meets a chamber at an archway `+`, Requirement 3.5):
//   1-2  horizontal, row 2,  cols 11..18   (chamber 1 right wall  -> chamber 2 left wall)
//   1-3  vertical,   col 3,  rows  6..7    (chamber 1 bottom wall -> chamber 3 top wall)
//   2-4  vertical,   col 25, rows  6..7    (chamber 2 bottom wall -> chamber 4 top wall)
//   3-4  horizontal, row 10, cols 11..18   (chamber 3 right wall  -> chamber 4 left wall)
//   4-5  vertical,   col 23, rows 14..15   (chamber 4 bottom wall -> chamber 5 top wall)
//
// The whole map occupies rows 0..21 and cols 0..29, comfortably inside the
// 79×25 playable region (Requirement 3.8).

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

    // --- Chambers (Requirement 3.1 / 3.2) --------------------------------
    carveChamber(board, /*top*/ 0, /*left*/ 0, /*id*/ 1);
    carveChamber(board, /*top*/ 0, /*left*/ 19, /*id*/ 2);
    carveChamber(board, /*top*/ 8, /*left*/ 0, /*id*/ 3);
    carveChamber(board, /*top*/ 8, /*left*/ 19, /*id*/ 4);
    carveChamber(board, /*top*/ 16, /*left*/ 19, /*id*/ 5);

    // --- Corridors + archways (Requirement 3.3 / 3.5) --------------------
    // 1-2: horizontal along row 2. Archway on chamber 1's right wall (col 10)
    // and chamber 2's left wall (col 19); corridor spans cols 11..18.
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

    // 4-5: vertical along col 23. Archway on chamber 4's bottom wall (row 13)
    // and chamber 5's top wall (row 16); corridor spans rows 14..15. (Piazza
    // Q6: chambers 4 and 5 are connected.)
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

// --- Spectres (task 7.5) -------------------------------------------------

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
