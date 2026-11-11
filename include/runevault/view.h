#ifndef RUNEVAULT_VIEW_H
#define RUNEVAULT_VIEW_H

#include "runevault/board.h"
#include "runevault/geometry.h"

#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace runevault {

class Level;

constexpr int kFrameRows = 30;
constexpr int kFrameCols = kPlayableCols; // 79
constexpr int kStatusRows = kFrameRows - kPlayableRows; // 5

namespace glyphs {
constexpr char kArcanist = '@';
constexpr char kWallVertical = '|';
constexpr char kWallHorizontal = '-';
constexpr char kArchway = '+';
constexpr char kCorridor = '#';
constexpr char kFloor = '.';
constexpr char kStairwayHidden = '.';  // renders as floor until revealed (2.6)
constexpr char kStairwayRevealed = '\\'; // renders as `\` once carried (2.7)
constexpr char kScroll = '?';
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

struct StatusInfo {
    std::string className;
    int fp = 0;                         // current Focus Points
    int maxFp = 0;                      // class maximum FP (shown as fp/maxFp)
    int pwr = 0;                        // effective Power
    int wr = 0;                         // effective Ward Rating
    int runeFragmentTotal = 0;          // total Rune Fragments collected
    int levelNumber = 1;                // current vault level (1..5), shown
                                        // alongside the Rune Fragment total
    std::string lastAction;             // the command the player just entered
    std::vector<std::string> messages;
};

// Later entries draw over earlier ones at the same position.
struct RenderOverlay {
    std::vector<std::pair<Position, char>> glyphs;

    // Convenience: append an overlay glyph at `p`.
    void add(const Position &p, char glyph) { glyphs.emplace_back(p, glyph); }
};

// Renders the 79×30 frame from a read-only model snapshot. Stateless apart from
// the ANSI-colour flag chosen at construction.
// Renders a read-only snapshot into a 79-column, 30-row frame.
class View {
public:
    explicit View(bool ansiColour = false);

    bool ansiColourEnabled() const { return ansiColour_; }
    void setAnsiColour(bool enabled) { ansiColour_ = enabled; }

    // Build the full frame string with the given overlay. Rows are separated by
    // '\n'; with colour OFF every row is exactly `kFrameCols` characters wide.
    std::string renderFrame(const Level &level,
                            const RenderOverlay &overlay) const;

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
