// test_scroll_identification.cpp
// RuneVault — scroll identification tracking (task 12.6).
//
// Covers the model-side scroll-identification bookkeeping and the DD-3
// always-`?` board invariant:
//   * Requirement 8.10: a scroll's TYPE becomes "remembered" (identified) on
//     first use and stays identified for the REMAINDER of the run; using a
//     scroll marks ITS type and leaves the other five unidentified.
//   * Requirement 8.9 / DD-3: identification is pure bookkeeping — a Scroll
//     always renders as `?` before and after its type is identified, and
//     identifying one type never changes any scroll's rendered glyph.
//   * The ScrollType-typed API and the int overloads agree.
//
// **Validates: Requirements 8.9, 8.10**

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/item.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/scroll_type.h"

#include <memory>

namespace {

using namespace runevault;

// All six scroll types, for exhaustive "the others stay unidentified" checks.
const ScrollType kAllTypes[] = {
    ScrollType::AttuneFocus, ScrollType::SurgePower, ScrollType::FortifyWard,
    ScrollType::DrainFocus,  ScrollType::SapPower,   ScrollType::ErodeWard};

// Build a Model with a small all-floor board and an Arcanist at (1,1) so all 8
// neighbours are in-bounds walkable floor tiles.
std::unique_ptr<Model> makeModel(int fp = 60, int pwr = 20, int wr = 20) {
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

// Place a scroll of `type` one step in direction `d` from the Arcanist.
void placeScroll(Model &model, Direction d, ScrollType type) {
    const Position target = Geometry::step(model.arcanistPosition(), d);
    auto scroll = std::unique_ptr<Scroll>(new Scroll(type));
    scroll->setPosition(target);
    model.level()->addItem(std::move(scroll));
}

} // namespace

// A fresh Model has no identified scroll types (Requirement 8.10 baseline).
RV_TEST(scroll_identification_fresh_model_has_none_identified) {
    Model m;
    for (ScrollType t : kAllTypes) {
        RV_CHECK(m.isScrollIdentified(t) == false);
    }
}

// Requirement 8.10: using a scroll marks ITS type identified and leaves the
// other five unidentified.
RV_TEST(scroll_identification_use_marks_only_that_type) {
    auto model = makeModel();
    placeScroll(*model, Direction::Ea, ScrollType::SurgePower);

    RV_CHECK(model->isScrollIdentified(ScrollType::SurgePower) == false);
    RV_CHECK(model->useScroll(Direction::Ea) == true);

    RV_CHECK(model->isScrollIdentified(ScrollType::SurgePower) == true);
    for (ScrollType t : kAllTypes) {
        if (t != ScrollType::SurgePower) {
            RV_CHECK(model->isScrollIdentified(t) == false);
        }
    }
}

// Requirement 8.10: identification persists across subsequent actions/uses
// within the run — using other scrolls and moving never un-marks an earlier
// type, and each new use accumulates.
RV_TEST(scroll_identification_persists_across_actions) {
    auto model = makeModel();

    // Use a SurgePower scroll to the east; SurgePower becomes identified.
    placeScroll(*model, Direction::Ea, ScrollType::SurgePower);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->isScrollIdentified(ScrollType::SurgePower) == true);

    // A subsequent move must not un-mark it.
    RV_CHECK(model->moveArcanist(Direction::We) == true); // (1,1) -> (1,0)
    RV_CHECK(model->isScrollIdentified(ScrollType::SurgePower) == true);

    // Move back and use a FortifyWard scroll; both types now identified.
    RV_CHECK(model->moveArcanist(Direction::Ea) == true); // (1,0) -> (1,1)
    placeScroll(*model, Direction::So, ScrollType::FortifyWard);
    RV_CHECK(model->useScroll(Direction::So) == true);
    RV_CHECK(model->isScrollIdentified(ScrollType::FortifyWard) == true);
    RV_CHECK(model->isScrollIdentified(ScrollType::SurgePower) == true);

    // The four unused types remain unidentified.
    RV_CHECK(model->isScrollIdentified(ScrollType::AttuneFocus) == false);
    RV_CHECK(model->isScrollIdentified(ScrollType::DrainFocus) == false);
    RV_CHECK(model->isScrollIdentified(ScrollType::SapPower) == false);
    RV_CHECK(model->isScrollIdentified(ScrollType::ErodeWard) == false);
}

// Requirement 8.9 / DD-3: identifying a scroll's type never changes any
// scroll's rendered glyph — a Scroll is `?` before and after identification.
RV_TEST(scroll_identification_does_not_change_board_symbol) {
    auto model = makeModel();

    // A scroll of every type renders as `?` up-front (nothing identified yet).
    for (ScrollType t : kAllTypes) {
        Scroll s(t);
        RV_CHECK(s.symbol() == '?');
    }

    // Use a SapPower scroll, identifying that type.
    placeScroll(*model, Direction::Ea, ScrollType::SapPower);
    RV_CHECK(model->useScroll(Direction::Ea) == true);
    RV_CHECK(model->isScrollIdentified(ScrollType::SapPower) == true);

    // Every scroll — the now-identified type AND the still-unidentified ones —
    // still renders as `?`. Identification is purely bookkeeping (DD-3).
    for (ScrollType t : kAllTypes) {
        Scroll s(t);
        RV_CHECK(s.symbol() == '?');
        RV_CHECK(s.symbol() == glyphs::kScroll);
    }
}

// The ScrollType-typed API and the int overloads agree on the same key.
RV_TEST(scroll_identification_typed_and_int_overloads_agree) {
    Model m;

    // Mark via the int overload; read back through both APIs.
    m.markScrollIdentified(static_cast<int>(ScrollType::DrainFocus));
    RV_CHECK(m.isScrollIdentified(ScrollType::DrainFocus) == true);
    RV_CHECK(m.isScrollIdentified(static_cast<int>(ScrollType::DrainFocus)) == true);

    // Mark via the typed API; read back through both APIs.
    m.markScrollIdentified(ScrollType::ErodeWard);
    RV_CHECK(m.isScrollIdentified(static_cast<int>(ScrollType::ErodeWard)) == true);
    RV_CHECK(m.isScrollIdentified(ScrollType::ErodeWard) == true);

    // An untouched type reads false through both APIs.
    RV_CHECK(m.isScrollIdentified(ScrollType::AttuneFocus) == false);
    RV_CHECK(m.isScrollIdentified(static_cast<int>(ScrollType::AttuneFocus)) == false);
}
