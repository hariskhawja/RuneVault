// test_integration.cpp
// RuneVault — end-to-end integration wiring (task 19.1, Requirements 11.1 /
// 11.5 / 12.1).
//
// These tests exercise the FULLY WIRED game: the Game controller driving the
// Model + View + Spawner over scripted std::istringstream input, and the
// optional layout-FILE path (LayoutLoader -> Game::useLayout ->
// Model::buildCurrentLevel -> Spawner::populateActorsOnly) that makes a run use
// a file-defined layout instead of random generation.
//
// The Game reads commands from a std::istream and renders to a std::ostream, so
// a full session (class selection -> play -> win/loss -> play again) is driven
// with scripted input and captured output — no TTY. rng is seeded first because
// level construction consumes randomness (Requirement 4.11).
//
// Two deterministic seams keep these robust rather than brittle:
//   * A tiny 3x3 layout whose only free floor tile is the centre forces the
//     Arcanist to spawn there, surrounded by 8 Drain Focus scrolls. A Hexblade
//     (100 FP, doubled -20 per Drain) then loses in exactly 5 scroll uses — a
//     deterministic run END with no dependence on spectre RNG — so the
//     play-again prompt (Requirement 11.5) is reached reliably.
//   * A 2-chamber layout with a scroll and a Shard Coffer proves a file drives
//     the level (Requirement 12.1): the file's items appear on the built level
//     and the actors-only Spawner pass places the Arcanist, spectres, and the
//     coffer's guarding Vault Anchor on top.

#include "test_util.h"

#include "runevault/arcanist_factory.h"
#include "runevault/game.h"
#include "runevault/guarded_item.h"
#include "runevault/item.h"
#include "runevault/layout_loader.h"
#include "runevault/model.h"
#include "runevault/spectre.h"
#include "shuffle.h" // rng::seed

#include <sstream>
#include <string>

