// test_notifications_model.cpp
// RuneVault — Model-level notification tests (feature: per-turn turn log).
//
// Exercises the notify() push sites wired into the Model: moves, fragment
// walk-over, attacks (hit + lethal defeat), Spectre attacks (hit/miss), and
// scroll use. The NotificationLog is a process-wide singleton, so EVERY test
// clears it at the START to avoid cross-test contamination. Uses the tiny
// assert-based harness (test_util.h); no main() here.

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/item.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/notification_log.h"
#include "runevault/scroll_type.h"
#include "runevault/spectre.h"
#include "shuffle.h" // rng::seed

#include <memory>
#include <string>

using namespace runevault;

namespace {

// Build a Model with an Arcanist at `pos` on a fresh default-layout Level.
std::unique_ptr<Model> makeModelAt(const Position &pos, int fp, int pwr,
                                   int wr) {
    auto model = std::unique_ptr<Model>(new Model());
    model->setArcanist(std::make_shared<Arcanist>(fp, pwr, wr));
    model->setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model->setArcanistPosition(pos);
    return model;
}

std::unique_ptr<GenericSpectre> makeSpectre(const Position &pos, int fp,
                                            int pwr, int wr) {
    return std::unique_ptr<GenericSpectre>(new GenericSpectre(
        SpectreType::Wraith, fp, pwr, wr, /*spawnChamber*/ 1, pos, 'W'));
}

// True if ANY buffered message contains `needle`.
bool anyMessageContains(const std::string &needle) {
    for (const auto &m : NotificationLog::instance().messages()) {
        if (m.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

// --- A successful move pushes "You move <dir>." ------------------------------

RV_TEST(notify_move_pushes_direction_message) {
    NotificationLog::instance().clear();

    // Arcanist at (2,3); (2,4) is an interior floor tile (see test_attack.cpp).
    auto model = makeModelAt(Position{2, 3}, /*fp*/ 50, /*pwr*/ 10, /*wr*/ 0);
    const bool moved = model->moveArcanist(Direction::Ea);

    RV_CHECK(moved == true);
    RV_CHECK(anyMessageContains("You move east"));
}

// --- A rejected move (into a wall / OOB) pushes NOTHING ----------------------

RV_TEST(notify_rejected_move_pushes_nothing) {
    NotificationLog::instance().clear();

    // Move far out of bounds via a wall: pick a position and a direction that is
    // guaranteed off-board. (0,0) moving North (row -1) leaves the board.
    auto model = makeModelAt(Position{0, 0}, /*fp*/ 50, /*pwr*/ 10, /*wr*/ 0);
    const bool moved = model->moveArcanist(Direction::No);

    RV_CHECK(moved == false);
    RV_CHECK(NotificationLog::instance().empty()); // no-op stays quiet
}

// --- A fragment walk-over pushes a "collect a Rune Fragment" message ---------

RV_TEST(notify_fragment_walkover_pushes_collect_message) {
    NotificationLog::instance().clear();

    auto model = makeModelAt(Position{2, 3}, /*fp*/ 50, /*pwr*/ 10, /*wr*/ 0);
    auto frag = std::unique_ptr<RuneFragment>(new RuneFragment(2));
    frag->setPosition(Position{2, 4});
    model->level()->addItem(std::move(frag));

    RV_CHECK(model->moveArcanist(Direction::Ea) == true);
    RV_CHECK(anyMessageContains("You collect a Rune Fragment"));
    RV_CHECK(anyMessageContains("+2 RF"));
}

// --- Attacking an adjacent Spectre pushes an attack message ------------------

RV_TEST(notify_attack_pushes_message_with_name_and_damage) {
    NotificationLog::instance().clear();

    auto model = makeModelAt(Position{2, 3}, /*fp*/ 50, /*pwr*/ 30, /*wr*/ 0);
    // A high-FP Spectre survives the hit so no defeat message is expected.
    model->level()->addSpectre(makeSpectre(Position{2, 4}, /*fp*/ 100,
                                           /*pwr*/ 5, /*wr*/ 0));

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(anyMessageContains("You attack the Wraith"));
    // pwr 30 vs wr 0 => 30 damage.
    RV_CHECK(anyMessageContains("30 damage"));
    // Not lethal: no defeat message.
    RV_CHECK(!anyMessageContains("You defeat"));
}

// --- A lethal attack pushes a defeat message (and a drop message) ------------

RV_TEST(notify_lethal_attack_pushes_defeat_message) {
    NotificationLog::instance().clear();

    auto model = makeModelAt(Position{2, 3}, /*fp*/ 50, /*pwr*/ 30, /*wr*/ 0);
    // Low-FP Spectre dies in one hit.
    model->level()->addSpectre(makeSpectre(Position{2, 4}, /*fp*/ 1,
                                           /*pwr*/ 5, /*wr*/ 0));

    RV_CHECK(model->handleAttack(Direction::Ea) == true);
    RV_CHECK(anyMessageContains("You attack the Wraith"));
    RV_CHECK(anyMessageContains("You defeat the Wraith!"));
    // GenericSpectre uses the base drop of 1 RF.
    RV_CHECK(anyMessageContains("Rune Fragment(s)"));
}

// --- resolveSpectreAttack pushes a hit OR a miss message ---------------------

RV_TEST(notify_spectre_attack_pushes_hit_or_miss) {
    NotificationLog::instance().clear();
    rng::seed(12345); // deterministic 50% roll

    auto model = makeModelAt(Position{2, 3}, /*fp*/ 100, /*pwr*/ 10, /*wr*/ 0);
    auto spectre = makeSpectre(Position{2, 4}, /*fp*/ 100, /*pwr*/ 30, /*wr*/ 0);

    model->resolveSpectreAttack(*spectre);

    // Exactly one of the two outcomes must have been logged, and it names the
    // Spectre type (Wraith).
    const bool hit = anyMessageContains("hits you for");
    const bool miss = anyMessageContains("attacks but misses");
    RV_CHECK(hit || miss);
    RV_CHECK(anyMessageContains("Wraith"));
}

// --- useScroll pushes "You use <ScrollType>." --------------------------------

RV_TEST(notify_use_scroll_pushes_use_message) {
    NotificationLog::instance().clear();

    auto model = makeModelAt(Position{2, 3}, /*fp*/ 50, /*pwr*/ 10, /*wr*/ 0);
    auto scroll = std::unique_ptr<Scroll>(new Scroll(ScrollType::AttuneFocus));
    scroll->setPosition(Position{2, 4});
    model->level()->addItem(std::move(scroll));

    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(anyMessageContains("You use Attune Focus"));
}
