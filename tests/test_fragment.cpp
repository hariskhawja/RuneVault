// test_fragment.cpp
// RuneVault — fragment walk-over collection (task 6.3, Requirement 5.4).
//
// Verifies that stepping the Arcanist onto a Rune Fragment tile adds the
// fragment's value to the run's RF total and removes it from the level, while
// non-fragment items are left in place and empty tiles change nothing.

#include "test_util.h"

#include "runevault/item.h"
#include "runevault/level.h"
#include "runevault/model.h"

#include <memory>

namespace {

using namespace runevault;

// A tiny non-fragment item used to prove only RuneFragments are auto-collected
// on walk-over. It records nothing; its mere survival on the level is the test.
class DummyItem : public Item {
public:
    char symbol() const override { return '?'; }
    bool onWalkOver(Model & /*model*/) override { return false; }
};

// Build a Model seated on the default vault with the Arcanist inside chamber 1
// (interior floor is rows 1..4, cols 1..9). Returns the Model with the Arcanist
// at `start`.
Model makeModelAt(const Position &start) {
    Model model;
    auto level = std::unique_ptr<Level>(new Level(makeDefaultBoard()));
    model.setLevel(std::move(level));
    model.setArcanistPosition(start);
    return model;
}

// Place a fragment worth `value` at `p` on the model's level.
void placeFragment(Model &model, const Position &p, int value) {
    auto frag = std::unique_ptr<RuneFragment>(new RuneFragment(value));
    frag->setPosition(p);
    model.level()->addItem(std::move(frag));
}

} // namespace

RV_TEST(fragment_walk_over_collects_value_and_removes_item) {
    // Arcanist at (1,1); fragment worth 4 immediately east at (1,2).
    Model model = makeModelAt(Position{1, 1});
    const Position fragPos{1, 2};
    placeFragment(model, fragPos, 4);

    RV_CHECK(model.runeFragmentTotal() == 0);
    RV_CHECK(model.level()->itemAt(fragPos) != nullptr);

    const bool moved = model.moveArcanist(Direction::Ea);

    RV_CHECK(moved);                                   // Arcanist still moved
    RV_CHECK(model.arcanistPosition() == fragPos);     // onto the fragment tile
    RV_CHECK(model.runeFragmentTotal() == 4);          // value added (Req 5.4)
    RV_CHECK(model.level()->itemAt(fragPos) == nullptr); // fragment removed
}

RV_TEST(walking_onto_empty_floor_does_not_change_rf_total) {
    Model model = makeModelAt(Position{1, 1});

    const bool moved = model.moveArcanist(Direction::Ea); // onto empty (1,2)

    RV_CHECK(moved);
    RV_CHECK(model.arcanistPosition() == (Position{1, 2}));
    RV_CHECK(model.runeFragmentTotal() == 0);
}

RV_TEST(non_fragment_item_is_not_collected_on_walk_over) {
    // Piazza Q4: movement is never blocked by an item. Walking onto a generic
    // non-fragment Item (e.g. DummyItem) is ALLOWED — the Arcanist occupies the
    // tile — but only Rune Fragments are collected on walk-over, so the item
    // stays put and the RF total is unchanged.
    Model model = makeModelAt(Position{1, 1});
    const Position itemPos{1, 2};
    auto dummy = std::unique_ptr<DummyItem>(new DummyItem());
    dummy->setPosition(itemPos);
    model.level()->addItem(std::move(dummy));

    const bool moved = model.moveArcanist(Direction::Ea);

    RV_CHECK(moved);                                      // walk-over allowed (Q4)
    RV_CHECK(model.arcanistPosition() == itemPos);         // occupies the tile
    RV_CHECK(model.runeFragmentTotal() == 0);             // nothing collected
    RV_CHECK(model.level()->itemAt(itemPos) != nullptr);  // still present
}

RV_TEST(fragment_value_grades_accumulate_across_pickups) {
    // Walk east across chamber 1's interior row 1, collecting the four grade
    // values 1, 2, 4, 6 laid out on cols 2..5. Total should be 13.
    Model model = makeModelAt(Position{1, 1});
    placeFragment(model, Position{1, 2}, 1); // common shard
    placeFragment(model, Position{1, 3}, 2); // resonant shard
    placeFragment(model, Position{1, 4}, 4); // Lich cache
    placeFragment(model, Position{1, 5}, 6); // Shard Coffer

    RV_CHECK(model.moveArcanist(Direction::Ea));
    RV_CHECK(model.runeFragmentTotal() == 1);
    RV_CHECK(model.moveArcanist(Direction::Ea));
    RV_CHECK(model.runeFragmentTotal() == 3);
    RV_CHECK(model.moveArcanist(Direction::Ea));
    RV_CHECK(model.runeFragmentTotal() == 7);
    RV_CHECK(model.moveArcanist(Direction::Ea));
    RV_CHECK(model.runeFragmentTotal() == 13);

    // All four fragments are gone from the level.
    RV_CHECK(model.level()->itemAt(Position{1, 2}) == nullptr);
    RV_CHECK(model.level()->itemAt(Position{1, 3}) == nullptr);
    RV_CHECK(model.level()->itemAt(Position{1, 4}) == nullptr);
    RV_CHECK(model.level()->itemAt(Position{1, 5}) == nullptr);
}
