// layout_loader.h
// RuneVault — the optional level-layout file loader (task 18.2, Requirement 12).
//
// When a level-layout file is supplied on the command line (task 18.1 parses
// the path into CliOptions::layoutFile), THIS module turns the file's text into
// a usable, engine-ready representation and — crucially — reports a missing or
// malformed file at startup (Requirement 12.4). It provides two entry points:
//
//   * `LayoutLoader::loadFromFile(path)`   — opens the file, slurps it, and
//     delegates to `loadFromString`. A file that cannot be opened yields
//     `ok == false` with a descriptive `error` (Requirement 12.4).
//   * `LayoutLoader::loadFromString(text)` — the pure parser (no I/O), so tests
//     feed it in-memory strings without touching the filesystem.
//
// and a builder that turns the parsed data into a playable `Level`:
//
//   * `LayoutLoader::buildLevel(loaded, index)` — copies level `index`'s parsed
//     `Board` into a fresh `Level` and drops the digit-derived items onto their
//     floor tiles (Requirement 12.2). Returns `nullptr` for an out-of-range
//     index or a `LoadedLayout` that failed to parse.
//
// ── File format (DOCUMENTED conventions) ─────────────────────────────────────
// Per Piazza Q15 Follow-up-2, a layout file holds ONE 79×25 map grid, EXCLUDING
// the 5 status rows (Requirement 12.1 — status rows are the View's job and never
// appear in the file). That single grid is APPLIED TO ALL FIVE LEVELS, since
// every level shares the same fixed chamber configuration; the file is NOT five
// separate grids. `loadFromString` parses the one grid and replicates its board
// + item placements across all five levels, so `boards[i]`/`items[i]` are
// identical for every i. (Q15 Follow-up-3: this makes scroll/fragment placement
// identical across all five levels, while the Arcanist, stairway, and Spectres
// stay randomly generated per level — they have no file encoding.)
//
// Leading/trailing blank (whitespace-only) lines are ignored as padding; the
// remaining lines form the single grid.
//
// Glyph → CellType mapping (matches the default-layout figure / cell.h):
//   '|' , '-'  -> Wall        (chamber edges)
//   '+'        -> Archway     (corridor/chamber junction)
//   '#'        -> Corridor
//   '.'        -> Floor
//   ' ' (space)-> void        (outside any level; modelled as Wall, non-walkable)
//   '0'..'9'   -> Floor tile HOSTING the item that digit denotes (Req 12.2)
//
// Digit → item mapping (Requirement 12.2), applied explicitly per digit:
//   0 = Attune Focus scroll   5 = Erode Ward scroll
//   1 = Surge Power scroll     6 = common shard   (FragmentGrade::CommonShard)
//   2 = Fortify Ward scroll    7 = resonant shard (FragmentGrade::ResonantShard)
//   3 = Drain Focus scroll     8 = Lich cache     (FragmentGrade::LichCache, value 4)
//   4 = Sap Power scroll       9 = Shard Coffer   (ShardCoffer guarded item)
//
// Chamber membership for a file layout is DERIVED by flood-filling contiguous
// Floor regions (4-connected) and numbering them 1..N in row-major discovery
// order — a documented simplification (the figure's chamber-id digits are not
// present in a raw map). Corridors, archways, walls and void stay `kNoChamber`.
//
// ── Malformed detection (Requirement 12.4) ───────────────────────────────────
// `loadFromString` returns `ok == false` with a descriptive `error` when:
//   * the file/string is empty (no map grid),
//   * the grid has more than kPlayableRows (25) rows,
//   * a row is wider than kPlayableCols (79) columns,
//   * an unrecognized glyph is encountered (strict: anything not listed above).
// `loadFromFile` additionally reports a file that cannot be opened.
//
// ── SCOPE (task 18.2) ────────────────────────────────────────────────────────
// This task delivers PARSING + error reporting + per-level Level construction
// from the map and item digits. Wiring the loaded layout into the Spawner for a
// full file-driven run, and placing the Arcanist / stairway / guarding Vault
// Anchors for a file layout, are integration details finalized in task 19.1;
// main() here reports a bad/missing file at startup (the testable 12.4
// behaviour) and otherwise leaves the random-generation path unchanged.

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

// The number of levels a layout file describes (Requirement 12.1: all five).
constexpr int kLayoutLevelCount = 5;

// A single digit-derived item placement: the floor tile it sits on plus the
// digit '0'..'9' that identifies which item to build (Requirement 12.2).
struct ItemPlacement {
    Position position; // the hosting floor tile
    char digit = '0';  // '0'..'9' per the Requirement 12.2 table
};

// The result of parsing a layout file (or string). On success `ok == true` and
// `boards`/`items` hold the five parsed levels; on failure `ok == false` and
// `error` carries a human-readable reason (printed at startup, Requirement
// 12.4).
struct LoadedLayout {
    bool ok = false;
    std::string error; // populated when !ok (Requirement 12.4)

    // Per-level parsed cell grids (glyph → CellType, chamber ids flood-filled).
    std::array<Board, kLayoutLevelCount> boards;

    // Per-level digit item placements (Requirement 12.2), in row-major order.
    std::array<std::vector<ItemPlacement>, kLayoutLevelCount> items;
};

// Parses layout files and builds playable Levels from them. Stateless — all
// members are static; the parsed data lives in the returned `LoadedLayout`.
class LayoutLoader {
public:
    // Read `path` and parse it. Returns ok=false with a descriptive error if
    // the file cannot be opened (Requirement 12.4) or the contents are
    // malformed. Reads the whole file into a string then defers to
    // loadFromString so the two paths share one parser.
    static LoadedLayout loadFromFile(const std::string &path);

    // Pure parser (no I/O) — parses `contents` per the documented format. See
    // the header notes for the glyph/digit conventions and malformed rules.
    static LoadedLayout loadFromString(const std::string &contents);

    // Build a playable Level for level `levelIndex` (0..4) from a successfully
    // parsed `loaded`: copies the parsed Board and drops each digit's item onto
    // its floor tile (Requirement 12.2). Returns nullptr when `loaded` failed to
    // parse or `levelIndex` is out of range.
    static std::unique_ptr<Level> buildLevel(const LoadedLayout &loaded,
                                             int levelIndex);

    // Build the Item denoted by a layout digit '0'..'9' (Requirement 12.2).
    // Returns nullptr for any non-digit char (callers only pass validated
    // digits). Exposed for focused digit-mapping tests.
    static std::unique_ptr<Item> makeItemForDigit(char digit);
};

} // namespace runevault

#endif // RUNEVAULT_LAYOUT_LOADER_H