namespace {

using namespace runevault;

bool contains(const std::string &haystack, const std::string &needle) {
    return haystack.find(needle) != std::string::npos;
}

int countOccurrences(const std::string &haystack, const std::string &needle) {
    if (needle.empty()) {
        return 0;
    }
    int count = 0;
    std::string::size_type pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

const char *kPromptToken = "Select your Arcanist class";

// A 3x3 all-floor chamber whose CENTRE is the only free tile; the 8 border
// tiles are Drain Focus scrolls (digit '3'). Per Q15 Follow-up-2 the file is
// ONE grid applied to all five levels, so this single grid IS the layout (these
// tests end on level 1).
const char *kDrainLayout =
    "333\n"
    "3.3\n"
    "333";

// Two floor chambers (separated by a void column): the left carries a Surge
// Power scroll ('1'), the right a Shard Coffer ('9'). Big enough to host the
// Arcanist, stairway (different chamber), the coffer's guard, and spectres. One
// grid, applied to all five levels (Q15 Follow-up-2).
const char *kTwoChamberLayout =
    "..... .....\n"
    "..1.. ..9..\n"
    "..... .....";

} // namespace

// ============================================================================
// Requirement 11.1 — a full default run executes end to end (smoke test).
// Class selection -> several dispatched+redrawn turns -> clean quit.
// ============================================================================
RV_TEST(integration_default_run_plays_then_quits_cleanly) {
    rng::seed(31337);
    // Sage, a few movement commands (each dispatched + redrawn), then quit.
    std::istringstream in("s\nno\nea\nso\nwe\nq\n");
    std::ostringstream out;
    Game game(in, out);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    RV_CHECK(contains(text, kPromptToken)); // class selection shown (Req 1.1)
    RV_CHECK(contains(text, "Sage"));       // the run started
    RV_CHECK(contains(text, "@"));          // the board was rendered each turn
}

// ============================================================================
// Requirement 11.5 — a run that ends offers a play-again prompt; `n` exits.
// The Hexblade drains itself to 0 FP with 5 Drain Focus scrolls (deterministic
// loss), then answers `n` at the play-again prompt to exit.
// ============================================================================
RV_TEST(integration_run_end_offers_play_again_then_n_exits) {
    rng::seed(4242);
    const LoadedLayout loaded = LayoutLoader::loadFromString(kDrainLayout);
    RV_CHECK(loaded.ok);
    // Hexblade, drain FP to 0 via 5 adjacent Drain Focus scrolls, then decline.
    std::istringstream in("h\nu no\nu so\nu ea\nu we\nu ne\nn\n");
    std::ostringstream out;
    Game game(in, out);
    game.useLayout(loaded);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    RV_CHECK(contains(text, "Hexblade"));     // the run started
    RV_CHECK(contains(text, "you have fallen")); // loss reported (Req 11.5 lead-in)
    RV_CHECK(contains(text, "Play again"));    // play-again offered (Req 11.5)
    // Declining exits without a second class-selection prompt cycle.
    RV_CHECK(countOccurrences(text, kPromptToken) == 1);
}

// ============================================================================
// Requirement 11.5 / 11.2 — answering `y` at play-again loops to a NEW class
// selection. The same deterministic loss, then `y` to play again, then `q` to
// quit at the fresh class-selection prompt.
// ============================================================================
RV_TEST(integration_play_again_y_loops_to_class_selection) {
    rng::seed(4242);
    const LoadedLayout loaded = LayoutLoader::loadFromString(kDrainLayout);
    RV_CHECK(loaded.ok);
    std::istringstream in("h\nu no\nu so\nu ea\nu we\nu ne\ny\nq\n");
    std::ostringstream out;
    Game game(in, out);
    game.useLayout(loaded);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    RV_CHECK(contains(text, "Play again"));
    // `y` starts a fresh run: the class-selection prompt is shown AGAIN before
    // the final `q` quits (initial + post-play-again == at least 2).
    RV_CHECK(countOccurrences(text, kPromptToken) >= 2);
}

// ============================================================================
// Requirement 12.1 — a layout FILE drives the level. Building the current level
// from a parsed layout (the exact path Game::startNewRun uses) must place the
// file's items on the level, and the actors-only Spawner pass must add the
// Arcanist, spectres, and the coffer's guarding Vault Anchor without adding
// random scrolls/fragments.
// ============================================================================
RV_TEST(integration_layout_file_drives_level_items_and_actors) {
    rng::seed(777);
    const LoadedLayout loaded = LayoutLoader::loadFromString(kTwoChamberLayout);
    RV_CHECK(loaded.ok);

    Model model;
    model.setArcanist(ArcanistFactory::create('s'));
    model.setLevelNumber(1);
    model.setAegisLevel(0); // no Aegis on level 1
    model.setLayout(&loaded);
    model.buildCurrentLevel();

    const Level *level = model.level();
    RV_CHECK(level != nullptr);
    if (level == nullptr) {
        return;
    }

    // The file's Surge Power scroll appears at its digit tile (1,2) (Req 12.2).
    Item *scrollItem = level->itemAt(Position{1, 2});
    Scroll *scroll = dynamic_cast<Scroll *>(scrollItem);
    RV_CHECK(scroll != nullptr);
    if (scroll != nullptr) {
        RV_CHECK(scroll->scrollType() == ScrollType::SurgePower);
    }

    // The file's Shard Coffer (a GuardedItem) appears at its '9' tile (1,8).
    Item *cofferItem = level->itemAt(Position{1, 8});
    RV_CHECK(dynamic_cast<GuardedItem *>(cofferItem) != nullptr);

    // The actors-only pass placed the Arcanist on a hostable floor tile.
    const Position arcanist = model.arcanistPosition();
    RV_CHECK(level->board().inBounds(arcanist));
    RV_CHECK(level->board().at(arcanist).canHost());

    // Spectres were placed, including a Vault Anchor guarding the coffer plus at
    // least one ordinary (non-Anchor) spectre eligible to carry the Cipher Gem.
    RV_CHECK(!level->spectres().empty());
    bool hasAnchor = false;
    bool hasNonAnchor = false;
    for (const std::unique_ptr<Spectre> &s : level->spectres()) {
        if (s == nullptr) {
            continue;
        }
        if (s->type() == SpectreType::VaultAnchor) {
            hasAnchor = true;
        } else {
            hasNonAnchor = true;
        }
    }
    RV_CHECK(hasAnchor);    // the coffer's guarding Vault Anchor (Req 4.6)
    RV_CHECK(hasNonAnchor); // ordinary spectres (Req 4.7)
}

// ============================================================================
// Requirement 12.1 (integration) — a layout file wired into the Game via
// useLayout() runs end to end: a run starts, renders a board, and quits cleanly.
// ============================================================================
RV_TEST(integration_layout_file_wired_into_game_runs) {
    rng::seed(2024);
    const LoadedLayout loaded = LayoutLoader::loadFromString(kTwoChamberLayout);
    RV_CHECK(loaded.ok);

    std::istringstream in("s\nq\n");
    std::ostringstream out;
    Game game(in, out);
    game.useLayout(loaded);

    const int code = game.run();
    const std::string text = out.str();

    RV_CHECK(code == 0);
    RV_CHECK(contains(text, "Sage")); // the run started under the file layout
    RV_CHECK(contains(text, "@"));    // the file-driven board was rendered
}
