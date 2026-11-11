#ifndef RUNEVAULT_CLI_OPTIONS_H
#define RUNEVAULT_CLI_OPTIONS_H

#include <ostream>
#include <string>

namespace runevault {

// Parsed command-line options (see header notes for the argument convention).
struct CliOptions {
    bool hasLayoutFile = false;
    std::string layoutFile;

    bool hasSeed = false;
    unsigned long seed = 0;

    bool seedError = false;

    bool bonusEnabled = false;
};

// Parse (argc, argv) into CliOptions. PURE: no I/O, no seeding. argv[0] is the
// program name and is ignored. See header notes for the accepted forms.
// Usage: [layoutfile] [seed] [-seed n] [-bonus]. Explicit seed flags take priority.
CliOptions parseArgs(int argc, char **argv);

bool reportSeedError(const CliOptions &opts, std::ostream &err);

} // namespace runevault

#endif // RUNEVAULT_CLI_OPTIONS_H
