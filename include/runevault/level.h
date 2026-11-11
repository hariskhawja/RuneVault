#ifndef RUNEVAULT_LEVEL_H
#define RUNEVAULT_LEVEL_H

#include "runevault/board.h"
#include "runevault/geometry.h"
#include "runevault/item.h"
#include "runevault/spectre.h"

#include <memory>
#include <vector>

namespace runevault {

constexpr int kChamberCount = 5;

Board makeDefaultBoard();

class Level {
public:
    explicit Level(Board board);

    // Read-only access to the owned board.
    const Board &board() const { return board_; }

    // The stairway's location on the board.
    Position stairway() const { return stairway_; }
    void setStairway(const Position &p) { stairway_ = p; }

    // Whether the stairway has been revealed (Cipher Gem picked up). Starts
    // false and is flipped when the current level's Cipher Gem is carried.
    bool stairwayRevealed() const { return stairwayRevealed_; }
    void setStairwayRevealed(bool revealed) { stairwayRevealed_ = revealed; }

    // Take ownership of `item`, adding it to the level.
    void addItem(std::unique_ptr<Item> item);

    // Read-only access to all items on the level, in insertion order.
    const std::vector<std::unique_ptr<Item>> &items() const { return items_; }

    // The item currently occupying `p`, or nullptr if the tile holds no item.
    // If multiple items share a tile the first inserted is returned.
    Item *itemAt(const Position &p) const;

    // Detach and return the item occupying `p` (transferring ownership to the
    // caller), removing it from the level. Returns nullptr if no item is there.
    // Transfers ownership to the caller; returns nullptr if the tile is empty.
    std::unique_ptr<Item> removeItemAt(const Position &p);

    // Take ownership of `s`, adding it to the level.
    void addSpectre(std::unique_ptr<Spectre> s);

    // Read-only access to all Spectres on the level, in insertion order.
    const std::vector<std::unique_ptr<Spectre>> &spectres() const {
        return spectres_;
    }

    Spectre *spectreAt(const Position &p) const;

    void removeDeadSpectres();

private:
    Board board_;
    Position stairway_{0, 0};
    bool stairwayRevealed_ = false;
    std::vector<std::unique_ptr<Item>> items_;
    std::vector<std::unique_ptr<Spectre>> spectres_;
};

} // namespace runevault

#endif // RUNEVAULT_LEVEL_H
