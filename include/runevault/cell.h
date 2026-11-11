#ifndef RUNEVAULT_CELL_H
#define RUNEVAULT_CELL_H

namespace runevault {

// The five kinds of tile that make up a vault level (design geometry table).
enum class CellType {
    Wall,     // vault wall — `|` / `-`; blocks movement; ' ' also considered as wall
    Archway,  // `+` — corridor/chamber junction; walkable, transit only
    Corridor, // `#` — connects chambers; walkable, transit only
    Floor,    // `.` — the only cell that hosts actors/items
    Stairway  // descent tile; walkable, special (never hosts actors/items)
};

constexpr int kNoChamber = -1;

// True iff an actor may occupy or transit through a tile of this type. Every
// cell type is walkable except walls.
inline bool isWalkable(CellType type) {
    return type != CellType::Wall;
}

inline bool canHost(CellType type) {
    return type == CellType::Floor;
}

struct Cell {
    CellType type = CellType::Wall;
    int chamberId = kNoChamber;

    // Convenience forwarders so callers can ask a cell directly.
    bool isWalkable() const { return runevault::isWalkable(type); }
    bool canHost() const { return runevault::canHost(type); }

    bool operator==(const Cell &other) const {
        return type == other.type && chamberId == other.chamberId;
    }
    bool operator!=(const Cell &other) const { return !(*this == other); }
};

} // namespace runevault

#endif // RUNEVAULT_CELL_H
