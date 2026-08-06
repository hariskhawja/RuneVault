// test_game.cpp
// RuneVault — Game controller session loop + session controls (task 17.1,
// Requirements 1.1/1.6/1.7/11.3/11.5).
//
// The Game reads commands from a std::istream and renders to a std::ostream, so
// these tests drive it with a std::istringstream of scripted input and capture
// output in a std::ostringstream — no TTY needed. rng is seeded first because
// startNewRun populates a level via the Spawner.
//
// Assertions search for substrings actually printed (the class-selection prompt
// text, the "@" Arcanist glyph, the "Sage" status label) rather than matching
// whole frames, so they stay robust to exact wording/layout.

#include "test_util.h"

#include "runevault/game.h"
#include "shuffle.h" // rng::seed

#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace runevault;

// Count non-overlapping occurrences of `needle` in `haystack`.
int countOccurrences(const std::string &haystack, const std::string &needle) {
    if (needle.empty()) {
        return 0;
    }
    int count = 0;
    std::string::size_type pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

// The class-selection prompt token used to detect (re-)prompting.
const char *kPromptToken = "Select your Arcanist class";

// A per-frame sentinel: the status area prints "Class:" exactly once per
// rendered frame (see View::buildStatusRows), so counting it counts frames.
const char *kFrameToken = "Class:";

// Split the captured session output into rendered frames. Each redraw() emits a
// full frame (every row terminated by '\n') followed by one extra blank line,
// so frames are separated by a blank line ("\n\n"). The class-selection prompt
// precedes the first frame (stripped by the caller when comparing frame bodies).
std::vector<std::string> splitFrames(const std::string &text) {
    std::vector<std::string> frames;
    std::string::size_type start = 0;
    while (start < text.size()) {
        const std::string::size_type end = text.find("\n\n", start);
        if (end == std::string::npos) {
            break;
        }
        frames.push_back(text.substr(start, end - start));
        start = end + 2;
    }
    return frames;
}

// Strip the trailing class-selection prompt ("...> ") that precedes the first
// frame so its board body can be compared against later frames.
std::string stripPrompt(const std::string &frame) {
    const std::string::size_type p = frame.rfind("> ");
    return p == std::string::npos ? frame : frame.substr(p + 2);
}

} // namespace

// --- Requirement 1.6: `q` at class selection quits without starting a run ----
RV_TEST(game_quit_at_class_selection_returns_without_run) {
    rng::seed(12345);
    std::istringstream in("q\n");
    std::ostringstream out;
    Game game(in, out);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    RV_CHECK(contains(text, kPromptToken)); // prompt was shown (Requirement 1.1)
    // Quitting without a run means no board was rendered: no Arcanist glyph.
    RV_CHECK(!contains(text, "@"));
}

// --- Requirement 1.7: invalid input re-prompts, then quit --------------------
RV_TEST(game_invalid_class_reprompts_then_quits) {
    rng::seed(12345);
    std::istringstream in("z\nq\n");
    std::ostringstream out;
    Game game(in, out);

    game.run();
    const std::string text = out.str();

    // The prompt appears at least twice: once initially, once after the invalid
    // `z` re-prompts (Requirement 1.7).
    RV_CHECK(countOccurrences(text, kPromptToken) >= 2);
    // Still never started a run (quit after re-prompt).
    RV_CHECK(!contains(text, "@"));
}

// --- Requirements 1.1-1.5: a valid class starts a run (board is rendered) ----
RV_TEST(game_valid_class_starts_run_then_quits) {
    rng::seed(12345);
    std::istringstream in("s\nq\n");
    std::ostringstream out;
    Game game(in, out);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    // A run began: the board was rendered (Arcanist glyph) and the status area
    // shows the Sage class label.
    RV_CHECK(contains(text, "@"));
    RV_CHECK(contains(text, "Sage"));
}

// --- Requirement 1.7 (multiple invalids): several bad inputs keep re-prompting
RV_TEST(game_multiple_invalid_then_valid_then_quit) {
    rng::seed(12345);
    std::istringstream in("x\n1\nq\n");
    std::ostringstream out;
    Game game(in, out);

    game.run();
    const std::string text = out.str();

    // Two invalid inputs -> at least three prompt appearances (initial + 2
    // re-prompts) before the quit.
    RV_CHECK(countOccurrences(text, kPromptToken) >= 3);
    RV_CHECK(!contains(text, "@")); // quit, no run started
}

