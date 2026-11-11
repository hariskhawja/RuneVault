#ifndef RUNEVAULT_GAME_H
#define RUNEVAULT_GAME_H

#include <iostream>
#include <string>

#include "runevault/layout_loader.h"
#include "runevault/model.h"
#include "runevault/view.h"

namespace runevault {

// Coordinates input, model updates, and rendering. Streams can be supplied by callers.
class Game {
public:
    explicit Game(std::istream &in = std::cin, std::ostream &out = std::cout);

    void useLayout(const LoadedLayout &layout);

    void setBonusEnabled(bool v) { bonusEnabled_ = v; }

    int run();

private:
    // Outcome of one play session, steering the outer run() loop.
    enum class SessionAction { Quit, Restart, RunEnded };

    char promptClassSelection();

    void resetRun();

    void startNewRun(char classKey);

    SessionAction playLoop();

    // False means no action occurred, so enemies do not receive a turn.
    bool dispatchCommand(const std::string &cmd);

    void reportOutcome();

    bool playAgainPrompt();

    void redraw();

    // Build the View overlay from the live Model: items, then living Spectres,
    // then the Arcanist `@` on top (later entries win on shared tiles).
    RenderOverlay buildOverlay() const;

    StatusInfo buildStatus() const;

    std::string lastAction_;

    std::istream &in_;
    std::ostream &out_;
    Model model_;
    View view_;

    bool hasLayout_ = false;
    LoadedLayout layout_; // Owned here; Model borrows its address for each run.

    bool bonusEnabled_ = false;
};

} // namespace runevault

#endif // RUNEVAULT_GAME_H
