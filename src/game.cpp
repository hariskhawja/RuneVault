// game.cpp
// RuneVault — the Game controller implementation (task 17.1).
//
// Implements the outer session loop and session controls (class selection,
// quit, restart, play-again) plus a minimal per-turn dispatch so the game is
// playable end-to-end. See game.h for the requirement mapping and the scope
// boundary with tasks 17.2 (full dispatch/redraw) and 17.3 (restart reset).

#include "runevault/game.h"

#include <memory>
#include <string>

#include "runevault/arcanist.h"
#include "runevault/arcanist_factory.h"
#include "runevault/geometry.h"
#include "runevault/level.h"
#include "runevault/notification_log.h"
#include "runevault/spawner.h"

namespace runevault {

namespace {

// The 8 movement command tokens (Requirement 5.1). A command equal to one of
// these is a movement request; anything else is a use/attack/session command.
bool isDirectionToken(const std::string &token) {
    return token == "no" || token == "so" || token == "ea" || token == "we" ||
           token == "ne" || token == "nw" || token == "se" || token == "sw";
}

} // namespace

Game::Game(std::istream &in, std::ostream &out)
    : in_(in), out_(out), model_(), view_(/*ansiColour=*/false) {}

void Game::useLayout(const LoadedLayout &layout) {
    // Task 19.1 (Requirement 12.1): adopt a file-driven layout for all runs. We
    // keep our OWN copy so it outlives each run; startNewRun points the Model at
    // &layout_ afterwards. A layout that failed to parse is ignored (guarded by
    // ok) so a malformed file — already reported at startup by main — never
    // silently drives play; runs then fall back to the random default layout.
    if (!layout.ok) {
        return;
    }
    layout_ = layout;
    hasLayout_ = true;
}

char Game::promptClassSelection() {
    // Requirement 1.1 / 1.7: prompt and re-prompt until a valid key or quit.
    while (true) {
        out_ << "Select your Arcanist class:\n"
             << "  s = Sage\n"
             << "  h = Hexblade\n"
             << "  w = Warden\n"
             << "  v = Voidwalker\n"
             << "  q = quit\n"
             << "> ";

        std::string token;
        if (!(in_ >> token)) {
            // End of input: treat as quit so the loop always terminates
            // (Requirement 1.6 semantics for a closed input stream).
            return 'q';
        }

        if (token.size() == 1) {
            const char key = token[0];
            if (key == 'q') {
                return 'q'; // Requirement 1.6: quit without starting a run.
            }
            // Requirements 1.2-1.5: only s/h/w/v are valid class keys. The
            // factory returns non-null exactly for these, so use it as the
            // single source of truth for validity.
            if (ArcanistFactory::create(key) != nullptr) {
                return key;
            }
        }
        // Requirement 1.7: any other input re-prompts (loop) without starting.
    }
}

void Game::resetRun() {
    // Requirement 11.2: a restart is a BRAND-NEW GAME. Reconstructing the Model
    // is the single mechanism that clears every run-scoped field at once:
    //   * Arcanist stats      -> baseArcanist_ null (a fresh class Arcanist is
    //                            attached next by startNewRun/setArcanist).
    //   * active scroll effects -> activeArcanist_ decorator chain dropped (no
    //                            temporary Surge/Fortify/Sap/Erode carries over;
    //                            it is rebuilt as a plain BaseArcanistComponent
    //                            when the next Arcanist is set).
    //   * total Rune Fragments -> runeFragmentTotal_ == 0.
    //   * lichAggroTriggered   -> global aggro flag == false.
    // Plus all other carry/run state: carryingCipherGem_, carryingAegisCloak_,
    // aegisSpawnedThisRun_, aegisLevel_ (0), the identified-scroll set, any
    // dropped-gem record, and runState_ (Playing).
    //
    // CONTRAST with a level DESCENT (Model::descend, task 16.1): a descent
    // deliberately RETAINS the Aegis Cloak and all permanent base-Arcanist FP
    // gains and only tears down the current level's temporary scroll effects. A
    // restart keeps NONE of that — a fresh Model wipes the Aegis, carried
    // relics, and identified scrolls too.
    model_ = Model();
}

void Game::startNewRun(char classKey) {
    // Requirement 11.2: start every run from a clean slate so no prior-run
    // state leaks in (the reset primitive shared with the `r` restart path).
    resetRun();

    // No action has been taken yet on a fresh run, so the status area's
    // "Action:" field starts empty (rendered as "-").
    lastAction_.clear();

    // Project spec (WITH/WITHOUT bonus, no recompile): resetRun() above did
    // `model_ = Model()`, which reset the fresh Model's bonus flag to false, so
    // re-apply the Game's bonus setting here on EVERY new/restarted run. This
    // lets Model::maybePromote fire (Prestige promotion) only when bonus is on.
    model_.setBonusEnabled(bonusEnabled_);

    // Feature: a new/restarted run starts with a clean notification log so no
    // messages from a prior run's final turn leak into the first frame.
    NotificationLog::instance().clear();

    // Requirements 1.2-1.5 / 1.8: build the class Arcanist (max FP == start FP
    // is enforced inside Arcanist) and attach it to the Model.
    model_.setArcanist(ArcanistFactory::create(classKey));

    // Run-start bookkeeping: level 1 and the run's Aegis-Cloak level choice
    // (Requirement 4.13). Chosen once here so the level build can consume it on
    // the selected level 2..4.
    model_.setLevelNumber(1);
    model_.setAegisLevel(Spawner::chooseAegisLevel());

    // Task 19.1 (Requirement 12.1): when a layout file was supplied, point the
    // Model at our owned copy so buildCurrentLevel() sources EVERY level (the
    // first and each descent) from the file. resetRun() above reconstructed the
    // Model (clearing any prior layout pointer), so we (re)set it here each run.
    // Without a layout file the pointer stays null and the random default path
    // is used unchanged.
    model_.setLayout(hasLayout_ ? &layout_ : nullptr);

    // Build + populate the first level. buildCurrentLevel() places the Arcanist,
    // stairway, spectres and items — from the layout file (board + digit items +
    // actors-only pass) when one is set, or the random default layout otherwise.
    // rng is seeded by the caller (main / tests) before run() so placement is
    // reproducible under a seed.
    model_.buildCurrentLevel();

    redraw();
}

Game::SessionAction Game::playLoop() {
    // Read-eval-render until the run ends or the player quits/restarts.
    while (model_.runState() == RunState::Playing) {
        std::string cmd;
        if (!(in_ >> cmd)) {
            return SessionAction::Quit; // closed input -> quit
        }
        if (cmd == "q") {
            return SessionAction::Quit; // Requirement 11.3
        }
        if (cmd == "r") {
            // Requirement 11.2: `r` during play restarts the game. The full
            // reset (resetRun) + new class selection are performed by run() when
            // it handles this SessionAction::Restart.
            return SessionAction::Restart;
        }

        // Record the entered command for the status area's "Action:" field. The
        // `u`/`a` branches of dispatchCommand append their direction token, so
        // the echoed action reads e.g. "a ne". Recorded regardless of whether
        // the command succeeds or is a no-op.
        lastAction_ = cmd;

        // Snapshot the level number so we can tell whether the player's move
        // triggered a descent (moveArcanist -> tryDescend rebuilds the level).
        const int levelBefore = model_.levelNumber();

        // Requirement 11.1: dispatch the turn command to its Model action.
        // Turn ordering (Requirement 6.1): the Arcanist has initiative — an
        // attack already resolves inside handleAttack before the Spectres act.
        const bool turnConsumed = dispatchCommand(cmd);

        // Requirement 11.4: "consuming a turn" == the Spectres get to act. So
        // advance the Spectres ONLY when the command actually consumed a turn.
        // A no-op or unrecognized command (turnConsumed == false) skips this,
        // so the Spectres do not move on an invalid input.
        //
        // Two extra guards keep the phase well-behaved:
        //   * runState() must still be Playing — a win reached inside the move
        //     (descend on level 5) has already ended the run, so the Spectres
        //     do not act after it.
        //   * the level number must be unchanged — if the move descended to a
        //     new level (levels 1..4), the fresh level's Spectres do not act on
        //     the turn the player arrived. This avoids the newly-spawned
        //     Spectres immediately moving/attacking on the descent turn; the
        //     spec does not require them to act here (documented design choice).
        if (turnConsumed && model_.runState() == RunState::Playing &&
            model_.levelNumber() == levelBefore) {
            model_.advanceSpectres(); // Spectre action phase (before the redraw
                                      // so their new positions are shown).
        }

        // Requirement 2.3: redraw the ENTIRE board after EVERY command — even a
        // no-op redraws the current (unchanged) state.
        redraw();
    }
    return SessionAction::RunEnded;
}

bool Game::dispatchCommand(const std::string &cmd) {
    // Requirement 11.1: translate one turn command into its Model action and
    // report whether the turn was CONSUMED. Requirement 11.4: a no-op or
    // unrecognized command returns false so the caller does not advance the
    // Spectres.
    if (isDirectionToken(cmd)) {
        // moveArcanist returns false on a wall / out-of-bounds no-op
        // (Requirements 5.3/11.4): no move, turn not consumed.
        return model_.moveArcanist(cmd);
    }
    if (cmd == "u") {
        std::string dir;
        if (in_ >> dir) {
            lastAction_ = cmd + " " + dir; // echo the full command (e.g. "u no")
            // useScroll returns false when there is no scroll one step in that
            // direction (Requirements 8.2/11.4) — a no-op, turn not consumed.
            // An unrecognized direction token is likewise a no-op (false).
            return model_.useScroll(dir);
        }
        return false; // dangling `u` at end of input: no-op, turn not consumed.
    }
    if (cmd == "a") {
        std::string dir;
        if (in_ >> dir) {
            lastAction_ = cmd + " " + dir; // echo the full command (e.g. "a ne")
            try {
                // handleAttack returns false when there is no adjacent living
                // Spectre to strike (Requirements 6.3/11.4): a no-op, turn not
                // consumed.
                return model_.handleAttack(Geometry::parse(dir));
            } catch (...) {
                // Bad/unrecognized direction token: no-op (Requirement 11.4).
                return false;
            }
        }
        return false; // dangling `a` at end of input: no-op, turn not consumed.
    }
    // Requirement 11.4: any other token is not a valid command — ignore it and
    // do NOT consume the turn.
    return false;
}

void Game::reportOutcome() {
    // Requirement 11.5 lead-in: surface the run outcome before the play-again
    // prompt.
    if (model_.runState() == RunState::Won) {
        out_ << "You escaped the vault -- victory!\n";
        if (model_.hasScore()) {
            out_ << "Final score: " << model_.finalScore() << "\n";
        }
    } else if (model_.isRunLost()) {
        out_ << "Your Focus Points are spent -- you have fallen.\n";
    }
}

bool Game::playAgainPrompt() {
    // Requirement 11.5: offer to play again. `y`/`yes` starts a fresh run; any
    // other response (including `n` and end-of-input) exits.
    out_ << "Play again? (y/n): ";
    std::string token;
    if (!(in_ >> token)) {
        return false;
    }
    return token == "y" || token == "Y" || token == "yes";
}

void Game::redraw() {
    const Level *level = model_.level();
    if (level == nullptr) {
        return;
    }
    out_ << view_.renderFrame(*level, buildOverlay(), buildStatus());

    // Per-turn NOTIFICATION LOG (feature): after the frame + status, print the
    // messages the just-processed turn's logic pushed, then CLEAR the log so the
    // next turn starts empty. DECISION: the "Notifications:" header is ALWAYS
    // printed (even when the list is empty) for output consistency, so tests can
    // reliably key off it. The Game owns this print+clear lifecycle (see
    // notification_log.h); no other component prints or clears the log.
    //
    // The trailing blank line (below) stays AFTER the notifications so each
    // redraw remains a single frame terminated by a blank-line separator (the
    // board + status + notifications are all one frame).
    // Print the turn's messages. displayMessages() leaves every line as-is
    // EXCEPT the "you have fallen" loss line, which is collapsed to a single
    // occurrence (many Spectres can each announce the lethal hit).
    NotificationLog &log = NotificationLog::instance();
    // Project spec (WITH/WITHOUT bonus, no recompile): the Notifications board
    // is a BONUS feature that adds lines beyond the strict 79x30 frame, so it
    // is printed ONLY when bonus is enabled. When disabled we print NEITHER the
    // header NOR any messages — but we STILL clear the log (below) so it never
    // accumulates across turns, and the trailing blank-line frame separator is
    // still emitted unconditionally so frame-splitting stays consistent.
    if (bonusEnabled_) {
        out_ << "Notifications:\n";
        for (const auto &m : log.displayMessages()) {
            out_ << m << "\n";
        }
    }
    log.clear();

    // Blank-line frame separator (renderFrame already newline-terminates its
    // last row; this closes the frame after the notifications section).
    out_ << "\n";
}

RenderOverlay Game::buildOverlay() const {
    RenderOverlay overlay;
    const Level *level = model_.level();
    if (level == nullptr) {
        return overlay;
    }

    // Items first so an actor sharing a tile draws over them (RenderOverlay:
    // later entry wins). Fragments '*', scrolls '?', coffer '$', gem 'C',
    // Aegis 'G' — each item supplies its own DD-7 glyph.
    for (const auto &item : level->items()) {
        if (item != nullptr) {
            overlay.add(item->position(), item->symbol());
        }
    }

    // Living Spectres next (dead ones are corpses awaiting removal).
    for (const auto &spectre : level->spectres()) {
        if (spectre != nullptr && !spectre->isDead()) {
            overlay.add(spectre->position(), spectre->symbol());
        }
    }

    // The Arcanist '@' draws on top of everything.
    overlay.add(model_.arcanistPosition(), glyphs::kArcanist);
    return overlay;
}

StatusInfo Game::buildStatus() const {
    StatusInfo status;
    const std::shared_ptr<Arcanist> &arcanist = model_.arcanist();
    if (arcanist != nullptr) {
        status.fp = arcanist->fp();
        status.maxFp = arcanist->maxFp();
        status.pwr = model_.effectivePwr();
        status.wr = model_.effectiveWr();
        if (arcanist->hasStrategy()) {
            status.className = arcanist->strategy().className();
        }
    }
    status.runeFragmentTotal = model_.runeFragmentTotal();
    // Current vault level and the command the player just entered, both shown
    // on the Rune Fragments row of the status area.
    status.levelNumber = model_.levelNumber();
    status.lastAction = lastAction_;
    return status;
}

int Game::run() {
    // Outer session loop (design "Main loop"). rng is seeded by the caller
    // before run() (main seeds from time; task 18 adds CLI seed parsing).
    while (true) {
        const char classKey = promptClassSelection();
        if (classKey == 'q') {
            break; // Requirement 1.6: quit without starting a run.
        }

        startNewRun(classKey);

        const SessionAction action = playLoop();
        if (action == SessionAction::Quit) {
            break; // Requirement 11.3
        }
        if (action == SessionAction::Restart) {
            // Requirement 11.2: `r` RESETS the run to a clean slate here, at the
            // point the restart command is handled — clearing Arcanist stats,
            // active scroll effects, the Rune Fragment total, and the global
            // lichAggroTriggered flag (see resetRun). We then loop back to
            // promptClassSelection() to REQUIRE a new class selection before the
            // next startNewRun (which resetRun()s again, harmlessly/idempotently)
            // begins play. Nothing is drawn with stale state in between.
            resetRun();
            continue; // back to a new class selection (a brand-new game).
        }

        // RunEnded (win or loss): Requirement 11.5 play-again prompt.
        reportOutcome();
        if (!playAgainPrompt()) {
            break;
        }
    }
    return 0;
}

} // namespace runevault
