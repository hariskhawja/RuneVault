// test_cli_options.cpp
// RuneVault — unit tests for command-line argument parsing (task 18.1,
// Requirement 12). parseArgs is pure, so each test builds a mutable argv array
// (char*[] matching `int argc, char** argv`) and asserts on the CliOptions.

#include "test_util.h"

#include "runevault/cli_options.h"

#include <cstring>
#include <sstream>
#include <vector>

using namespace runevault;

namespace {

// Build a CliOptions from a literal argv list. The first element is treated as
// the program name (argv[0]) to mirror a real invocation. Uses mutable buffers
// because parseArgs takes `char**`.
CliOptions parseFrom(const std::vector<const char *> &args) {
    std::vector<std::vector<char>> storage;
    storage.reserve(args.size());
    for (const char *a : args) {
        storage.emplace_back(a, a + std::strlen(a) + 1); // include the NUL
    }
    std::vector<char *> argv;
    argv.reserve(storage.size());
    for (auto &buf : storage) {
        argv.push_back(buf.data());
    }
    return parseArgs(static_cast<int>(argv.size()), argv.data());
}

} // namespace

RV_TEST(cli_no_args_defaults) {
    CliOptions o = parseFrom({"runevault"});
    RV_CHECK(!o.hasSeed);
    RV_CHECK(!o.hasLayoutFile);
    RV_CHECK(!o.seedError);
}

RV_TEST(cli_seed_flag_numeric) {
    CliOptions o = parseFrom({"runevault", "-seed", "42"});
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 42UL);
    RV_CHECK(!o.seedError);
    RV_CHECK(!o.hasLayoutFile);
}

RV_TEST(cli_long_seed_flag_numeric) {
    CliOptions o = parseFrom({"runevault", "--seed", "7"});
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 7UL);
    RV_CHECK(!o.seedError);
}

RV_TEST(cli_seed_non_numeric_is_error) {
    CliOptions o = parseFrom({"runevault", "-seed", "abc"});
    RV_CHECK(!o.hasSeed);
    RV_CHECK(o.seedError);
}

RV_TEST(cli_seed_missing_value_is_error) {
    CliOptions o = parseFrom({"runevault", "-seed"});
    RV_CHECK(!o.hasSeed);
    RV_CHECK(o.seedError);
}

RV_TEST(cli_seed_trailing_junk_rejected) {
    CliOptions o = parseFrom({"runevault", "-seed", "42x"});
    RV_CHECK(!o.hasSeed);
    RV_CHECK(o.seedError);
}

RV_TEST(cli_layout_file_positional) {
    CliOptions o = parseFrom({"runevault", "layout.txt"});
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "layout.txt");
    RV_CHECK(!o.hasSeed);
    RV_CHECK(!o.seedError);
}

RV_TEST(cli_layout_then_seed) {
    CliOptions o = parseFrom({"runevault", "layout.txt", "-seed", "7"});
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "layout.txt");
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 7UL);
    RV_CHECK(!o.seedError);
}

RV_TEST(cli_seed_then_layout) {
    CliOptions o = parseFrom({"runevault", "-seed", "7", "layout.txt"});
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "layout.txt");
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 7UL);
    RV_CHECK(!o.seedError);
}

// Spec CLI form `runevault [layoutfile] [seed]`: the FIRST bare token is the
// layout file and the SECOND (numeric) bare token is the positional seed.
RV_TEST(cli_first_bare_token_wins_as_layout) {
    CliOptions o = parseFrom({"runevault", "first.txt", "42"});
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "first.txt");
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 42UL);
    RV_CHECK(!o.seedError);
}

// --- Positional seed (spec CLI form) -----------------------------------------
RV_TEST(cli_positional_seed_after_layout) {
    CliOptions o = parseFrom({"runevault", "board.txt", "42"});
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "board.txt");
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 42UL);
    RV_CHECK(!o.seedError);
}

// A non-numeric SECOND positional token is a seed error (the spec seed is a
// number), Requirement 12.5.
RV_TEST(cli_positional_seed_non_numeric_is_error) {
    CliOptions o = parseFrom({"runevault", "board.txt", "notanumber"});
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "board.txt");
    RV_CHECK(!o.hasSeed);
    RV_CHECK(o.seedError);
}

