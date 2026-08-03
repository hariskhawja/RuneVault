// item.h
// RuneVault — the Item base class and a generic RuneFragment (task 5.2).
//
// `Item` is the abstract base for every placeable non-Spectre object (design
// "Items: Hierarchy + Guarded-Item State"): Rune Fragments, Arcane Scrolls, the
// Cipher Gem, and the guarded Major Items all derive from it so placement and
// rendering are reused rather than duplicated. This task implements only the
// base plus a generic `RuneFragment`; the Scroll / CipherGem / GuardedItem
// subclasses and the guarded-item State machine arrive in later tasks (12.x /
// 13.x).
//
// The design's interface (from the Item class diagram) is:
//
//     class Item {
//     public:
//         virtual char symbol() const = 0;
//         Position position() const;  void setPosition(const Position&);
//         virtual void onWalkOver(Model&) = 0;
//     };
//
// `onWalkOver` takes a `Model&`, but the Model does not exist yet (task 5.4).
// A pure virtual declaration needs only a *reference* to an incomplete type, so
// we forward-declare `class Model;` and keep `onWalkOver` pure virtual without
// including model.h. This keeps Item compilable standalone.
//
// RuneFragment is a generic Item carrying an int value (the grade-specific
// values — common shard 1, resonant shard 2, Lich cache 4, Shard Coffer 6 —
// are introduced in task 13.1; here we simply store the value passed to the
// constructor). Its board glyph is `*` (DD-7), reused from `runevault::glyphs`.
// `RuneFragment::onWalkOver` (defined in item.cpp) credits its value to the
// Model, announces the pickup, and returns true so the owning Level removes it.

#ifndef RUNEVAULT_ITEM_H
#define RUNEVAULT_ITEM_H

#include "runevault/fragment_grade.h" // FragmentGrade + fragmentValue (task 13.1)
#include "runevault/geometry.h"
#include "runevault/scroll_type.h" // ScrollType (the six Arcane Scroll kinds)
#include "runevault/view.h" // runevault::glyphs (DD-7 symbol map)

namespace runevault {

// Forward declaration only — a reference to an incomplete `Model` is all the
// pure virtual `onWalkOver` needs. The Model is implemented in task 5.4.
class Model;

// Abstract base for every placeable non-Spectre object. Owns the shared
// position state; subclasses supply their render glyph and walk-over behaviour.
class Item {
public:
    virtual ~Item() = default;

    // The board glyph for this item (DD-7). Pure virtual: each concrete item
    // fixes its own symbol (e.g. RuneFragment -> `*`, Scroll -> `?`).
    virtual char symbol() const = 0;

    // The item's board coordinate.
    Position position() const { return position_; }
    void setPosition(const Position &p) { position_ = p; }

    // Invoked when the Arcanist walks onto this item's tile. Pure virtual so
    // each item type defines its own effect (fragment collection, Cipher Gem
    // pickup, ...). Takes `Model&` by reference so only a forward declaration
    // is required here.
    //
    // Returns whether the item was CONSUMED by the walk-over: true means the
    // owning `Level` should physically remove it (the item never deletes
    // itself — the owner does), false means leave it in place. This lets the
    // Model dispatch uniformly (`if (item->onWalkOver(*this)) removeItemAt(...)`)
    // with no RTTI/type-switch, while each subclass owns its own effect,
    // notification, and consumed/kept decision.
    virtual bool onWalkOver(Model &model) = 0;

    // Whether this item BLOCKS the Arcanist from entering its tile (the
    // occupancy invariant: no two objects share a cell). By default an Item
    // blocks movement — the Arcanist cannot walk onto its tile. The handful of
    // walk-over-able items (Rune Fragments, which are collected on entry, and
    // the guarded Major Items per DD-4) override this to return false.
    virtual bool blocksArcanist() const { return true; }

protected:
    Position position_{};
};

// A generic Rune Fragment: an Item carrying an integer value. Grade-specific
// values are assigned by the caller (task 13.1); this class only stores and
// exposes whatever value it is constructed with.
class RuneFragment : public Item {
public:
    // Create a fragment worth `value` Rune Fragments at the (default) origin;
    // callers set the board position via `setPosition`.
    explicit RuneFragment(int value) : value_(value) {}

    // Convenience overload (task 13.1): create a fragment of a named grade, its
    // value taken from `fragmentValue` (Requirement 9.1: common shard 1,
    // resonant shard 2, Lich cache 4, Shard Coffer 6).
    explicit RuneFragment(FragmentGrade grade) : value_(fragmentValue(grade)) {}

    // Rune Fragments always render as `*` (DD-7).
    char symbol() const override { return glyphs::kRuneFragment; }

    // Rune Fragments are the spec's stated occupancy exception: they are walked
    // OVER and collected on entry, so they never block the Arcanist.
    bool blocksArcanist() const override { return false; }

    // The fragment's Rune Fragment value.
    int value() const { return value_; }

    // Walk-over collection (add `value_` to the RF total, push the pickup
    // message) and report consumed (true) so the owning Level removes it. The
    // definition lives in item.cpp.
    bool onWalkOver(Model &model) override;

private:
    int value_;
};

// An Arcane Scroll: an Item carrying one of the six ScrollType effects
// (task 12.2). Unlike a Rune Fragment, a Scroll is NOT collected by walking
// over it — it is consumed via the `u <dir>` command (Model::useScroll), which
// searches the 1-block radius, applies the scroll's effect, and removes it.
//
// Requirement 8.9 / DD-3: a Scroll ALWAYS renders as `?` on the board
// regardless of its type — the board glyph never reveals which effect it holds
// (types are only learned once a scroll of that type has been used).
class Scroll : public Item {
public:
    // Create a Scroll of the given effect type at the (default) origin; callers
    // set the board position via `setPosition`.
    explicit Scroll(ScrollType type) : type_(type) {}

    // Requirement 8.9 / DD-3: every scroll renders as `?`, never revealing its
    // type on the board.
    char symbol() const override { return glyphs::kScroll; }

    // The scroll's effect type (used by Model::useScroll to apply the effect).
    ScrollType scrollType() const { return type_; }

    // Scrolls are NEVER collected by walking over them (contrast RuneFragment):
    // they are consumed via the `u <dir>` command (Model::useScroll). Walking
    // onto a scroll's tile therefore does nothing — this is deliberately a
    // no-op, and it is never consumed on walk-over (returns false).
    bool onWalkOver(Model & /*model*/) override { return false; }

    // Scrolls inherit the default blocksArcanist() == true: they BLOCK
    // movement. A scroll is used from an ADJACENT tile via the `u <dir>`
    // command (Model::useScroll), never by walking onto it, so the Arcanist is
    // kept off the scroll's tile to preserve the occupancy invariant.

private:
    ScrollType type_;
};

} // namespace runevault

#endif // RUNEVAULT_ITEM_H
