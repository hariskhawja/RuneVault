// test_view.cpp
// RuneVault — board frame renderer tests (task 3.1).
//
// Uses the assert-based harness (test_util.h): each RV_TEST auto-registers and
// the shared runner in test_main.cpp drives them. With ANSI colour OFF these
// tests assert the frame geometry (30 rows × 79 cols), the DD-7 glyphs for
// known cells of the default board, stairway hidden/revealed rendering, overlay
// placement, and that scrolls always render as `?`
// (Requirements 2.3, 2.4, 2.5, 2.6, 2.7, 2.9).

#include "test_util.h"

#include "runevault/level.h"
#include "runevault/view.h"

#include <string>
#include <vector>

using namespace runevault;

namespace {

// Split a rendered frame into its rows (each row was terminated with '\n').
std::vector<std::string> splitLines(const std::string &frame) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : frame) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

} // namespace

RV_TEST(view_frame_has_30_rows_of_width_79) {
    Level level(makeDefaultBoard());
    View view; // colour OFF
    std::vector<std::string> lines = splitLines(view.renderFrame(level));

    // 79 columns × 30 rows total frame (Requirements 2.1, 2.2).
    RV_CHECK(lines.size() == 30);
    for (const std::string &row : lines) {
        RV_CHECK(row.size() == 79);
    }
    // Frame dimension constants agree with the board playable region.
    RV_CHECK(kFrameRows == 30);
    RV_CHECK(kFrameCols == 79);
    RV_CHECK(kStatusRows == 5);
}

RV_TEST(view_status_area_is_blank) {
    Level level(makeDefaultBoard());
    View view;
    std::vector<std::string> lines = splitLines(view.renderFrame(level));

    // The bottom 5 rows (25..29) are the status area — blank for this task.
    for (int r = kPlayableRows; r < kFrameRows; ++r) {
        RV_CHECK(lines[r] == std::string(79, ' '));
    }
}

RV_TEST(view_renders_wall_archway_corridor_floor_glyphs) {
    Level level(makeDefaultBoard());
    View view;
    std::vector<std::string> lines = splitLines(view.renderFrame(level));

    // Chamber 1 occupies rows 0..5, cols 0..10 (see level.cpp).
    // Corner -> `|`; top-wall interior -> `-`; side wall -> `|`.
    RV_CHECK(lines[0][0] == '|');  // top-left corner
    RV_CHECK(lines[0][5] == '-');  // top wall interior segment
    RV_CHECK(lines[5][5] == '-');  // bottom wall interior segment
    RV_CHECK(lines[1][0] == '|');  // left side wall
    // Chamber interior floor.
    RV_CHECK(lines[3][3] == '.');
    // Archway at the chamber-1 right wall where the 1-2 corridor connects,
    // and the corridor tile just past it (Requirement 3.5 / DD-7).
    RV_CHECK(lines[2][10] == '+'); // archway
    RV_CHECK(lines[2][11] == '#'); // corridor
    // Structure-free background between chambers stays blank.
    RV_CHECK(lines[1][11] == ' ');
}

RV_TEST(view_stairway_hidden_renders_as_floor) {
    Level level(makeDefaultBoard());
    level.setStairway(Position{3, 6}); // a floor tile in chamber 1
    level.setStairwayRevealed(false);

    View view;
    std::vector<std::string> lines = splitLines(view.renderFrame(level));
    // Hidden stairway is indistinguishable from a floor tile (Requirement 2.6).
    RV_CHECK(lines[3][6] == '.');
}

RV_TEST(view_stairway_revealed_renders_as_backslash) {
    Level level(makeDefaultBoard());
    level.setStairway(Position{3, 6});
    level.setStairwayRevealed(true);

    View view;
    std::vector<std::string> lines = splitLines(view.renderFrame(level));
    // Revealed stairway renders as `\` once the Cipher Gem is carried (2.7).
    RV_CHECK(lines[3][6] == '\\');
}

RV_TEST(view_overlay_glyph_appears_at_position) {
    Level level(makeDefaultBoard());
    View view;

    RenderOverlay overlay;
    overlay.add(Position{3, 4}, glyphs::kArcanist); // @ in chamber 1
    overlay.add(Position{2, 2}, glyphs::kScroll);   // a scroll renders `?`
    overlay.add(Position{4, 7}, glyphs::kWraith);   // a Spectre glyph

    std::vector<std::string> lines = splitLines(view.renderFrame(level, overlay));
    RV_CHECK(lines[3][4] == '@');
    RV_CHECK(lines[2][2] == '?'); // scrolls ALWAYS render as `?`
    RV_CHECK(lines[4][7] == 'W');
    // A cell with no overlay still shows its floor glyph.
    RV_CHECK(lines[3][3] == '.');
}

RV_TEST(view_overlay_takes_precedence_over_stairway) {
    Level level(makeDefaultBoard());
    level.setStairway(Position{3, 6});
    level.setStairwayRevealed(true);

    RenderOverlay overlay;
    overlay.add(Position{3, 6}, glyphs::kArcanist); // Arcanist standing on stairs

    View view;
    std::vector<std::string> lines = splitLines(view.renderFrame(level, overlay));
    RV_CHECK(lines[3][6] == '@');
}

RV_TEST(view_later_overlay_entry_wins_at_same_cell) {
    Level level(makeDefaultBoard());
    RenderOverlay overlay;
    overlay.add(Position{3, 3}, glyphs::kCipherGem); // dropped item
    overlay.add(Position{3, 3}, glyphs::kArcanist);  // actor drawn on top

    View view;
    std::vector<std::string> lines = splitLines(view.renderFrame(level, overlay));
    RV_CHECK(lines[3][3] == '@');
}

