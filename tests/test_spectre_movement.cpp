// test_spectre_movement.cpp
// RuneVault — Spectre movement with chamber containment (task 10.3,
// Requirements 7.1/7.2/7.3/7.4/7.10) via Model::advanceSpectres.
//
// Coverage:
//   * A mobile Spectre (Wraith) wanders only to floor tiles inside its spawn
//     chamber (Requirements 7.3/7.10) and each step is within its 1-block
//     radius of the previous position (Requirement 7.2).
//   * A Vault Anchor never moves (Requirement 7.4).
//   * A Spectre whose in-chamber floor neighbours are all occupied stays put
//     (candidates empty).
//   * A Spectre never lands on the Arcanist's tile.
//   * One advanceSpectres call moves each Spectre at most one cell (Chebyshev
//     distance <= 1), i.e. "each moved at most once" (Requirement 7.1).
//
// Uses the tiny assert-based harness (test_util.h); no main() here —
// tests/test_main.cpp drives run_all(). rng is seeded for determinism.

#include "test_util.h"

#include "runevault/board.h"
#include "runevault/cell.h"
#include "runevault/geometry.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spectre.h"
#include "shuffle.h"

#include <memory>

using namespace runevault;

namespace {

// Chebyshev distance between two positions: the number of "king moves" apart.
// A single 8-direction step has Chebyshev distance exactly 1; staying put is 0.
int chebyshev(const Position &a, const Position &b) {
    const int dr = a.row > b.row ? a.row - b.row : b.row - a.row;
    const int dc = a.col > b.col ? a.col - b.col : b.col - a.col;
    return dr > dc ? dr : dc;
}

// Build a Model owning a default-layout Level with the Arcanist tucked far away
// (chamber 5) so it does not interfere unless a test moves it.
Model makeModel() {
    Model m;
    m.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    // Chamber 5 interior starts at (row 17, col 20); keep the Arcanist there.
    m.setArcanistPosition(Position{17, 20});
    return m;
}

// True iff `p` is a floor tile belonging to chamber `chamberId`.
bool isChamberFloor(const Board &board, const Position &p, int chamberId) {
    if (!board.inBounds(p)) {
        return false;
    }
    const Cell &c = board.at(p);
    return c.type == CellType::Floor && c.chamberId == chamberId;
}

} // namespace

// --- Wandering stays inside the spawn chamber, one step at a time -----------

RV_TEST(spectre_wanders_within_chamber_floor_and_one_block_radius) {
    rng::seed(4242);
    Model m = makeModel();
    const Board &board = m.level()->board();

    // A Wraith placed in the interior of chamber 1 (rows 1..4, cols 1..9).
    const Position start{2, 5};
    m.level()->addSpectre(std::unique_ptr<Spectre>(new Wraith(/*chamber*/ 1, start)));
    Spectre *w = m.level()->spectreAt(start);
    RV_CHECK(w != nullptr);

    Position prev = w->position();
    RV_CHECK(isChamberFloor(board, prev, 1));

    // Over many actions the Spectre must always remain on a chamber-1 floor
    // tile (Requirements 7.3/7.10) and never jump more than one cell
    // (Requirement 7.2 — moves are drawn from the 1-block radius).
    for (int i = 0; i < 200; ++i) {
        m.advanceSpectres();
        const Position now = w->position();
        RV_CHECK(isChamberFloor(board, now, 1)); // never leaves chamber 1
        RV_CHECK(chebyshev(prev, now) <= 1);      // at most one cell per action
        prev = now;
    }
}

// --- Vault Anchor is stationary (Requirement 7.4) ---------------------------

RV_TEST(vault_anchor_never_moves) {
    rng::seed(7);
    Model m = makeModel();

    const Position anchorAt{3, 4};
    m.level()->addSpectre(
        std::unique_ptr<Spectre>(new VaultAnchor(/*chamber*/ 1, anchorAt)));
    Spectre *a = m.level()->spectreAt(anchorAt);
    RV_CHECK(a != nullptr);
    RV_CHECK(a->isMobile() == false);

    for (int i = 0; i < 100; ++i) {
        m.advanceSpectres();
        RV_CHECK(a->position() == anchorAt); // stays put every action
    }
}

