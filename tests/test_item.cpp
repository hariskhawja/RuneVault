// test_item.cpp
// RuneVault — Item base + generic RuneFragment tests (task 5.2).
//
// Item is abstract, so the base interface (position round-trip, the pure
// virtual onWalkOver / symbol) is exercised through RuneFragment plus a tiny
// test-only concrete Item subclass. Requirement 2.4 fixes the fragment glyph
// `*` (DD-7).

#include "test_util.h"

#include "runevault/item.h"
#include "runevault/geometry.h"
#include "runevault/view.h"

namespace {

// A minimal concrete Item used only to verify the abstract base interface
// (position storage + the ability to override symbol/onWalkOver).
class DummyItem : public runevault::Item {
public:
    char symbol() const override { return 'Z'; }
    bool onWalkOver(runevault::Model & /*model*/) override { return false; }
};

} // namespace

RV_TEST(rune_fragment_symbol_is_star) {
    using namespace runevault;
    RuneFragment frag(1);
    RV_CHECK(frag.symbol() == '*');
    RV_CHECK(frag.symbol() == glyphs::kRuneFragment);
}

RV_TEST(rune_fragment_value_returns_constructed_value) {
    using namespace runevault;
    RV_CHECK(RuneFragment(1).value() == 1); // common shard
    RV_CHECK(RuneFragment(2).value() == 2); // resonant shard
    RV_CHECK(RuneFragment(4).value() == 4); // lich cache
    RV_CHECK(RuneFragment(6).value() == 6); // shard coffer
}

RV_TEST(rune_fragment_position_round_trips) {
    using namespace runevault;
    RuneFragment frag(2);
    // Default-constructed position is the origin.
    RV_CHECK(frag.position() == (Position{0, 0}));

    frag.setPosition(Position{7, 12});
    RV_CHECK(frag.position().row == 7);
    RV_CHECK(frag.position().col == 12);
    RV_CHECK(frag.position() == (Position{7, 12}));
}

RV_TEST(item_base_interface_via_dummy_subclass) {
    using namespace runevault;
    DummyItem item;
    item.setPosition(Position{3, 4});
    RV_CHECK(item.position() == (Position{3, 4}));
    RV_CHECK(item.symbol() == 'Z');

    // The base pointer sees the overridden symbol/position (polymorphism).
    Item *base = &item;
    RV_CHECK(base->symbol() == 'Z');
    RV_CHECK(base->position() == (Position{3, 4}));
}