// PRECEDENCE: an explicit `-seed` flag WINS over a positional second token.
// `board.txt 99 -seed 7` -> layout board.txt, seed 7 (not 99).
RV_TEST(cli_explicit_seed_flag_wins_over_positional) {
    CliOptions o = parseFrom({"runevault", "board.txt", "99", "-seed", "7"});
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "board.txt");
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 7UL); // flag wins, positional 99 ignored
    RV_CHECK(!o.seedError);
}

// The same precedence holds when the flag appears BEFORE the positional token.
RV_TEST(cli_explicit_seed_flag_wins_over_positional_flag_first) {
    CliOptions o = parseFrom({"runevault", "-seed", "7", "board.txt", "99"});
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "board.txt");
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 7UL);
    RV_CHECK(!o.seedError);
}

// --- `-bonus`/`--bonus` runtime toggle ---------------------------------------
RV_TEST(cli_bonus_flag_sets_enabled) {
    CliOptions o = parseFrom({"runevault", "-bonus"});
    RV_CHECK(o.bonusEnabled);
}

RV_TEST(cli_long_bonus_flag_sets_enabled) {
    CliOptions o = parseFrom({"runevault", "--bonus"});
    RV_CHECK(o.bonusEnabled);
}

RV_TEST(cli_bonus_absent_defaults_false) {
    CliOptions o = parseFrom({"runevault", "board.txt", "42"});
    RV_CHECK(!o.bonusEnabled);
}

// `-bonus` is a FLAG, not a bare/positional token: it must not be mistaken for
// the layout file or the seed, and it is order-independent.
RV_TEST(cli_bonus_flag_not_treated_as_positional) {
    CliOptions o = parseFrom({"runevault", "-bonus", "board.txt", "42"});
    RV_CHECK(o.bonusEnabled);
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "board.txt"); // -bonus did not become the layout
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 42UL);              // -bonus did not shift the seed slot
    RV_CHECK(!o.seedError);
}

// Full spec-style invocation with everything present.
RV_TEST(cli_layout_positional_seed_and_bonus) {
    CliOptions o = parseFrom({"runevault", "board.txt", "42", "-bonus"});
    RV_CHECK(o.hasLayoutFile);
    RV_CHECK(o.layoutFile == "board.txt");
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 42UL);
    RV_CHECK(!o.seedError);
    RV_CHECK(o.bonusEnabled);
}

RV_TEST(cli_seed_zero_is_valid) {
    CliOptions o = parseFrom({"runevault", "-seed", "0"});
    RV_CHECK(o.hasSeed);
    RV_CHECK(o.seed == 0UL);
    RV_CHECK(!o.seedError);
}

// Requirement 12.5: reportSeedError writes a message and returns true when the
// seed argument was invalid.
RV_TEST(cli_report_seed_error_writes_message) {
    CliOptions o;
    o.seedError = true;
    std::ostringstream err;
    bool reported = reportSeedError(o, err);
    RV_CHECK(reported);
    RV_CHECK(!err.str().empty());
}

// Requirement 12.5: reportSeedError stays silent (no message, returns false)
// when there was no seed error, so normal runs print nothing.
RV_TEST(cli_report_seed_error_silent_when_no_error) {
    CliOptions o;
    o.seedError = false;
    std::ostringstream err;
    bool reported = reportSeedError(o, err);
    RV_CHECK(!reported);
    RV_CHECK(err.str().empty());
}

// End-to-end: a non-numeric seed parses to seedError and reportSeedError then
// emits the user-facing message (Requirement 12.5).
RV_TEST(cli_report_seed_error_end_to_end_non_numeric) {
    CliOptions o = parseFrom({"runevault", "-seed", "abc"});
    RV_CHECK(o.seedError);
    std::ostringstream err;
    RV_CHECK(reportSeedError(o, err));
    RV_CHECK(!err.str().empty());
}

// End-to-end: a valid numeric seed is not an error, so reportSeedError prints
// nothing (Requirement 12.5 fallback only triggers on bad input).
RV_TEST(cli_report_seed_error_end_to_end_numeric_silent) {
    CliOptions o = parseFrom({"runevault", "-seed", "42"});
    RV_CHECK(!o.seedError);
    std::ostringstream err;
    RV_CHECK(!reportSeedError(o, err));
    RV_CHECK(err.str().empty());
}
