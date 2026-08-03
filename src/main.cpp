// main.cpp
// RuneVault — program entry point.
//
// Constructs the MVC Controller (`Game`) and runs the session loop (task 17.1).
// The Game reads commands from std::cin and renders to std::cout by default.
//
// CLI parsing (task 18.1, Requirement 12): parse argv into CliOptions (an
// optional layout-file argument and an optional RNG seed), then seed the rng
// ONCE at startup before any randomness is consumed (Requirement 12.3):
//   * a valid `-seed <n>`      -> rng::seed(n)   (reproducible run)
//   * no seed / bad seed value -> rng::seed_from_time()  (non-reproducible)
// A non-numeric seed also sets seedError; task 18.3 refines the user-facing
// error message printed here before the time fallback. Loading the layout file
// contents is task 18.2 (LayoutLoader) — see the TODO below.
//
// BONUS toggle (project spec page 12: the game must support play both WITH and
// WITHOUT the bonus features, with NO recompilation). The `-bonus`/`--bonus`
// flag enables both non-spec additions at RUNTIME — the per-turn Notifications
// board and the Prestige promotion — by forwarding CliOptions::bonusEnabled to
// Game::setBonusEnabled. Absent the flag, the base game is spec-compliant (a
// strict 79x30 frame, no Notifications board, no promotion).

#include "runevault/cli_options.h"
#include "runevault/game.h"
#include "runevault/layout_loader.h"
#include "shuffle.h"

#include <iostream>

int main(int argc, char **argv) {
    runevault::CliOptions opts = runevault::parseArgs(argc, argv);

    // Seed the rng exactly ONCE at startup (Requirement 12.3). A valid seed
    // makes the run reproducible; otherwise (absent OR non-numeric) we fall
    // back to wall-clock time. Requirement 12.5: a non-numeric seed is an
    // error — report it (via reportSeedError) before seeding from system time.
    if (opts.hasSeed) {
        rng::seed(opts.seed);
    } else {
        runevault::reportSeedError(opts, std::cerr);
        rng::seed_from_time();
    }

    runevault::Game game; // defaults to std::cin / std::cout

    // Project spec (page 12): enable the bonus features (Notifications board +
    // Prestige promotion) at runtime per the `-bonus` flag — no recompile. Off
    // by default so the base game is spec-compliant.
    game.setBonusEnabled(opts.bonusEnabled);

    // Requirement 12.4: when a layout file is supplied, load it and REPORT a
    // missing / malformed file at startup (failing fast with a clear message).
    // Task 19.1 (Requirement 12.1): on a successful parse, hand the layout to
    // the Game so EVERY level of EVERY run is built from the file (board + digit
    // items + an actors-only Spawner pass) instead of the random default layout.
    if (opts.hasLayoutFile) {
        runevault::LoadedLayout layout =
            runevault::LayoutLoader::loadFromFile(opts.layoutFile);
        if (!layout.ok) {
            std::cerr << "runevault: " << layout.error << std::endl;
            return 1;
        }
        game.useLayout(layout);
    }

    return game.run();
}
