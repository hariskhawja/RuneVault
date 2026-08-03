// test_scroll_use.cpp
// RuneVault — Scroll item + `u <dir>` scroll use (task 12.2).
//
// Covers the Scroll Item and Model::useScroll behaviour:
//   * Requirement 8.9 / DD-3: a Scroll always renders as `?` regardless of type.
//   * Requirement 8.1: `u <dir>` consumes an adjacent scroll and removes it.
//   * Requirement 8.2: `u <dir>` with no scroll there is a no-op (returns false).
//   * Requirement 8.3: Attune Focus permanently restores up to 10 FP (capped).
//   * Requirement 8.4: Drain Focus permanently reduces up to 10 FP (floored 0).
//   * Requirements 8.5-8.8: the four temporary stat scrolls shift effective
//     Pwr/WR by +/-5 (floored at 0 for the debuffs) and stack via repeated use.
//
// **Validates: Requirements 8.1, 8.2, 8.3, 8.4, 8.9**

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/item.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/scroll_type.h"

#include <memory>

namespace {

using namespace runevault;

// Build a Model with a small all-floor board and an Arcanist at (1,1) so all 8
// neighbours are in-bounds walkable floor tiles. `fp/pwr/wr` seed the Arcanist.
std::unique_ptr<Model> makeScrollModel(int fp = 30, int pwr = 20, int wr = 20) {
    Board board(3, 3);
    for (int r = 0; r < board.rows(); ++r) {
        for (int c = 0; c < board.cols(); ++c) {
            board.setCell(Position{r, c}, CellType::Floor, 1);
        }
    }
    auto model = std::unique_ptr<Model>(new Model());
    model->setLevel(std::unique_ptr<Level>(new Level(std::move(board))));
    model->setArcanist(std::make_shared<Arcanist>(fp, pwr, wr));
    model->setArcanistPosition(Position{1, 1});
    return model;
}

// Place a scroll of `type` one step EAST of the Arcanist (its target tile for
// useScroll(Direction::Ea)).
void placeScrollEast(Model &model, ScrollType type) {
    const Position target =
        Geometry::step(model.arcanistPosition(), Direction::Ea);
    auto scroll = std::unique_ptr<Scroll>(new Scroll(type));
    scroll->setPosition(target);
    model.level()->addItem(std::move(scroll));
}

} // namespace

// Requirement 8.9 / DD-3: every scroll type renders as `?`.
RV_TEST(scroll_symbol_is_question_mark_for_every_type) {
    const ScrollType all[] = {ScrollType::AttuneFocus, ScrollType::SurgePower,
                              ScrollType::FortifyWard, ScrollType::DrainFocus,
                              ScrollType::SapPower,    ScrollType::ErodeWard};
    for (ScrollType t : all) {
        Scroll s(t);
        RV_CHECK(s.symbol() == '?');
        RV_CHECK(s.symbol() == glyphs::kScroll);
        RV_CHECK(s.scrollType() == t);
    }
}

// Requirement 8.1: `u <dir>` with an adjacent scroll consumes it (returns true)
// and removes it from the level.
RV_TEST(usescroll_consumes_adjacent_scroll) {
    auto model = makeScrollModel();
    placeScrollEast(*model, ScrollType::AttuneFocus);
    const Position target = Geometry::step(model->arcanistPosition(), Direction::Ea);

    RV_CHECK(model->level()->itemAt(target) != nullptr); // present before

    const bool used = model->useScroll(Direction::Ea);

    RV_CHECK(used == true);                               // turn consumed
    RV_CHECK(model->level()->itemAt(target) == nullptr);  // scroll removed
}

// Requirement 8.2: `u <dir>` with NO scroll there is a no-op — returns false and
// nothing changes.
RV_TEST(usescroll_no_scroll_is_noop) {
    auto model = makeScrollModel(25, 20, 20);
    const int fpBefore = model->arcanist()->fp();

    const bool used = model->useScroll(Direction::Ea);

    RV_CHECK(used == false);                        // turn NOT consumed
    RV_CHECK(model->arcanist()->fp() == fpBefore);  // no effect applied
}

