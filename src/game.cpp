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

bool isDirectionToken(const std::string &token) {
    return token == "no" || token == "so" || token == "ea" || token == "we" ||
           token == "ne" || token == "nw" || token == "se" || token == "sw";
}

} // namespace

Game::Game(std::istream &in, std::ostream &out)
    : in_(in), out_(out), model_(), view_(/*ansiColour=*/false) {}

void Game::useLayout(const LoadedLayout &layout) {
    if (!layout.ok) {
        return;
    }
    layout_ = layout;
    hasLayout_ = true;
}

char Game::promptClassSelection() {
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
            return 'q';
        }

        if (token.size() == 1) {
            const char key = token[0];
            if (key == 'q') {
                return 'q';
            }
            if (ArcanistFactory::create(key) != nullptr) {
                return key;
            }
        }
    }
}

void Game::resetRun() {
    model_ = Model();
}

void Game::startNewRun(char classKey) {
    resetRun();

    // No action has been taken yet on a fresh run, so the status area's
    // "Action:" field starts empty (rendered as "-").
    lastAction_.clear();

    model_.setBonusEnabled(bonusEnabled_);

    // Feature: a new/restarted run starts with a clean notification log so no
    // messages from a prior run's final turn leak into the first frame.
    NotificationLog::instance().clear();

    model_.setArcanist(ArcanistFactory::create(classKey));

    model_.setLevelNumber(1);
    model_.setAegisLevel(Spawner::chooseAegisLevel());

    model_.setLayout(hasLayout_ ? &layout_ : nullptr);

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
            return SessionAction::Quit;
        }
        if (cmd == "r") {
            return SessionAction::Restart;
        }

        lastAction_ = cmd;

        // Snapshot the level number so we can tell whether the player's move
        // triggered a descent (moveArcanist -> tryDescend rebuilds the level).
        const int levelBefore = model_.levelNumber();

        const bool turnConsumed = dispatchCommand(cmd);

        if (turnConsumed && model_.runState() == RunState::Playing &&
            model_.levelNumber() == levelBefore) {
            model_.advanceSpectres(); // Spectre action phase (before the redraw
                                      // so their new positions are shown).
        }

        redraw();
    }
    return SessionAction::RunEnded;
}

bool Game::dispatchCommand(const std::string &cmd) {
    if (isDirectionToken(cmd)) {
        return model_.moveArcanist(cmd);
    }
    if (cmd == "u") {
        std::string dir;
        if (in_ >> dir) {
            lastAction_ = cmd + " " + dir; // echo the full command (e.g. "u no")
            return model_.useScroll(dir);
        }
        return false; // dangling `u` at end of input: no-op, turn not consumed.
    }
    if (cmd == "a") {
        std::string dir;
        if (in_ >> dir) {
            lastAction_ = cmd + " " + dir; // echo the full command (e.g. "a ne")
            try {
                return model_.handleAttack(Geometry::parse(dir));
            } catch (...) {
                return false;
            }
        }
        return false; // dangling `a` at end of input: no-op, turn not consumed.
    }
    return false;
}

void Game::reportOutcome() {
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

    NotificationLog &log = NotificationLog::instance();
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
    while (true) {
        const char classKey = promptClassSelection();
        if (classKey == 'q') {
            break;
        }

        startNewRun(classKey);

        const SessionAction action = playLoop();
        if (action == SessionAction::Quit) {
            break;
        }
        if (action == SessionAction::Restart) {
            resetRun();
            continue; // back to a new class selection (a brand-new game).
        }

        reportOutcome();
        if (!playAgainPrompt()) {
            break;
        }
    }
    return 0;
}

} // namespace runevault
