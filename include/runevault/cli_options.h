// cli_options.h
// RuneVault — command-line argument parsing (task 18.1, Requirement 12).
//
// The program accepts an OPTIONAL level-layout file and an OPTIONAL RNG seed
// (Requirement 12: Command-Line Options). This module turns the raw
// (argc, argv) into a small, inspectable `CliOptions` value. `parseArgs` is a
// PURE function — it performs NO I/O and NO seeding — so it is trivially
// unit-testable (tests build argv arrays and assert on the result). The
// effects (seeding the rng, printing errors, loading the layout file) happen
// in main() based on the parsed options.
//
// ── Argument convention (DOCUMENTED) ─────────────────────────────────────────
// The spec CLI form (project spec) is:  ./runevault [layoutfile] [seed]
// i.e. the layout file is the FIRST positional (bare) token and the seed is an
// OPTIONAL SECOND positional token (a number). For backwards compatibility we
// also keep the explicit `-seed` flag and a runtime `-bonus` toggle:
//
//   * a bare token NOT beginning with '-' (1st) — the level-layout file path.
//   * a bare token NOT beginning with '-' (2nd) — the RNG seed. It must parse
//     strictly as an unsigned long (all digits, no trailing junk); a
//     non-numeric second positional token is a seed error (Requirement 12.5).
//   * `-seed <number>` (or `--seed <number>`) — the RNG seed, explicit form.
//     `<number>` must parse strictly as an unsigned long.
//   * `-bonus` (or `--bonus`) — enable the BONUS features at runtime (default
//     OFF). This is a FLAG (starts with '-') and is therefore NOT counted as a
//     bare/positional token. See `bonusEnabled` below.
//
// PRECEDENCE: if BOTH an explicit `-seed` flag AND a positional second token
// are supplied, the explicit `-seed` flag WINS (the positional seed is
// ignored). This holds regardless of token order and even when the explicit
// `-seed` value was itself invalid (the flag still claims the seed slot).
//
// Accepted invocations (order-independent):
//
//   runevault                        (no layout, seed from time)
//   runevault board.txt              (layout, seed from time)
//   runevault board.txt 42           (layout + positional seed)
//   runevault board.txt 42 -bonus    (layout + positional seed + bonus)
//   runevault -seed 42               (no layout, seeded via flag)
//   runevault -bonus board.txt       (bonus + layout; -bonus is not positional)
//   runevault board.txt -seed 7 -bonus (layout + explicit seed + bonus)
//   runevault mylayout.txt -seed 42  (layout + seed)
//   runevault -seed 42 mylayout.txt  (layout + seed)
//
// The FIRST bare token wins as the layout file; the SECOND bare token is the
// positional seed; any further bare tokens are ignored (kept simple).
//
// ── Seeding (Requirement 12.3 / 12.5) ────────────────────────────────────────
// Requirement 12.3: WHERE a seed argument is provided, seed the RNG with that
// value ONCE at startup so runs are reproducible. When absent, seed from the
// system time. Requirement 12.5: a non-numeric seed is an error — report it
// (the user-facing MESSAGE is refined in task 18.3) and seed from time instead.
// `parseArgs` records a bad seed by setting `seedError = true` (and leaving
// `hasSeed = false`); the caller falls back to `rng::seed_from_time()`.
//
// SCOPE NOTE: this task only sets up parsing + seeding. Actually loading and
// parsing the layout file contents is task 18.2 (LayoutLoader); refining the
// seed-error message text is task 18.3.

#ifndef RUNEVAULT_CLI_OPTIONS_H
#define RUNEVAULT_CLI_OPTIONS_H

#include <ostream>
#include <string>

namespace runevault {

// Parsed command-line options (see header notes for the argument convention).
struct CliOptions {
    // Layout file (a bare positional token, task 18.2 loads its contents).
    bool hasLayoutFile = false;
    std::string layoutFile;

    // RNG seed (`-seed`/`--seed <number>`), Requirement 12.3. `hasSeed` is true
    // only when a valid numeric seed was supplied.
    bool hasSeed = false;
    unsigned long seed = 0;

    // Set when a seed argument was present but its value was missing or not a
    // strict unsigned-long (Requirement 12.5) — whether supplied via the
    // explicit `-seed` flag OR the positional second token. The caller reports
    // the error and falls back to seed_from_time().
    bool seedError = false;

    // Enable the BONUS features at runtime (project spec: the game must support
    // play both WITH and WITHOUT the bonus features, with NO recompilation).
    // Set by the `-bonus`/`--bonus` flag; defaults to OFF so the base game is
    // spec-compliant. When true, main() wires it into the Game, which in turn
    // enables (1) the per-turn Notifications board (Game printing) and (2) the
    // Prestige promotion (Model::maybePromote). When false, neither appears.
    bool bonusEnabled = false;
};

// Parse (argc, argv) into CliOptions. PURE: no I/O, no seeding. argv[0] is the
// program name and is ignored. See header notes for the accepted forms.
CliOptions parseArgs(int argc, char **argv);

// Report a non-numeric / missing seed argument (Requirement 12.5). When
// `opts.seedError` is set, writes a clear, user-facing message to `err`
// explaining that the seed was invalid and that the generator will be seeded
// from the system time instead, and returns true. When `opts.seedError` is
// false, writes NOTHING and returns false.
//
// This helper deliberately does NOT touch the rng (no seeding side effect), so
// the message can be unit-tested in isolation with a std::ostringstream. main()
// pairs it with the documented time fallback: it calls reportSeedError and then
// rng::seed_from_time() whenever no valid seed is present.
bool reportSeedError(const CliOptions &opts, std::ostream &err);

} // namespace runevault

#endif // RUNEVAULT_CLI_OPTIONS_H