// --- Boxed-in Spectre with no free neighbour stays put ----------------------

RV_TEST(spectre_with_all_neighbours_occupied_stays_put) {
    rng::seed(101);
    Model m = makeModel();

    // Place the mover at a chamber-1 interior tile and surround its 8 neighbours
    // with stationary Vault Anchors so every in-chamber floor neighbour is
    // occupied. With no free candidate the mover cannot move.
    const Position center{2, 5};
    m.level()->addSpectre(std::unique_ptr<Spectre>(new Wraith(/*chamber*/ 1, center)));

    for (Direction d : {Direction::No, Direction::So, Direction::Ea, Direction::We,
                        Direction::Ne, Direction::Nw, Direction::Se, Direction::Sw}) {
        const Position n = Geometry::step(center, d);
        m.level()->addSpectre(
            std::unique_ptr<Spectre>(new VaultAnchor(/*chamber*/ 1, n)));
    }

    Spectre *mover = m.level()->spectreAt(center);
    RV_CHECK(mover != nullptr);

    for (int i = 0; i < 50; ++i) {
        m.advanceSpectres();
        RV_CHECK(mover->position() == center); // boxed in — never moves
    }
}

// --- Spectre never steps onto the Arcanist's tile ---------------------------

RV_TEST(spectre_never_moves_onto_arcanist) {
    rng::seed(31337);
    Model m = makeModel();

    // Put the Arcanist inside chamber 1 adjacent to the Spectre.
    const Position arcanist{2, 5};
    m.setArcanistPosition(arcanist);

    const Position start{2, 4}; // west of the Arcanist, chamber-1 floor
    m.level()->addSpectre(std::unique_ptr<Spectre>(new Wraith(/*chamber*/ 1, start)));
    Spectre *w = m.level()->spectreAt(start);
    RV_CHECK(w != nullptr);

    for (int i = 0; i < 300; ++i) {
        m.advanceSpectres();
        RV_CHECK(w->position() != arcanist); // never lands on the Arcanist
    }
}

// --- Each Spectre moves at most one cell per action (row-order pass) --------

RV_TEST(each_spectre_moves_at_most_one_cell_per_action) {
    rng::seed(2024);
    Model m = makeModel();

    // Two movers in chamber 1, spaced apart so neither boxes the other.
    const Position aStart{1, 2};
    const Position bStart{4, 8};
    m.level()->addSpectre(std::unique_ptr<Spectre>(new Wraith(/*chamber*/ 1, aStart)));
    m.level()->addSpectre(std::unique_ptr<Spectre>(new Shade(/*chamber*/ 1, bStart)));

    Spectre *a = m.level()->spectreAt(aStart);
    Spectre *b = m.level()->spectreAt(bStart);
    RV_CHECK(a != nullptr);
    RV_CHECK(b != nullptr);

    Position aPrev = a->position();
    Position bPrev = b->position();
    for (int i = 0; i < 100; ++i) {
        m.advanceSpectres();
        // A single pass moves each Spectre at most once => Chebyshev step <= 1.
        RV_CHECK(chebyshev(aPrev, a->position()) <= 1);
        RV_CHECK(chebyshev(bPrev, b->position()) <= 1);
        // And both remain contained in chamber 1.
        RV_CHECK(a->position() != b->position()); // never share a tile
        aPrev = a->position();
        bPrev = b->position();
    }
}

// --- No-op safety: advanceSpectres with no level does nothing ---------------

RV_TEST(advance_spectres_no_level_is_noop) {
    Model m; // no level set
    m.advanceSpectres();
    RV_CHECK(m.level() == nullptr);
}