// --- Requirement 11.3: `q` during the play loop quits the game ---------------
RV_TEST(game_quit_during_play_loop_exits) {
    rng::seed(2024);
    // Select Warden, make a legal-or-illegal move, then quit from the play loop.
    std::istringstream in("w\nno\nq\n");
    std::ostringstream out;
    Game game(in, out);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    RV_CHECK(contains(text, "Warden")); // the Warden run started
    RV_CHECK(contains(text, "@"));      // board rendered
}

// --- Requirement 11.5: play-again prompt appears after a run ends ------------
// Drive the run to a loss by seeding a low-FP Arcanist is not directly possible
// through the Game (the factory sets class stats), so instead we verify the
// play-again path loosely: after quitting, no play-again prompt is required,
// but selecting a class then feeding `y` at a fresh selection cycle re-shows the
// class prompt. Here we confirm that quitting at the very first prompt never
// emits a play-again prompt (the prompt is only offered on win/loss, not quit).
RV_TEST(game_quit_does_not_offer_play_again) {
    rng::seed(12345);
    std::istringstream in("q\n");
    std::ostringstream out;
    Game game(in, out);

    game.run();
    const std::string text = out.str();

    // Requirement 11.5 scopes the play-again offer to win/loss endings; a plain
    // quit must not print it.
    RV_CHECK(!contains(text, "Play again"));
}

// --- Requirement 11.2 (partial): `r` at class selection re-prompts a new run -
// `r` is not a valid class key, so at the class-selection prompt it simply
// re-prompts (Requirement 1.7 path); the restart-into-new-selection semantics
// from the play loop are finalized in task 17.3. This guards that `r` before a
// run does not crash and keeps prompting until quit.
RV_TEST(game_r_before_run_reprompts_until_quit) {
    rng::seed(12345);
    std::istringstream in("r\nq\n");
    std::ostringstream out;
    Game game(in, out);

    game.run();
    const std::string text = out.str();

    RV_CHECK(countOccurrences(text, kPromptToken) >= 2);
    RV_CHECK(!contains(text, "@"));
}

// ============================================================================
// Task 17.2 — command dispatch with full redraw + turn model.
// Requirements 11.1 (dispatch + redraw), 11.4 (ignore invalid, no turn spent),
// 2.3 (redraw the ENTIRE board after every command).
// ============================================================================

// --- Requirement 2.3: every command yields a full redraw --------------------
// Frame count == initial redraw (startNewRun) + one redraw per turn command
// processed in the play loop (each of `no`, `ea`, and the invalid `zzz`), so a
// run of "s no ea zzz q" renders exactly 1 + 3 = 4 frames.
RV_TEST(game_full_redraw_after_every_command) {
    rng::seed(4242);
    std::istringstream in("s\nno\nea\nzzz\nq\n");
    std::ostringstream out;
    Game game(in, out);

    game.run();
    const std::string text = out.str();

    // Initial frame + one per turn command (including the invalid one, which
    // still redraws the unchanged board — Requirement 2.3).
    RV_CHECK(countOccurrences(text, kFrameToken) == 4);
    RV_CHECK(contains(text, "@")); // the board was rendered each time
}

// --- Requirement 11.4: an invalid command is ignored (no game-state change) --
// A garbage token ("zzz") is not a valid command: it must not change any GAME
// state (no turn consumed) but still triggers a full redraw. The status area's
// "Action:" field DOES change, since it echoes whatever the player entered
// (pass or fail) — so we compare the BOARD ROWS only, which must be identical.
RV_TEST(game_invalid_command_redraws_without_state_change) {
    rng::seed(4242);
    std::istringstream in("s\nzzz\nq\n");
    std::ostringstream out;
    Game game(in, out);

    game.run();
    const std::string text = out.str();

    // Two frames: the initial redraw and the post-invalid redraw.
    RV_CHECK(countOccurrences(text, kFrameToken) == 2);

    const std::vector<std::string> frames = splitFrames(text);
    RV_CHECK(frames.size() == 2u);

    // Compare only the board portion (everything before the status area, which
    // begins at the "Class:" row): the invalid command consumed no turn, so the
    // board is unchanged (Requirement 11.4).
    const std::string before = stripPrompt(frames[0]);
    const std::string after = frames[1];
    const std::string boardBefore = before.substr(0, before.find("Class:"));
    const std::string boardAfter = after.substr(0, after.find("Class:"));
    RV_CHECK(boardBefore == boardAfter);

    // The Action field echoed the invalid input.
    RV_CHECK(contains(text, "Action: zzz"));
    RV_CHECK(contains(text, "@")); // did not crash; board rendered
}

