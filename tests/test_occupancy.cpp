// test_occupancy.cpp
// RuneVault — movement onto occupied tiles (Piazza Q4).
//
// Piazza Q4 clarifies the occupancy invariant ("no two objects share a cell,
// except the stated walk-over exceptions") is a SPAWN-time rule, NOT a movement
// gate: "nothing in the spec blocks movement onto an item tile; only collection
// is gated." So the Arcanist may freely walk onto ANY item tile:
//   * Rune Fragments — walked over and COLLECTED on entry (Requirement 5.4).
//   * Scrolls — walked over and merely OCCUPIED; the scroll stays put and is
//     used from an adjacent tile via `u <dir>` (not collected on walk-over).
//   * Guarded Major Items — walked over and occupied (DD-4); collection stays
//     gated by the guarded-item State machine.
//
// The one thing that DOES block movement is a LIVING Spectre: the Arcanist
// attacks it with `a <dir>` and never walks onto it (Q4 addresses items, not
// Spectres).
//
// **Validates: movement onto occupied tiles per Q4 (Requirements 5.2/5.4/5.5)**

#include "test_util.h"

#include "runevault/guarded_item.h"
#include "runevault/item.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/notification_log.h"
#include "runevault/scroll_type.h"
#include "runevault/spectre.h"

#include <memory>

namespace {

using namespace runevault;

// Build a Model on the default vault with the Arcanist at `start`. (2,3) is an
// interior floor tile whose east neighbour (2,4) is also interior floor,
// matching the existing movement/attack tests.
Model makeModelAt(const Position &start) {
    Model model;
    model.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model.setArcanistPosition(start);
    return model;
}

bool anyMessageContains(const std::string &needle) {
    for (const std::string &m : NotificationLog::instance().messages()) {
        if (m.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

// Q4: a Scroll does NOT block movement. The Arcanist walks onto its tile and
// occupies it; the scroll stays put (it is used from an adjacent tile via
// `u <dir>`, never collected on walk-over).
RV_TEST(occupancy_move_onto_scroll_is_allowed_not_collected) {
    NotificationLog::instance().clear();
    Model model = makeModelAt(Position{2, 3});
    const Position scrollPos{2, 4};
    auto scroll = std::unique_ptr<Scroll>(new Scroll(ScrollType::SurgePower));
    scroll->setPosition(scrollPos);
    model.level()->addItem(std::move(scroll));

    const bool moved = model.moveArcanist(Direction::Ea);

    RV_CHECK(moved);                                       // walk-onto allowed (Q4)
    RV_CHECK(model.arcanistPosition() == scrollPos);        // occupies the tile
    RV_CHECK(model.level()->itemAt(scrollPos) != nullptr);  // scroll still there
}

// A successful walk onto a scroll pushes the ordinary move notification (the
// move is a genuine, turn-consuming move — nothing is blocked).
RV_TEST(occupancy_move_onto_scroll_pushes_move_message) {
    NotificationLog::instance().clear();
    Model model = makeModelAt(Position{2, 3});
    auto scroll = std::unique_ptr<Scroll>(new Scroll(ScrollType::SurgePower));
    scroll->setPosition(Position{2, 4});
    model.level()->addItem(std::move(scroll));

    RV_CHECK(model.moveArcanist(Direction::Ea) == true);
    RV_CHECK(anyMessageContains("You move")); // ordinary move notification
}

// A LIVING Spectre BLOCKS movement: the Arcanist attacks it with `a <dir>`,
// never walks onto it.
RV_TEST(occupancy_move_onto_living_spectre_is_rejected) {
    NotificationLog::instance().clear();
    Model model = makeModelAt(Position{2, 3});
    const Position spectrePos{2, 4};
    auto spectre = std::unique_ptr<GenericSpectre>(new GenericSpectre(
        SpectreType::Wraith, /*fp*/ 60, /*pwr*/ 20, /*wr*/ 10, /*chamber*/ 1,
        spectrePos, 'W'));
    model.level()->addSpectre(std::move(spectre));

    const bool moved = model.moveArcanist(Direction::Ea);

    RV_CHECK(!moved);                                       // blocked — no move
    RV_CHECK(model.arcanistPosition() == (Position{2, 3})); // position unchanged
    RV_CHECK(model.level()->spectreAt(spectrePos) != nullptr); // spectre still there
}

// EXCEPTION: a Rune Fragment is walked over and collected on entry.
RV_TEST(occupancy_move_onto_fragment_is_allowed_and_collects) {
    NotificationLog::instance().clear();
    Model model = makeModelAt(Position{2, 3});
    const Position fragPos{2, 4};
    auto frag = std::unique_ptr<RuneFragment>(new RuneFragment(4));
    frag->setPosition(fragPos);
    model.level()->addItem(std::move(frag));

    RV_CHECK(model.runeFragmentTotal() == 0);

    const bool moved = model.moveArcanist(Direction::Ea);

    RV_CHECK(moved);                                       // walk-over allowed
    RV_CHECK(model.arcanistPosition() == fragPos);          // onto the tile
    RV_CHECK(model.runeFragmentTotal() == 4);               // collected (Req 5.4)
    RV_CHECK(model.level()->itemAt(fragPos) == nullptr);    // fragment removed
}

// EXCEPTION (DD-4): a guarded item (locked Shard Coffer) is walk-onto-able; the
// Arcanist occupies its tile but does NOT collect it while it is Locked.
RV_TEST(occupancy_move_onto_locked_guarded_item_is_allowed_not_collected) {
    NotificationLog::instance().clear();
    Model model = makeModelAt(Position{2, 3});
    const Position cofferPos{2, 4};
    auto coffer = std::unique_ptr<ShardCoffer>(new ShardCoffer());
    coffer->setPosition(cofferPos);
    ShardCoffer *cofferPtr = coffer.get();
    model.level()->addItem(std::move(coffer));

    RV_CHECK(cofferPtr->isLocked() == true);

    const bool moved = model.moveArcanist(Direction::Ea);

    RV_CHECK(moved);                                       // walk-onto allowed
    RV_CHECK(model.arcanistPosition() == cofferPos);        // occupies the tile
    RV_CHECK(model.level()->itemAt(cofferPos) != nullptr);  // coffer still present
    RV_CHECK(cofferPtr->isLocked() == true);                // still locked
    RV_CHECK(model.runeFragmentTotal() == 0);               // not collected
}
