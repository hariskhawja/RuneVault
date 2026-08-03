// test_walkover.cpp
// RuneVault — guarded-item walk-over allowance and stairway reveal (task 6.5).
//
// Covers the two model-side behaviours implemented in task 6.5:
//   * DD-4 / Requirement 5.5: the Arcanist may occupy a guarded-item tile
//     regardless of guard state; movement is never blocked by an item and a
//     non-fragment item is occupied, not collected.
//   * Requirements 2.6/2.7: the stairway is hidden by default and is revealed
//     once the Cipher Gem is carried and the Arcanist acts.
//
// **Validates: Requirements 5.5, 2.6, 2.7**

#include "test_util.h"

#include "runevault/item.h"
#include "runevault/level.h"
#include "runevault/model.h"

#include <memory>

namespace {

using namespace runevault;

// A minimal non-fragment Item standing in for a guarded Major Item (Shard
// Coffer / Aegis Cloak). It is deliberately NOT a RuneFragment so the model's
// walk-over logic must occupy — but never collect — it. Its glyph is arbitrary
// and its walk-over effect is a no-op (guarded collection is gated elsewhere).
class DummyGuardedItem : public Item {
public:
    char symbol() const override { return '$'; }
    bool onWalkOver(Model &) override { return false; }
    // DD-4: like the real guarded Major Items, this stand-in is walk-onto-able
    // (movement is never blocked by it); collection is gated elsewhere.
    bool blocksArcanist() const override { return false; }
};

// Build a Model whose current level is a small all-floor strip so movement is
// always walkable. Places the Arcanist at (row, col=0).
std::unique_ptr<Model> makeStripModel(int row = 1) {
    Board board(3, 3);
    for (int r = 0; r < board.rows(); ++r) {
        for (int c = 0; c < board.cols(); ++c) {
            board.setCell(Position{r, c}, CellType::Floor, 1);
        }
    }
    auto model = std::unique_ptr<Model>(new Model());
    model->setLevel(std::unique_ptr<Level>(new Level(std::move(board))));
    model->setArcanistPosition(Position{row, 0});
    return model;
}

} // namespace

// DD-4 / Requirement 5.5: stepping onto a guarded (non-fragment) item tile
// succeeds, the Arcanist ends up on the tile, and the item is left in place.
RV_TEST(walkover_guarded_item_is_occupied_not_collected) {
    auto model = makeStripModel();
    const Position start = model->arcanistPosition();
    const Position target{start.row, start.col + 1}; // one step east

    auto item = std::unique_ptr<DummyGuardedItem>(new DummyGuardedItem());
    item->setPosition(target);
    model->level()->addItem(std::move(item));

    const bool moved = model->moveArcanist(Direction::Ea);

    RV_CHECK(moved == true);                              // move not blocked
    RV_CHECK(model->arcanistPosition() == target);        // occupies the tile
    RV_CHECK(model->level()->itemAt(target) != nullptr);  // item NOT collected
}

// Requirement 2.6: a freshly constructed level hides the stairway.
RV_TEST(walkover_stairway_hidden_by_default) {
    auto model = makeStripModel();
    RV_CHECK(model->level()->stairwayRevealed() == false);
}

// Requirement 2.7: carrying the Cipher Gem reveals the stairway after a move.
RV_TEST(walkover_stairway_revealed_when_carrying_gem) {
    auto model = makeStripModel();
    model->level()->setStairway(Position{2, 2});
    model->setCarryingCipherGem(true);

    RV_CHECK(model->level()->stairwayRevealed() == false); // still hidden pre-move

    const bool moved = model->moveArcanist(Direction::Ea);

    RV_CHECK(moved == true);
    RV_CHECK(model->level()->stairwayRevealed() == true);
}

// Requirement 2.6: without the Cipher Gem, moving never reveals the stairway.
RV_TEST(walkover_stairway_stays_hidden_without_gem) {
    auto model = makeStripModel();
    model->level()->setStairway(Position{2, 2});

    const bool moved = model->moveArcanist(Direction::Ea);

    RV_CHECK(moved == true);
    RV_CHECK(model->level()->stairwayRevealed() == false);
}