// --- Status area: Rune Fragments row also shows Level and Action -------------
// The status area's Rune Fragments row carries the current vault level and the
// command the player just entered ("Action: <cmd>"). On the very first frame no
// action has been taken yet, so Action shows "-"; after a move it echoes it.
RV_TEST(game_status_row_shows_level_and_action) {
    rng::seed(4242);
    std::istringstream in("s\nno\nq\n");
    std::ostringstream out;
    Game game(in, out);

    game.run();
    const std::string text = out.str();

    // Level is shown alongside the Rune Fragment total (level 1 at run start).
    RV_CHECK(contains(text, "Rune Fragments: "));
    RV_CHECK(contains(text, "Level: 1"));
    // The initial frame has no action yet; the post-move frame echoes "no".
    RV_CHECK(contains(text, "Action: -"));
    RV_CHECK(contains(text, "Action: no"));
}

// A directional `u`/`a` command echoes the FULL command including the direction.
RV_TEST(game_status_action_includes_direction_token) {
    rng::seed(4242);
    std::istringstream in("s\na ne\nq\n");
    std::ostringstream out;
    Game game(in, out);

    game.run();
    const std::string text = out.str();

    RV_CHECK(contains(text, "Action: a ne"));
}

// --- Requirement 11.1: a recognized movement command is dispatched + redrawn -
// A valid class then a movement token renders a second frame with the Arcanist
// glyph present (the command was dispatched to Model::moveArcanist and the
// board redrawn).
RV_TEST(game_valid_move_dispatches_and_redraws) {
    rng::seed(4242);
    std::istringstream in("s\nno\nq\n");
    std::ostringstream out;
    Game game(in, out);

    game.run();
    const std::string text = out.str();

    // Initial frame + one for the movement command.
    RV_CHECK(countOccurrences(text, kFrameToken) == 2);
    RV_CHECK(contains(text, "@")); // Arcanist rendered in the updated frame
}

// --- Requirement 11.4: a dangling `u`/`a` with no direction is a safe no-op --
// A `u` or `a` at end of input has no direction token to consume; the dispatch
// treats it as a no-op (no turn consumed) and the loop then hits end-of-input
// and quits without crashing.
RV_TEST(game_dangling_use_and_attack_are_noops) {
    rng::seed(4242);
    std::istringstream in("s\nu\n");
    std::ostringstream out;
    Game game(in, out);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    RV_CHECK(contains(text, "@")); // run started, board rendered, no crash
    // The dangling `u` consumes no turn but STILL triggers the per-command
    // redraw (Requirement 2.3): initial frame + one redraw for the `u`. EOF
    // then quits the loop.
    RV_CHECK(countOccurrences(text, kFrameToken) == 2);
}

// ============================================================================
// Task 17.3 — restart reset semantics (Requirement 11.2).
// `r` during active play resets the run to a clean slate and REQUIRES a new
// class selection before play resumes. The reset is observable through the
// Controller: the class-selection prompt is shown AGAIN after `r`, and picking
// a class after the restart starts a fresh, fully rendered run.
// ============================================================================

// --- Requirement 11.2: `r` during play returns to class selection ------------
// After selecting a class, taking a move, then pressing `r`, the class-selection
// prompt must appear AGAIN (a new class selection is required). So the prompt
// token appears at least twice: once at the start, once after the restart. The
// trailing `q` then quits cleanly.
RV_TEST(game_restart_during_play_returns_to_class_selection) {
    rng::seed(2024);
    std::istringstream in("s\nno\nr\nq\n");
    std::ostringstream out;
    Game game(in, out);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    // Prompt shown at the start AND again after `r` (Requirement 11.2 requires a
    // new class selection on restart).
    RV_CHECK(countOccurrences(text, kPromptToken) >= 2);
    // The initial run rendered a board (Sage selected first).
    RV_CHECK(contains(text, "Sage"));
}

