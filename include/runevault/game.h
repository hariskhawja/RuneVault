// game.h
// RuneVault — the Game controller: main loop + session controls (task 17.1).
//
// `Game` is the MVC **Controller** (design "Game (Controller)"): it owns the
// synchronous read-eval-render loop, mediates between the `Model` (state/rules)
// and the `View` (rendering), and translates discrete text commands read from
// an input stream into Model operations, redrawing the whole board after each.
//
// TESTABILITY (critical design choice): the loop reads from a `std::istream&`
// and writes to a `std::ostream&` (defaulting to std::cin / std::cout) rather
// than hard-coding the standard streams. Tests drive a Game with a
// `std::istringstream` of scripted commands and capture output in a
// `std::ostringstream`, so the full session loop is exercised without a TTY.
//
// SCOPE (task 17.1): this task implements the OUTER SESSION LOOP and the
// prompts/session controls:
//   * Requirement 1.1 — at the start of a run, prompt the player to select an
//     Arcanist class (s=Sage, h=Hexblade, w=Warden, v=Voidwalker) or quit (q).
//   * Requirements 1.2-1.5 — a valid key builds the Arcanist via
//     ArcanistFactory::create(key), sets it on the Model, chooses the run's
//     Aegis level (Spawner::chooseAegisLevel), builds + populates the first
//     level (Spawner), sets levelNumber 1, and begins play.
//   * Requirement 1.6 — `q` at the class-selection prompt quits without a run.
//   * Requirement 1.7 — invalid class-selection input re-prompts (loop until a
//     valid key or q).
//   * Requirement 11.3 — `q` quits the game (from the play loop too).
//   * Requirement 11.5 — when a run ends by win or loss, offer a play-again
//     prompt; on yes start a fresh run (new class selection), on no exit.
//   * Requirements 11.2/11.3 — `r` at the class-selection / play-again / play
//     loop restarts into a fresh class selection.
//
// The per-command DISPATCH (move/use/attack) and the turn model are finalized
// in task 17.2: dispatchCommand returns whether the command CONSUMED a turn,
// the play loop advances the Spectres only on a consumed turn (Requirement
// 11.4), redraws the ENTIRE board after every command (Requirement 2.3), and
// ignores invalid/no-op commands without consuming a turn.
//
// RESTART RESET (task 17.3, Requirement 11.2): pressing `r` during play returns
// SessionAction::Restart; the Controller then RESETS the run to a clean slate
// via resetRun() (which reconstructs the Model, zeroing Arcanist stats, active
// scroll effects, the Rune Fragment total, and the global lichAggroTriggered
// flag) and loops back to require a NEW class selection. This is a brand-new
// game and, unlike a level DESCENT (task 16.1), retains NOTHING — not the Aegis
// Cloak, not carried relics, not identified scrolls.

#ifndef RUNEVAULT_GAME_H
#define RUNEVAULT_GAME_H

#include <iostream>
#include <string>

#include "runevault/layout_loader.h"
#include "runevault/model.h"
#include "runevault/view.h"

namespace runevault {

class Game {
public:
    // Construct the controller over the given input/output streams. Defaults to
    // std::cin / std::cout for real play; tests inject string streams so the
    // loop is fully driveable/observable (see header notes).
    explicit Game(std::istream &in = std::cin, std::ostream &out = std::cout);

    // --- Optional level-layout file wiring (task 19.1, Requirement 12.1) --
    // Drive every run's five levels from a parsed layout FILE instead of the
    // random default layout. main() calls this once at startup when a valid
    // layout file was supplied on the command line (see main.cpp). The Game
    // keeps its OWN copy of the LoadedLayout so it outlives each run; every
    // startNewRun then points the Model at it (Model::setLayout) so the first
    // level and every descent are built from the file (board + digit items +
    // an actors-only Spawner pass). Calling it with an un-parsed layout is a
    // no-op guarded by LoadedLayout::ok. When never called, runs use the random
    // default layout exactly as before.
    void useLayout(const LoadedLayout &layout);

    // --- Runtime BONUS toggle (project spec: WITH/WITHOUT bonus, no recompile) --
    // Enable/disable the bonus features for this Game (default OFF = base game).
    // main() calls this once at startup from CliOptions::bonusEnabled. It gates
    // BOTH bonus additions at runtime with no recompilation:
    //   * the per-turn Notifications board — redraw() prints the "Notifications:"
    //     section ONLY when enabled (so the base game stays a strict 79x30 frame).
    //   * the Prestige promotion — startNewRun() forwards this to the Model
    //     (Model::setBonusEnabled) so Model::maybePromote only fires when enabled.
    void setBonusEnabled(bool v) { bonusEnabled_ = v; }

    // Run the session loop until the player quits. Returns a process exit code
    // (0). Structure (design "Main loop"):
    //   LOOP
    //     key := promptClassSelection()   // s/h/w/v or q
    //     IF key = quit THEN break        // Requirement 1.6
    //     startNewRun(key)                // Requirements 1.2-1.5
    //     action := playLoop()            // reads commands until end/quit/restart
    //     IF action = Quit THEN break     // Requirement 11.3
    //     IF action = Restart THEN continue
    //     reportOutcome(); IF NOT playAgain() THEN break   // Requirement 11.5
    //   END LOOP
    int run();

private:
    // Outcome of one play session, steering the outer run() loop.
    enum class SessionAction { Quit, Restart, RunEnded };

