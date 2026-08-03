// view.h
// RuneVault — the board frame renderer (task 3.1).
//
// The `View` is a PURE renderer (design MVC / DD-5): it reads an immutable
// snapshot of the model and produces the 79-column × 30-row terminal frame as a
// string. It NEVER mutates game state. The top 25 rows are the playable region
// drawn tile-for-tile from the `Board` (Requirement 2.3 — the entire board is
// redrawn on every command); the bottom 5 rows are the status area, rendered
// blank by THIS task and populated by task 3.2.
//
// Symbol mapping is the authoritative DD-7 map (Requirement 2.4/2.5): every
// glyph constant lives centrally in `runevault::glyphs` so later components
// (Spectres, Items, the Spawner) reuse them rather than hardcoding characters.
//
//   @  Arcanist            | / -  vault walls        +  archway
//   #  corridor            .      floor tile         \  stairway (revealed)
//   ?  Arcane Scroll       *      Rune Fragment      C  Cipher Gem
//   $  Shard Coffer        G      Aegis Cloak
//   W B R S L A X          Spectres (Wraith, Banshee, Revenant, Shade, Lich,
//                          Vault Anchor, Specter Lord)
//
// Stairway rendering (Requirements 2.6 / 2.7): the stairway tile renders as a
// floor `.` while `Level::stairwayRevealed()` is false, and as `\` once the
// Cipher Gem has been picked up (revealed == true).
//
// Actors and items do not have model classes yet (Arcanist is task 5.1,
// Spectres task 5.3, Items task 5.2, spawning task 15). To stay usable now and
// decoupled from that future state, the renderer accepts a lightweight
// `RenderOverlay`: a list of (Position, glyph) pairs the caller supplies for
// everything that sits ON TOP of the board tiles. Later tasks (5.x / 15 / 17)
// will populate the overlay from real Arcanist / Spectre / Item objects; the
// View itself needs no changes when they do.
//
// ANSI colour (Requirement 2.9) is OPTIONAL and OFF by default so tests can
// assert plain glyphs. When enabled, the Arcanist, revealed stairway, Spectres,
// Rune Fragments, and Scrolls are wrapped in colour codes.

#ifndef RUNEVAULT_VIEW_H
#define RUNEVAULT_VIEW_H

#include "runevault/board.h"
#include "runevault/geometry.h"

#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace runevault {

// `Level` is only referenced by reference in this header's declarations, so a
// forward declaration is sufficient here. Including level.h would introduce a
// cycle (level.h -> item.h -> view.h); view.cpp includes level.h for the
// definition it needs at render time.
class Level;

// The full terminal frame is 79 columns × 30 rows (Requirement 2.1). The bottom
// `kStatusRows` rows are the status area (Requirement 2.2); the top
// `kPlayableRows` (25) rows are the board.
constexpr int kFrameRows = 30;
constexpr int kFrameCols = kPlayableCols; // 79
constexpr int kStatusRows = kFrameRows - kPlayableRows; // 5

// DD-7 final symbol map (Requirement 2.4 / 2.5). Kept in one place so no later
// component hardcodes a raw character.
namespace glyphs {
constexpr char kArcanist = '@';
constexpr char kWallVertical = '|';
constexpr char kWallHorizontal = '-';
constexpr char kArchway = '+';
constexpr char kCorridor = '#';
constexpr char kFloor = '.';
constexpr char kStairwayHidden = '.';  // renders as floor until revealed (2.6)
constexpr char kStairwayRevealed = '\\'; // renders as `\` once carried (2.7)
constexpr char kScroll = '?';           // ALL scroll types, always (DD-1/DD-3)
constexpr char kRuneFragment = '*';
constexpr char kCipherGem = 'C';
constexpr char kShardCoffer = '$';
constexpr char kAegisCloak = 'G';
constexpr char kWraith = 'W';
constexpr char kBanshee = 'B';
constexpr char kRevenant = 'R';
constexpr char kShade = 'S';
constexpr char kLich = 'L';
constexpr char kVaultAnchor = 'A';
constexpr char kSpecterLord = 'X';
constexpr char kBlank = ' '; // background (structure-free) space
} // namespace glyphs