// --- Requirement 11.2: restart then selecting a class starts a fresh run -----
// `r` restarts back to class selection; choosing a DIFFERENT class (Hexblade)
// afterwards must start a fresh, fully rendered run showing that class in the
// status area and a freshly drawn Arcanist.
RV_TEST(game_restart_then_new_class_starts_fresh_run) {
    rng::seed(2024);
    // Sage run, restart, then pick Hexblade, then quit.
    std::istringstream in("s\nr\nh\nq\n");
    std::ostringstream out;
    Game game(in, out);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    // Both classes appear: the original Sage run and the post-restart Hexblade
    // run (a fresh Arcanist was built after the reset — Requirement 11.2).
    RV_CHECK(contains(text, "Sage"));
    RV_CHECK(contains(text, "Hexblade"));
    // The fresh run rendered a board.
    RV_CHECK(contains(text, "@"));
}

// ============================================================================
// Feature — per-turn NOTIFICATION LOG print/clear wiring.
// Every redraw prints a "Notifications:" section then clears the log; a
// successful move logs "You move <dir>." which the following frame renders.
// ============================================================================

// --- The "Notifications:" header is printed on every redraw (bonus ON) -------
// The Notifications board is a BONUS feature (project spec: WITH/WITHOUT bonus,
// no recompile), so it prints only when bonus is enabled — set it before run().
RV_TEST(game_redraw_prints_notifications_section) {
    rng::seed(4242);
    std::istringstream in("s\nq\n");
    std::ostringstream out;
    Game game(in, out);
    game.setBonusEnabled(true);

    game.run();
    const std::string text = out.str();

    // With bonus enabled the header is printed on every redraw, including the
    // initial startNewRun redraw.
    RV_CHECK(contains(text, "Notifications:"));
}

// --- A successful move logs a "You move <dir>." line in the next frame -------
RV_TEST(game_successful_move_logs_move_notification) {
    rng::seed(4242);
    // Try all 8 directions so at least one is a legal move that logs a message
    // (the initial spawn tile has at least one walkable neighbour).
    std::istringstream in("s\nno\nso\nea\nwe\nne\nnw\nse\nsw\nq\n");
    std::ostringstream out;
    Game game(in, out);
    game.setBonusEnabled(true); // Notifications board only prints with bonus on

    game.run();
    const std::string text = out.str();

    // At least one legal move happened, so a "You move " line was rendered
    // before the log was cleared on that frame.
    RV_CHECK(contains(text, "You move "));
}

// --- Base game (bonus OFF, the default): NO Notifications section ------------
// Project spec compliance: the base game is a strict 79x30 frame with no extra
// notification lines, so after a turn the output must NOT contain the header
// or any turn message.
RV_TEST(game_base_game_omits_notifications_section) {
    rng::seed(4242);
    std::istringstream in("s\nno\nq\n");
    std::ostringstream out;
    Game game(in, out); // bonus defaults OFF

    game.run();
    const std::string text = out.str();

    RV_CHECK(!contains(text, "Notifications:"));
    RV_CHECK(!contains(text, "You move ")); // no turn messages leak into output
    RV_CHECK(contains(text, "@"));          // the base game still renders a board
}

// --- With bonus enabled: the Notifications section DOES appear after a turn --
RV_TEST(game_bonus_enabled_shows_notifications_section) {
    rng::seed(4242);
    std::istringstream in("s\nno\nq\n");
    std::ostringstream out;
    Game game(in, out);
    game.setBonusEnabled(true);

    game.run();
    const std::string text = out.str();

    RV_CHECK(contains(text, "Notifications:"));
}

// --- Requirement 11.2: repeated `r` keeps re-requiring a class selection -----
// Two restart cycles must each re-show the class-selection prompt: initial +
// two post-restart prompts == at least three prompt appearances before quit.
RV_TEST(game_repeated_restart_reprompts_each_time) {
    rng::seed(2024);
    std::istringstream in("s\nr\nw\nr\nq\n");
    std::ostringstream out;
    Game game(in, out);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    // Initial Sage prompt + prompt after first `r` (pick Warden) + prompt after
    // second `r` (then quit) == at least 3 appearances.
    RV_CHECK(countOccurrences(text, kPromptToken) >= 3);
    RV_CHECK(contains(text, "Warden")); // the second run started before restart
}
