#ifndef RUNEVAULT_LAYOUT_LOADER_H
#define RUNEVAULT_LAYOUT_LOADER_H

#include "runevault/board.h"
#include "runevault/geometry.h"
#include "runevault/level.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace runevault {

constexpr int kLayoutLevelCount = 5;

struct ItemPlacement {
    Position position; // the hosting floor tile
    char digit = '0';
};

struct LoadedLayout {
    bool ok = false;
    std::string error;

    // Per-level parsed cell grids (glyph → CellType, chamber ids flood-filled).
    std::array<Board, kLayoutLevelCount> boards;

    std::array<std::vector<ItemPlacement>, kLayoutLevelCount> items;
};

// Parses layout files and builds playable Levels from them. Stateless — all
// members are static; the parsed data lives in the returned `LoadedLayout`.
class LayoutLoader {
public:
    static LoadedLayout loadFromFile(const std::string &path);

    // Parse one map, reused across five levels. Digits place items on floor tiles.
    static LoadedLayout loadFromString(const std::string &contents);

    static std::unique_ptr<Level> buildLevel(const LoadedLayout &loaded,
                                             int levelIndex);

    static std::unique_ptr<Item> makeItemForDigit(char digit);
};

} // namespace runevault

#endif // RUNEVAULT_LAYOUT_LOADER_H
