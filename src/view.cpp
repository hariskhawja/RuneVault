// view.cpp
// RuneVault — board frame renderer implementation (task 3.1).
//
// See view.h for the component contract. The renderer walks the board in
// row-major order, resolving each cell to a DD-7 glyph, then overlays the
// caller-supplied actor/item glyphs and the stairway glyph. The bottom 5 status
// rows are emitted blank (task 3.2 fills them).
//
// Wall orientation: the `Board` stores every non-carved tile as `Wall`, so the
// View decides a wall's appearance from its neighbourhood. A wall renders as
// `-` when it caps a chamber floor above or below, as `|` when a chamber floor
// lies to its side (or only diagonally, i.e. a chamber corner), and as a blank
// background space otherwise. This reproduces the design's "Default Fixed
// Layout" figure, where only chamber perimeters are drawn and the space between
// chambers/corridors is left blank.

#include "runevault/view.h"

#include "runevault/cell.h"
#include "runevault/level.h"

#include <string>

namespace runevault {
namespace {

// Pad with spaces or truncate `text` so the result is EXACTLY `width`
// characters. Keeps every status line at the 79-column frame width so the frame
// stays 30 rows × 79 cols with colour off (Requirement 2.2).
std::string fitToWidth(const std::string &text, int width) {
    const std::size_t w = static_cast<std::size_t>(width);
    if (text.size() >= w) {
        return text.substr(0, w);
    }
    std::string out = text;
    out.append(w - out.size(), ' ');
    return out;
}

// True iff `(row, col)` is in bounds and is a chamber FLOOR tile. Walls,
// corridors, archways and the (special) stairway type are not chamber floor, so
// only real chamber interiors drive wall orientation.
bool isFloorAt(const Board &board, int row, int col) {
    const Position p{row, col};
    return board.inBounds(p) && board.at(p).type == CellType::Floor;
}

} // namespace

View::View(bool ansiColour) : ansiColour_(ansiColour) {}

char View::wallGlyph(const Board &board, const Position &p) const {
    const bool floorN = isFloorAt(board, p.row - 1, p.col);
    const bool floorS = isFloorAt(board, p.row + 1, p.col);
    const bool floorE = isFloorAt(board, p.row, p.col + 1);
    const bool floorW = isFloorAt(board, p.row, p.col - 1);

    // Floor directly above/below -> this wall caps a chamber horizontally.
    if (floorN || floorS) {
        return glyphs::kWallHorizontal;
    }
    // Floor directly to a side -> vertical side wall.
    if (floorE || floorW) {
        return glyphs::kWallVertical;
    }
    // Only a diagonal chamber floor -> a chamber corner; drawn as `|`.
    const bool floorDiag =
        isFloorAt(board, p.row - 1, p.col - 1) ||
        isFloorAt(board, p.row - 1, p.col + 1) ||
        isFloorAt(board, p.row + 1, p.col - 1) ||
        isFloorAt(board, p.row + 1, p.col + 1);
    if (floorDiag) {
        return glyphs::kWallVertical;
    }
    // Structure-free background: leave blank so the layout matches the figure.
    return glyphs::kBlank;
}

char View::cellGlyph(const Board &board, const Position &p) const {
    const Cell &cell = board.at(p);
    switch (cell.type) {
    case CellType::Floor:
        return glyphs::kFloor;
    case CellType::Corridor:
        return glyphs::kCorridor;
    case CellType::Archway:
        return glyphs::kArchway;
    case CellType::Stairway:
        // A cell typed as Stairway renders hidden by default; the revealed `\`
        // is driven by Level state in renderFrame.
        return glyphs::kStairwayHidden;
    case CellType::Wall:
        return wallGlyph(board, p);
    }
    return glyphs::kBlank;
}

std::string View::decorate(char glyph) const {
    if (!ansiColour_) {
        return std::string(1, glyph);
    }
    const char *code = nullptr;
    switch (glyph) {
    case glyphs::kArcanist:
        code = "1;33"; // bright yellow
        break;
    case glyphs::kStairwayRevealed:
        code = "1;36"; // cyan
        break;
    case glyphs::kRuneFragment:
        code = "1;35"; // magenta
        break;
    case glyphs::kScroll:
        code = "1;34"; // blue
        break;
    case glyphs::kWraith:
    case glyphs::kBanshee:
    case glyphs::kRevenant:
    case glyphs::kShade:
    case glyphs::kLich:
    case glyphs::kVaultAnchor:
    case glyphs::kSpecterLord:
        code = "1;31"; // red
        break;
    default:
        break;
    }
    if (code == nullptr) {
        return std::string(1, glyph);
    }
    return std::string("\x1b[") + code + "m" + glyph + "\x1b[0m";
}

std::string View::renderFrame(const Level &level,
                              const RenderOverlay &overlay) const {
    const Board &board = level.board();
    const int rows = board.rows();
    const int cols = board.cols();

    // Flatten the overlay into a per-cell lookup; later entries overwrite
    // earlier ones at the same position (an actor drawn over an item).
    std::vector<std::vector<char>> overlayGlyph(
        rows, std::vector<char>(cols, '\0'));
    for (const auto &entry : overlay.glyphs) {
        const Position &p = entry.first;
        if (p.row >= 0 && p.row < rows && p.col >= 0 && p.col < cols) {
            overlayGlyph[p.row][p.col] = entry.second;
        }
    }

    const Position stairway = level.stairway();
    const bool stairwayIsFloor =
        board.inBounds(stairway) && board.at(stairway).isWalkable();

    std::string out;
    out.reserve(static_cast<std::size_t>(kFrameRows) * (kFrameCols + 1));

    // --- Playable region: the board tiles (Requirement 2.3) ----------------
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const Position p{r, c};
            char glyph;
            if (overlayGlyph[r][c] != '\0') {
                // Actors / items sit on top of everything (Requirement 2.4/2.5).
                glyph = overlayGlyph[r][c];
            } else if (stairwayIsFloor && p == stairway) {
                // Stairway hidden as floor until revealed (Requirements 2.6/2.7).
                glyph = level.stairwayRevealed() ? glyphs::kStairwayRevealed
                                                 : glyphs::kStairwayHidden;
            } else {
                glyph = cellGlyph(board, p);
            }
            out += decorate(glyph);
        }
        out += '\n';
    }

    // --- Status area: blank rows (no status supplied) -----------------------
    for (int r = 0; r < kStatusRows; ++r) {
        out.append(static_cast<std::size_t>(kFrameCols), glyphs::kBlank);
        out += '\n';
    }

    return out;
}

