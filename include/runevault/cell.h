// cell.h
// RuneVault — board cell type and geometry semantics (task 2.1).
//
// A `Cell` is a single tile of the playable region. Every cell carries its
// `CellType` (which fixes its render glyph and its walkability/host semantics)
// and its **chamber membership** — the id of the chamber it belongs to, or
// `kNoChamber` for corridors / archways / off-chamber tiles.
//
// The geometry table (design "Board & Level Model / Geometry", Requirement 2.4
// for glyphs and Requirement 3.5 for archway junctions) is the single source of
// truth for two orthogonal questions:
//
//   * isWalkable(type) — may an actor step onto / transit through the tile?
//     Walls block movement; every other cell type is walkable.
//   * canHost(type)    — may a Spectre or item be *placed* on the tile?
//     Only floor tiles host actors/items. Archways, corridors and the stairway
//     are transit-only, and walls host nothing.
//
// | Cell type | Glyph            | Walkable | Can host actors/items |
// |-----------|------------------|----------|-----------------------|
// | Wall      | `|` `-`          | no       | no                    |
// | Archway   | `+`              | yes      | no (transit only)     |
// | Corridor  | `#`              | yes      | no (transit only)     |
// | Floor     | `.`              | yes      | yes                   |
// | Stairway  | `\` (hidden `.`) | yes      | no (special)          |
//
// Rendering (glyph selection, hidden-stairway handling per DD-7) is the View's
// job (task 3.1); this component deliberately stays limited to type + chamber +
// walkable/host semantics.

#ifndef RUNEVAULT_CELL_H
#define RUNEVAULT_CELL_H

namespace runevault {

// The five kinds of tile that make up a vault level (design geometry table).
enum class CellType {
    Wall,     // vault wall — `|` / `-`; blocks movement
    Archway,  // `+` — corridor/chamber junction; walkable, transit only
    Corridor, // `#` — connects chambers; walkable, transit only
    Floor,    // `.` — the only cell that hosts actors/items
    Stairway  // descent tile; walkable, special (never hosts actors/items)
};

// Sentinel chamber id for cells that belong to no chamber (corridors and
// archways). Chambers themselves are numbered 1..5 (Requirement 3.1).
constexpr int kNoChamber = -1;

// True iff an actor may occupy or transit through a tile of this type. Every
// cell type is walkable except walls.
inline bool isWalkable(CellType type) {
    return type != CellType::Wall;
}

// True iff a Spectre or item may be *placed* on a tile of this type. Only floor
// tiles host actors/items; archways, corridors, the stairway and walls do not
// (design: "Spectres and items may spawn only on floor tiles").
inline bool canHost(CellType type) {
    return type == CellType::Floor;
}

// A single board tile: its type plus the chamber it belongs to (`kNoChamber`
// for corridor / archway / none). Cells are plain values; bounds and layout
// belong to the Board component (later tasks).
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
