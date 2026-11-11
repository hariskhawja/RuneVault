#ifndef RUNEVAULT_ITEM_H
#define RUNEVAULT_ITEM_H

#include "runevault/fragment_grade.h"
#include "runevault/geometry.h"
#include "runevault/scroll_type.h"
#include "runevault/view.h"

namespace runevault {

class Model;

// Abstract base for every placeable non-Spectre object. Owns the shared
// position state; subclasses supply their render glyph and walk-over behaviour.
class Item {
public:
    virtual ~Item() = default;

    virtual char symbol() const = 0;

    // The item's board coordinate.
    Position position() const { return position_; }
    void setPosition(const Position &p) { position_ = p; }

    virtual bool onWalkOver(Model &model) = 0;

    virtual bool blocksArcanist() const { return true; }

protected:
    Position position_{};
};

class RuneFragment : public Item {
public:
    // Create a fragment worth `value` Rune Fragments at the (default) origin;
    // callers set the board position via `setPosition`.
    explicit RuneFragment(int value) : value_(value) {}

    explicit RuneFragment(FragmentGrade grade) : value_(fragmentValue(grade)) {}

    char symbol() const override { return glyphs::kRuneFragment; }

    // Collected on entry; fragments do not block movement.
    bool blocksArcanist() const override { return false; }

    // The fragment's Rune Fragment value.
    int value() const { return value_; }

    bool onWalkOver(Model &model) override;

private:
    int value_;
};

class Scroll : public Item {
public:
    // Create a Scroll of the given effect type at the (default) origin; callers
    // set the board position via `setPosition`.
    explicit Scroll(ScrollType type) : type_(type) {}

    char symbol() const override { return glyphs::kScroll; }

    // The scroll's effect type (used by Model::useScroll to apply the effect).
    ScrollType scrollType() const { return type_; }

    bool onWalkOver(Model & /*model*/) override { return false; }

private:
    ScrollType type_;
};

} // namespace runevault

#endif // RUNEVAULT_ITEM_H
