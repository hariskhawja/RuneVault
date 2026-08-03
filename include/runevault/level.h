// level.h
// RuneVault — the hardcoded default vault layout and the Level container
// (task 2.3).
//
// This component provides two things:
//
//   * `makeDefaultBoard()` — a free factory that returns a `Board` populated
//     tile-for-tile with the built-in default layout used when no layout file
//     is supplied on the command line (design "Default Fixed Layout"). The
//     layout is 5 chambers (each a 9-column × 4-row floor interior, Requirement
//     3.2) joined by 5 corridors whose connectivity matches the spec exactly —
//     1-2, 1-3, 2-4, 3-4, 4-5 (Requirement 3.3) — with an archway `+` on the
//     chamber wall wherever a corridor connects (Requirement 3.5). Every
//     chamber is reachable from every other (Requirement 3.4). The map is drawn
//     at its fixed dimensions without scaling (DD-5 / Requirement 3.7) and fits
//     inside the 79×25 playable region (Requirement 3.8). All five levels share
//     this identical layout (Requirement 3.6), so the same factory is used for
//     each level.
//
//   * `Level` — a plain container that owns a `Board` (by value) plus the
//     level's stairway location and whether the stairway has been revealed
//     (design data model: `Level` "tracks the stairway location and its
//     hidden/revealed state"). Spawning (task 15) and level transitions (task
//     16) build on this scaffolding later; for now `Level` is intentionally a
//     minimal owner of state with simple accessors.

#ifndef RUNEVAULT_LEVEL_H
#define RUNEVAULT_LEVEL_H

#include "runevault/board.h"
#include "runevault/geometry.h"
#include "runevault/item.h"
#include "runevault/spectre.h"

#include <memory>
#include <vector>

namespace runevault {

// The number of chambers in every vault level (Requirement 3.1). Chambers are
// numbered 1..kChamberCount.
constexpr int kChamberCount = 5;

// Build a fresh `Board` populated with the built-in default layout. Every call
// returns an independent board so each of the five levels can own its own copy
// (Requirement 3.6 — the layout is identical on every level).
Board makeDefaultBoard();

// A single vault level: owns its `Board` and tracks the stairway. The stairway
// renders as a floor tile until the Cipher Gem is carried (Requirement 2.6/2.7,
// handled by the View); `stairwayRevealed` records that hidden/revealed state.
class Level {
public:
    // Construct a level around `board` (taken by value and moved in). The
    // stairway defaults to (0,0) and hidden; the Spawner sets the real stairway
    // location later (task 15).
    explicit Level(Board board);

    // Read-only access to the owned board.
    const Board &board() const { return board_; }

    // The stairway's location on the board.
    Position stairway() const { return stairway_; }
    void setStairway(const Position &p) { stairway_ = p; }

    // Whether the stairway has been revealed (Cipher Gem picked up). Starts
    // false and is flipped when the current level's Cipher Gem is carried.
    bool stairwayRevealed() const { return stairwayRevealed_; }
    void setStairwayRevealed(bool revealed) { stairwayRevealed_ = revealed; }

    // --- Items on the level (task 6.3) -----------------------------------
    // The Level owns every placeable non-Spectre object (Rune Fragments today;
    // Scrolls, the Cipher Gem, and Guarded Major Items later). Ownership is via
    // unique_ptr, which makes Level move-only — that is fine because the Model
    // owns the Level through a unique_ptr and only ever moves it.

    // Take ownership of `item`, adding it to the level.
    void addItem(std::unique_ptr<Item> item);

    // Read-only access to all items on the level, in insertion order.
    const std::vector<std::unique_ptr<Item>> &items() const { return items_; }

    // The item currently occupying `p`, or nullptr if the tile holds no item.
    // If multiple items share a tile the first inserted is returned.
    Item *itemAt(const Position &p) const;

    // Detach and return the item occupying `p` (transferring ownership to the
    // caller), removing it from the level. Returns nullptr if no item is there.
    std::unique_ptr<Item> removeItemAt(const Position &p);

    // --- Spectres on the level (task 7.5) --------------------------------
    // The Level owns every Spectre on the floor (Requirement 2.5). Ownership is
    // via unique_ptr, mirroring the item API above; the Model reaches Spectres
    // through level()->spectreAt(...) for combat (Arcanist attack task 7.5,
    // Spectre attack task 7.6) and iterates spectres() for the Spectre action
    // phase (task 10.3). Spawning (task 15) populates these later.

    // Take ownership of `s`, adding it to the level.
    void addSpectre(std::unique_ptr<Spectre> s);

    // Read-only access to all Spectres on the level, in insertion order.
    const std::vector<std::unique_ptr<Spectre>> &spectres() const {
        return spectres_;
    }

    // The Spectre currently occupying `p`, or nullptr if none is there. Returns
    // the first inserted Spectre whose position() == p (dead ones included —
    // dead Spectres are normally cleared via removeDeadSpectres(), so callers
    // that must ignore corpses should check isDead() on the result).
    Spectre *spectreAt(const Position &p) const;

    // Erase every Spectre whose isDead() is true. Handy after combat resolves a
    // kill (used by the drops/removal step, task 7.8, and the Spectre action
    // phase, task 10.x).
    void removeDeadSpectres();

private:
    Board board_;
    Position stairway_{0, 0};
    bool stairwayRevealed_ = false;
    std::vector<std::unique_ptr<Item>> items_;
    std::vector<std::unique_ptr<Spectre>> spectres_;
};

} // namespace runevault

#endif // RUNEVAULT_LEVEL_H