std::vector<std::string> View::buildStatusRows(const StatusInfo &status) const {
    // Row layout across the bottom 5 rows (Requirement 2.2 / 2.8):
    //   row 0 : separator rule
    //   row 1 : class + FP + Pwr + WR
    //   row 2 : total Rune Fragments
    //   row 3+: identified-scroll hints / messages (one per row, truncated)
    std::vector<std::string> rows;
    rows.reserve(static_cast<std::size_t>(kStatusRows));

    // Separator rule spanning the full frame width.
    rows.push_back(std::string(static_cast<std::size_t>(kFrameCols),
                               glyphs::kWallHorizontal));

    // Class + combat stats. FP is shown as current/max (Requirement 2.8).
    std::string stats = "Class: " + status.className +
                        "   FP: " + std::to_string(status.fp) + "/" +
                        std::to_string(status.maxFp) +
                        "   Pwr: " + std::to_string(status.pwr) +
                        "   WR: " + std::to_string(status.wr);
    rows.push_back(fitToWidth(stats, kFrameCols));

    // Total Rune Fragments collected.
    std::string rf = "Rune Fragments: " + std::to_string(status.runeFragmentTotal);
    rows.push_back(fitToWidth(rf, kFrameCols));

    // Remaining rows: scroll hints / messages, most recent first, one per line.
    const int messageRows = kStatusRows - static_cast<int>(rows.size());
    for (int i = 0; i < messageRows; ++i) {
        if (i < static_cast<int>(status.messages.size())) {
            rows.push_back(fitToWidth(status.messages[i], kFrameCols));
        } else {
            rows.push_back(std::string(static_cast<std::size_t>(kFrameCols),
                                       glyphs::kBlank));
        }
    }

    return rows;
}

std::string View::renderFrame(const Level &level, const RenderOverlay &overlay,
                              const StatusInfo &status) const {
    // Render the board + blank status area, then replace the blank status rows
    // with the populated ones. The board portion is identical to the
    // overlay-only overload (Requirement 2.3).
    std::string out = renderFrame(level, overlay);

    // Drop the trailing blank status rows so we can append the populated ones.
    // Each row (board and status) is followed by '\n'; find the split point at
    // the end of the board region.
    std::size_t pos = 0;
    int newlines = 0;
    for (std::size_t i = 0; i < out.size() && newlines < kPlayableRows; ++i) {
        if (out[i] == '\n') {
            ++newlines;
            pos = i + 1;
        }
    }
    out.erase(pos);

    for (const std::string &row : buildStatusRows(status)) {
        out += row;
        out += '\n';
    }
    return out;
}

std::string View::renderFrame(const Level &level) const {
    return renderFrame(level, RenderOverlay{});
}

void View::render(std::ostream &os, const Level &level,
                  const RenderOverlay &overlay) const {
    os << renderFrame(level, overlay);
}

} // namespace runevault