// The string-token overload also works and rejects bad tokens.
RV_TEST(usescroll_token_overload) {
    auto model = makeScrollModel();
    placeScrollEast(*model, ScrollType::SurgePower);

    RV_CHECK(model->useScroll(std::string("bogus")) == false); // bad token
    RV_CHECK(model->useScroll(std::string("ea")) == true);     // valid, consumes
}

// Requirement 8.3: Attune Focus restores up to 10 FP, capped at maxFp. Reduce
// FP first so the +10 has room, then confirm it never exceeds max.
RV_TEST(usescroll_attune_focus_heals_up_to_ten_capped) {
    auto model = makeScrollModel(30, 20, 20); // maxFp == 30
    model->arcanist()->reduceFp(15);          // fp = 15
    placeScrollEast(*model, ScrollType::AttuneFocus);

    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->arcanist()->fp() == 25); // 15 + 10

    // Near the cap the heal is clamped to maxFp.
    placeScrollEast(*model, ScrollType::AttuneFocus);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->arcanist()->fp() == 30); // 25 + 10 -> capped at 30
    RV_CHECK(model->isScrollIdentified(static_cast<int>(ScrollType::AttuneFocus)));
}

// Requirement 8.4: Drain Focus reduces up to 10 FP, floored at 0.
RV_TEST(usescroll_drain_focus_reduces_up_to_ten_floored) {
    auto model = makeScrollModel(30, 20, 20);
    model->arcanist()->reduceFp(24); // fp = 6
    placeScrollEast(*model, ScrollType::DrainFocus);

    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->arcanist()->fp() == 0); // 6 - 10 -> floored at 0
}

// Requirement 8.5: Surge Power raises effective Power by 5.
RV_TEST(usescroll_surge_power_raises_effective_pwr) {
    auto model = makeScrollModel(30, 20, 20);
    RV_CHECK(model->effectivePwr() == 20);
    placeScrollEast(*model, ScrollType::SurgePower);

    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectivePwr() == 25);
    RV_CHECK(model->effectiveWr() == 20); // WR unaffected
}

// Requirement 8.6: Fortify Ward raises effective Ward Rating by 5.
RV_TEST(usescroll_fortify_ward_raises_effective_wr) {
    auto model = makeScrollModel(30, 20, 20);
    placeScrollEast(*model, ScrollType::FortifyWard);

    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectiveWr() == 25);
    RV_CHECK(model->effectivePwr() == 20); // Pwr unaffected
}

// Requirement 8.7: Sap Power lowers effective Power by 5, floored at 0.
RV_TEST(usescroll_sap_power_lowers_effective_pwr_floored) {
    auto model = makeScrollModel(30, 3, 20); // small pwr to test the floor
    placeScrollEast(*model, ScrollType::SapPower);

    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectivePwr() == 0); // max(0, 3 - 5) == 0
}

// Requirement 8.8: Erode Ward lowers effective Ward Rating by 5, floored at 0.
RV_TEST(usescroll_erode_ward_lowers_effective_wr_floored) {
    auto model = makeScrollModel(30, 20, 20);
    placeScrollEast(*model, ScrollType::ErodeWard);

    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectiveWr() == 15); // 20 - 5
}

// Requirements 8.5: temporary effects stack via repeated use — two Surge Power
// scrolls -> +10 effective Power.
RV_TEST(usescroll_temporary_effects_stack) {
    auto model = makeScrollModel(30, 20, 20);

    placeScrollEast(*model, ScrollType::SurgePower);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectivePwr() == 25);

    placeScrollEast(*model, ScrollType::SurgePower);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->effectivePwr() == 30); // stacked +5 +5
}