RV_TEST(view_ansi_colour_off_produces_plain_glyphs) {
    Level level(makeDefaultBoard());
    RenderOverlay overlay;
    overlay.add(Position{3, 4}, glyphs::kArcanist);

    View view(false);
    RV_CHECK(!view.ansiColourEnabled());
    std::string frame = view.renderFrame(level, overlay);
    // No escape sequences when colour is off.
    RV_CHECK(frame.find('\x1b') == std::string::npos);
}

RV_TEST(view_ansi_colour_on_wraps_selected_glyphs) {
    Level level(makeDefaultBoard());
    RenderOverlay overlay;
    overlay.add(Position{3, 4}, glyphs::kArcanist);

    View view(true);
    RV_CHECK(view.ansiColourEnabled());
    std::string frame = view.renderFrame(level, overlay);
    // The Arcanist glyph is colourised (Requirement 2.9).
    RV_CHECK(frame.find("\x1b[") != std::string::npos);
    RV_CHECK(frame.find("\x1b[1;33m@\x1b[0m") != std::string::npos);
}

// --- Status-area renderer tests (task 3.2, Requirements 2.2 / 2.8) ---------

// Return only the bottom 5 status rows of a rendered frame.
static std::vector<std::string> statusRows(const std::string &frame) {
    std::vector<std::string> lines = splitLines(frame);
    std::vector<std::string> status;
    for (std::size_t r = static_cast<std::size_t>(kPlayableRows);
         r < lines.size(); ++r) {
        status.push_back(lines[r]);
    }
    return status;
}

// True iff `needle` occurs somewhere in the status rows.
static bool statusContains(const std::vector<std::string> &rows,
                           const std::string &needle) {
    for (const std::string &row : rows) {
        if (row.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

RV_TEST(view_status_frame_stays_30_rows_of_width_79) {
    Level level(makeDefaultBoard());
    View view; // colour OFF

    StatusInfo status;
    status.className = "Sage";
    status.fp = 100;
    status.maxFp = 120;
    status.pwr = 18;
    status.wr = 22;
    status.runeFragmentTotal = 7;
    status.messages.push_back("You feel your focus sharpen.");

    std::vector<std::string> lines =
        splitLines(view.renderFrame(level, RenderOverlay{}, status));

    // Supplying a StatusInfo must not change the frame geometry (Req 2.2).
    RV_CHECK(lines.size() == 30);
    for (const std::string &row : lines) {
        RV_CHECK(row.size() == 79);
    }
}

RV_TEST(view_status_shows_class_fp_pwr_wr_and_rf_total) {
    Level level(makeDefaultBoard());
    View view;

    StatusInfo status;
    status.className = "Warden";
    status.fp = 143;
    status.maxFp = 150;
    status.pwr = 16;
    status.wr = 22;
    status.runeFragmentTotal = 12;

    std::vector<std::string> rows =
        statusRows(view.renderFrame(level, RenderOverlay{}, status));

    // Class, FP, Pwr, WR, and RF total all appear in the status area (Req 2.8).
    RV_CHECK(statusContains(rows, "Warden"));
    RV_CHECK(statusContains(rows, "143"));   // current FP
    RV_CHECK(statusContains(rows, "150"));   // max FP
    RV_CHECK(statusContains(rows, "Pwr: 16"));
    RV_CHECK(statusContains(rows, "WR: 22"));
    RV_CHECK(statusContains(rows, "12"));    // RF total
    RV_CHECK(statusContains(rows, "Rune Fragments"));
}

RV_TEST(view_status_displays_supplied_message) {
    Level level(makeDefaultBoard());
    View view;

    StatusInfo status;
    status.className = "Hexblade";
    status.messages.push_back("Attune Focus scroll identified.");

    std::vector<std::string> rows =
        statusRows(view.renderFrame(level, RenderOverlay{}, status));
    RV_CHECK(statusContains(rows, "Attune Focus scroll identified."));
}

RV_TEST(view_status_truncates_long_messages_to_79_cols) {
    Level level(makeDefaultBoard());
    View view;

    StatusInfo status;
    status.className = "Voidwalker";
    // A message far longer than the 79-column frame width.
    status.messages.push_back(std::string(200, 'x'));

    std::vector<std::string> rows =
        statusRows(view.renderFrame(level, RenderOverlay{}, status));
    // Exactly 5 status rows, each clamped to 79 columns (Req 2.2).
    RV_CHECK(rows.size() == 5);
    for (const std::string &row : rows) {
        RV_CHECK(row.size() == 79);
    }
}

RV_TEST(view_status_board_region_matches_overlay_only_overload) {
    Level level(makeDefaultBoard());
    level.setStairway(Position{3, 6});
    level.setStairwayRevealed(true);

    RenderOverlay overlay;
    overlay.add(Position{3, 4}, glyphs::kArcanist);

    View view;
    StatusInfo status;
    status.className = "Sage";

    std::vector<std::string> boardOnly =
        splitLines(view.renderFrame(level, overlay));
    std::vector<std::string> withStatus =
        splitLines(view.renderFrame(level, overlay, status));

    // The top 25 board rows are byte-identical whether or not status is drawn.
    for (int r = 0; r < kPlayableRows; ++r) {
        RV_CHECK(boardOnly[r] == withStatus[r]);
    }
}