// A small, decoupled snapshot of the values the status area (Requirement 2.8)
// must display. The real Model/Arcanist aren't built yet (Arcanist is task 5.1),
// so the caller supplies these values directly; later tasks (17.x) will populate
// this struct from the live Model/Arcanist. Keeping it here means the View has
// no dependency on those future classes.
//
// The status area is the bottom `kStatusRows` (5) rows of the 79×30 frame
// (Requirement 2.2). renderFrame lays the fields across those rows: class and
// combat stats, the total Rune Fragments, and any identified-scroll hints /
// messages (Requirement 2.8). Long messages are truncated to fit 79 columns.
struct StatusInfo {
    std::string className;              // e.g. "Sage" (Requirement 2.8)
    int fp = 0;                         // current Focus Points
    int maxFp = 0;                      // class maximum FP (shown as fp/maxFp)
    int pwr = 0;                        // effective Power
    int wr = 0;                         // effective Ward Rating
    int runeFragmentTotal = 0;          // total Rune Fragments collected
    std::vector<std::string> messages;  // scroll hints / messages (Req 2.8)
};

// Everything drawn ON TOP of the board tiles (Arcanist, Spectres, items). The
// caller supplies glyphs positioned in board coordinates; if two entries target
// the same cell, the LATER entry wins (so an actor added after an item is drawn
// over it). Positions outside the board are ignored. Later tasks build this
// from real actor/item objects.
struct RenderOverlay {
    std::vector<std::pair<Position, char>> glyphs;

    // Convenience: append an overlay glyph at `p`.
    void add(const Position &p, char glyph) { glyphs.emplace_back(p, glyph); }
};

// Renders the 79×30 frame from a read-only model snapshot. Stateless apart from
// the ANSI-colour flag chosen at construction.
class View {
public:
    // `ansiColour` defaults OFF so tests observe plain glyphs (Requirement 2.9
    // makes colour optional).
    explicit View(bool ansiColour = false);

    bool ansiColourEnabled() const { return ansiColour_; }
    void setAnsiColour(bool enabled) { ansiColour_ = enabled; }

    // Build the full frame string with the given overlay. Rows are separated by
    // '\n'; with colour OFF every row is exactly `kFrameCols` characters wide.
    std::string renderFrame(const Level &level,
                            const RenderOverlay &overlay) const;

    // Full frame with the status area (Requirement 2.2/2.8) populated from the
    // supplied `status`. The top 25 board rows are rendered exactly as the
    // overlay-only overload; the bottom 5 rows show the class, FP, Pwr, WR, RF
    // total, and messages. With colour OFF every row is exactly `kFrameCols`
    // wide (status lines are padded/truncated to 79 columns).
    std::string renderFrame(const Level &level, const RenderOverlay &overlay,
                            const StatusInfo &status) const;

    // Convenience overload with no overlay (bare board + stairway).
    std::string renderFrame(const Level &level) const;

    // Write the frame directly to an output stream.
    void render(std::ostream &os, const Level &level,
                const RenderOverlay &overlay) const;

private:
    // Glyph for the board tile at `p` (walls resolve orientation via adjacency).
    char cellGlyph(const Board &board, const Position &p) const;

    // Resolve a Wall cell to `-` (horizontal cap), `|` (vertical side / corner),
    // or a blank background space when the wall borders no chamber floor.
    char wallGlyph(const Board &board, const Position &p) const;

    // Wrap a glyph in an ANSI colour code when colour is enabled; otherwise
    // returns the single character unchanged.
    std::string decorate(char glyph) const;

    // Build the `kStatusRows` status lines (each exactly `kFrameCols` wide) from
    // `status`. Appended verbatim below the board rows in renderFrame.
    std::vector<std::string> buildStatusRows(const StatusInfo &status) const;

    bool ansiColour_;
};

} // namespace runevault

#endif // RUNEVAULT_VIEW_H
