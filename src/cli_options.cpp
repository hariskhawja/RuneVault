// cli_options.cpp
// RuneVault — command-line argument parsing (task 18.1, Requirement 12).
//
// See cli_options.h for the documented argument convention. `parseArgs` is a
// pure function: it inspects the tokens and fills a CliOptions value, without
// performing any I/O or seeding.

#include "runevault/cli_options.h"

#include <stdexcept> // std::invalid_argument, std::out_of_range
#include <string>

namespace runevault {

namespace {

// Strictly parse `token` as an unsigned long. Returns true and writes `out`
// only when the ENTIRE token is a non-empty run of decimal digits with no
// trailing junk (so "42x" and "" are rejected). std::stoul would happily stop
// at the first non-digit, so we also verify it consumed every character.
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

    // Whether an EXPLICIT `-seed`/`--seed` flag claimed the seed slot. When
    // set, a positional second token never overrides it (the flag wins — see
    // the precedence note in cli_options.h). It is set even when the explicit
    // seed value was invalid, so the flag still owns the seed slot.
    bool explicitSeedSet = false;

    // The positional (second bare token) seed candidate, deferred until after
    // the full pass so we know whether an explicit `-seed` flag was present.
    bool hasPositionalSeed = false;
    std::string positionalSeed;

    // Count of BARE (non-flag) tokens seen so far. The 1st is the layout file,
    // the 2nd is the positional seed; flag-looking tokens ('-'-prefixed,
    // including `-bonus`) are skipped and never counted here.
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
                // Non-numeric seed (Requirement 12.5): error + fall back later.
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

        // Any other flag-looking token (starts with '-') is not recognized;
        // ignore it so future options can be added without breaking. Skipping
        // it here also keeps it OUT of the bare-token count (below).
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

    // Apply the positional seed ONLY when no explicit `-seed` flag set the seed
    // (flag-wins precedence). A non-numeric positional second token is a seed
    // error, mirroring the explicit-flag behaviour (Requirement 12.5).
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

// Requirement 12.5: IF a provided RNG seed argument is non-numeric, THEN report
// the error and seed the generator from the system time instead. This helper
// only handles the REPORTING half (message to `err`); the caller performs the
// time fallback via rng::seed_from_time(). No message is written when there was
// no seed error, keeping normal runs silent.
bool reportSeedError(const CliOptions &opts, std::ostream &err) {
    if (!opts.seedError) {
        return false;
    }
    err << "runevault: invalid seed argument (non-numeric); seeding from "
           "system time instead\n";
    return true;
}

} // namespace runevault
