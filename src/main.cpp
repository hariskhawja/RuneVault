#include "runevault/cli_options.h"
#include "runevault/game.h"
#include "runevault/layout_loader.h"
#include "shuffle.h"

#include <iostream>

int main(int argc, char **argv) {
    runevault::CliOptions opts = runevault::parseArgs(argc, argv);

    if (opts.hasSeed) {
        rng::seed(opts.seed);
    } else {
        runevault::reportSeedError(opts, std::cerr);
        rng::seed_from_time();
    }

    runevault::Game game; // defaults to std::cin / std::cout

    game.setBonusEnabled(opts.bonusEnabled);

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
