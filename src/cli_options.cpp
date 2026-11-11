#include "runevault/cli_options.h"

#include <stdexcept>
#include <string>

namespace runevault {

namespace {

bool parseStrictUnsignedLong(const std::string &token, unsigned long &out) {
    if (token.empty()) {
        return false;
    }
    // Reject a leading sign or any non-digit up front; std::stoul otherwise
    // accepts leading '+'/'-' and whitespace, which we do not want for a seed.
    for (char c : token) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    try {
        std::size_t consumed = 0;
        unsigned long value = std::stoul(token, &consumed);
        if (consumed != token.size()) {
            return false; // trailing junk (defensive; the digit scan covers it)
        }
        out = value;
        return true;
    } catch (const std::invalid_argument &) {
        return false;
    } catch (const std::out_of_range &) {
        return false;
    }
}

// True when `token` is the seed flag in either accepted spelling.
bool isSeedFlag(const std::string &token) {
    return token == "-seed" || token == "--seed";
}

} // namespace

CliOptions parseArgs(int argc, char **argv) {
    CliOptions opts;

    bool explicitSeedSet = false;

    // The positional (second bare token) seed candidate, deferred until after
    // the full pass so we know whether an explicit `-seed` flag was present.
    bool hasPositionalSeed = false;
    std::string positionalSeed;

    int bareIndex = 0;

    // Start at 1 to skip argv[0] (the program name).
    for (int i = 1; i < argc; ++i) {
        std::string token = argv[i];

        if (isSeedFlag(token)) {
            // The seed flag must be followed by a value token.
            if (i + 1 >= argc) {
                // Missing value: treat as a seed error. The flag still claims
                // the seed slot so a positional token cannot override it.
                opts.hasSeed = false;
                opts.seedError = true;
                explicitSeedSet = true;
                break;
            }
            std::string value = argv[i + 1];
            ++i; // consume the value token regardless of validity
            unsigned long parsed = 0;
            if (parseStrictUnsignedLong(value, parsed)) {
                opts.hasSeed = true;
                opts.seed = parsed;
                opts.seedError = false;
            } else {
                opts.hasSeed = false;
                opts.seedError = true;
            }
            explicitSeedSet = true; // explicit flag wins over any positional
            continue;
        }

        // Runtime BONUS toggle (project spec: WITH/WITHOUT bonus, no recompile).
        // It is a FLAG, so it is NOT counted as a bare/positional token.
        if (token == "-bonus" || token == "--bonus") {
            opts.bonusEnabled = true;
            continue;
        }

        if (!token.empty() && token[0] == '-') {
            continue;
        }

        // A bare (non-flag) positional token. FIRST -> layout file, SECOND ->
        // positional seed (spec CLI form: `runevault [layoutfile] [seed]`).
        if (bareIndex == 0) {
            opts.hasLayoutFile = true;
            opts.layoutFile = token;
        } else if (bareIndex == 1) {
            // Defer parsing/applying until the pass ends: an explicit `-seed`
            // flag (anywhere in argv) takes precedence over this positional.
            hasPositionalSeed = true;
            positionalSeed = token;
        }
        // Extra bare tokens (3rd and beyond) are ignored (kept simple).
        ++bareIndex;
    }

    if (!explicitSeedSet && hasPositionalSeed) {
        unsigned long parsed = 0;
        if (parseStrictUnsignedLong(positionalSeed, parsed)) {
            opts.hasSeed = true;
            opts.seed = parsed;
            opts.seedError = false;
        } else {
            opts.hasSeed = false;
            opts.seedError = true;
        }
    }

    return opts;
}

bool reportSeedError(const CliOptions &opts, std::ostream &err) {
    if (!opts.seedError) {
        return false;
    }
    err << "runevault: invalid seed argument (non-numeric); seeding from "
           "system time instead\n";
    return true;
}

} // namespace runevault