    // Print the class-selection prompt and read a selection, re-prompting on
    // invalid input (Requirement 1.7). Returns the chosen class key ('s', 'h',
    // 'w', 'v') or 'q' to quit (Requirement 1.6). End-of-input is treated as
    // quit so the loop always terminates.
    char promptClassSelection();

    // Reset the run to a brand-new, clean slate (task 17.3, Requirement 11.2).
    // Reconstructs the Model (`model_ = Model()`), which zeroes EVERY piece of
    // run-scoped state the requirement enumerates:
    //   * Arcanist stats      — baseArcanist_ cleared (a fresh class Arcanist is
    //                           built by startNewRun/setArcanist afterwards).
    //   * active scroll effects — activeArcanist_ decorator chain dropped (a
    //                           fresh BaseArcanistComponent is rebuilt on the
    //                           next setArcanist; no temporary decorators carry
    //                           over).
    //   * total Rune Fragments — runeFragmentTotal_ back to 0.
    //   * lichAggroTriggered   — global aggro flag back to false.
    // It ALSO clears every other carry/run flag: carryingCipherGem_,
    // carryingAegisCloak_, aegisSpawnedThisRun_, aegisLevel_, the identified-
    // scroll set, any dropped-gem record, and runState_ back to Playing.
    //
    // CONTRAST with a level DESCENT (Model::descend, task 16.1): a descent
    // RETAINS the Aegis Cloak and the permanent (base-Arcanist) FP gains while
    // only tearing down the current level's temporary scroll effects. A RESTART
    // is a brand-new game: it retains NOTHING from the prior run.
    void resetRun();

    // Build and begin a fresh run for the valid class `classKey` (Requirements
    // 1.1-1.5): reset to a clean Model (resetRun, Requirement 11.2), Arcanist
    // from the factory, run-start Aegis-level choice, first level built +
    // populated by the Spawner, level number 1, and an initial redraw.
    void startNewRun(char classKey);

    // The per-run play loop: read a command and act on it, redrawing after each
    // recognized command, until the run ends (runState() != Playing), the
    // player quits (`q`), or restarts (`r`). Returns which of those occurred.
    SessionAction playLoop();

    // Per-turn dispatch (task 17.2). Translate one turn command into its Model
    // action and report whether the turn was CONSUMED:
    //   * movement token (no/so/ea/we/ne/nw/se/sw) -> Model::moveArcanist
    //   * `u <dir>` -> Model::useScroll
    //   * `a <dir>` -> Model::handleAttack
    // Requirement 11.1: the command is dispatched to the corresponding action
    // (the caller redraws after). Requirement 11.4: a command that resolves to
    // a no-op (moving into a wall, using with no scroll in that direction,
    // attacking empty air) OR an unrecognized token returns false — the turn is
    // NOT consumed, so the Spectres do not get to act. Returns true only when
    // the Model action actually took effect (the turn is consumed).
    bool dispatchCommand(const std::string &cmd);

    // Report the run outcome after the play loop ends by win or loss
    // (Requirement 11.5 lead-in): a win line (with the final score) or a loss
    // line.
    void reportOutcome();

    // Offer the play-again prompt (Requirement 11.5). Returns true to start a
    // fresh run (new class selection), false to exit. End-of-input -> false.
    bool playAgainPrompt();

    // Redraw the entire board + status area to the output stream from the
    // current Model snapshot (Requirement 2.3 full redraw). No-op if no level.
    void redraw();

    // Build the View overlay from the live Model: items, then living Spectres,
    // then the Arcanist `@` on top (later entries win on shared tiles).
    RenderOverlay buildOverlay() const;

    // Build the status snapshot (class name, FP/maxFP, effective Pwr/WR, RF
    // total) the View status area displays (Requirement 2.8).
    StatusInfo buildStatus() const;

    // The raw command the player entered most recently this run, echoed in the
    // status area as "Action: <cmd>". Recorded for EVERY turn command whether it
    // succeeded or was a no-op (the spec's status area is free-form, and showing
    // the attempted action helps the player see that input registered). Cleared
    // at the start of each new/restarted run.
    std::string lastAction_;

    std::istream &in_;
    std::ostream &out_;
    Model model_;
    View view_;

    // Optional layout-file source for a file-driven run (task 19.1). `hasLayout_`
    // gates use; `layout_` is the Game-owned copy the Model points at each run
    // (its address is stable for the Game's lifetime, so the Model's non-owning
    // pointer never dangles). Default: no layout — the random default path.
    bool hasLayout_ = false;
    LoadedLayout layout_;

    // Runtime bonus toggle (default OFF = spec-compliant base game). Gates the
    // Notifications board printing in redraw() and, forwarded to the Model each
    // run, the Prestige promotion in Model::maybePromote. See setBonusEnabled.
    bool bonusEnabled_ = false;
};

} // namespace runevault

#endif // RUNEVAULT_GAME_H
