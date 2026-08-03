// guarded_item.h
// RuneVault — the guarded major items and their State machine (task 13.2).
//
// A "guarded item" is a Major Item that sits on a board tile but cannot be
// COLLECTED until the Vault Anchor guarding it is defeated (Requirements
// 9.4/9.5/9.7). Walking ONTO the tile is always allowed (DD-4 / Requirement
// 5.5) — the Model's movement code only checks cell walkability, never item
// presence — so the gate lives purely in the collection path, expressed with
// the **State** pattern (DQ5): a small fixed set of states, Locked and
// Unlocked, with the item self-driving its transition when its Anchor dies.
//
//   GuardState (abstract)
//     ├── LockedState    canCollect() -> false   (Requirement 9.4/9.7)
//     └── UnlockedState  canCollect() -> true     (Requirement 9.5)
//
//   Item
//     └── GuardedItem (abstract)  — owns the current GuardState + collected flag
//           ├── ShardCoffer   '$'  effect: +6 Rune Fragments   (Req 9.5)
//           ├── AegisCloak    'G'  effect: setCarryingAegisCloak(true) (Req 9.7)
//           └── CipherGem     'C'  effect: carry gem + reveal stairway (Req 9.3)
//
// SCOPE (task 13.2): this task implements the State machine and the three
// concrete items with their own tryCollect / onAnchorDeath / applyCollectionEffect
// semantics only. It does NOT wire Anchor-death into the combat/spawn pipeline
// nor remove the collected item from the Level — that PHYSICAL REMOVAL and the
// Anchor-death → onAnchorDeath() wiring are the caller's responsibility in task
// 13.3. `tryCollect` performs only the model-side effect and reports success;
// the caller detaches the item from the Level (Level::removeItemAt) on a true
// return.
//
// The header only references `Model` by reference (in the pure-virtual /
// override signatures), so — mirroring item.h — we rely on item.h's forward
// declaration of `class Model;` and include model.h only in the .cpp.

#ifndef RUNEVAULT_GUARDED_ITEM_H
#define RUNEVAULT_GUARDED_ITEM_H

#include "runevault/fragment_grade.h" // FragmentGrade + fragmentValue (Shard Coffer = 6)
#include "runevault/item.h"           // Item base + forward-declared Model
#include "runevault/view.h"           // runevault::glyphs (DD-7 symbol map)

#include <memory>
#include <string>

namespace runevault {

// --- State pattern (DQ5) -----------------------------------------------------
// The guard lock is a two-state machine. The only behaviour that differs
// between states is whether the guarded item may be collected, so the abstract
// base exposes a single query. Concrete states are stateless singletons in
// practice; GuardedItem swaps the pointer it holds on onAnchorDeath().
class GuardState {
public:
    virtual ~GuardState() = default;

    // Whether collection is permitted in this state. LockedState -> false
    // (Requirements 9.4/9.7), UnlockedState -> true (Requirement 9.5).
    virtual bool canCollect() const = 0;
};

// Locked: the guarding Vault Anchor is still alive, so collection is prevented
// (Requirements 9.4/9.7). This is the starting state for the Shard Coffer and
// the Aegis Cloak.
class LockedState : public GuardState {
public:
    bool canCollect() const override { return false; }
};

// Unlocked: the guarding Vault Anchor has been defeated, so collection succeeds
// (Requirement 9.5). A dropped Cipher Gem starts in this state (Requirement
// 9.3 — it is immediately collectible on walk-over).
class UnlockedState : public GuardState {
public:
    bool canCollect() const override { return true; }
};

// --- GuardedItem intermediate base ------------------------------------------
// An Item guarded by a Vault Anchor, owning the State machine plus a
// "collected" latch so a successful collection is one-shot (no double-award).
class GuardedItem : public Item {
public:
    // Construct in the Locked state by default (the Shard Coffer / Aegis Cloak
    // begin guarded). Concrete items that start collectible (the dropped Cipher
    // Gem, Requirement 9.3) pass `startUnlocked = true`.
    explicit GuardedItem(bool startUnlocked = false);

    // --- State queries ---------------------------------------------------
    bool isUnlocked() const { return state_->canCollect(); }
    bool isLocked() const { return !state_->canCollect(); }

    // Transition Locked -> Unlocked (Requirement 9.5: the guarding Vault Anchor
    // has been defeated). Idempotent: calling it again while already Unlocked is
    // a harmless no-op. Task 13.3 wires this into the Anchor-death combat path.
    void onAnchorDeath();

    // Attempt to collect this item (Requirements 9.4/9.5/9.7).
    //   * While Locked: collection is PREVENTED — returns false with no effect.
    //   * While Unlocked and not yet collected: applies the subclass collection
    //     effect (applyCollectionEffect), latches `collected_`, and returns true.
    //   * A second call after a successful collect is a no-op returning false
    //     (guards against double-award).
    // On a true return the CALLER (Model, task 13.3) is responsible for the
    // physical removal of this item from the Level; tryCollect performs only the
    // model-side effect.
    bool tryCollect(Model &model);

    // True once this item has been successfully collected.
    bool collected() const { return collected_; }

    // DD-4: walking onto a guarded-item tile is always allowed and does not, by
    // itself, block movement. Walk-over routes through tryCollect so that once
    // the item is Unlocked, stepping onto it collects it; while Locked this is a
    // no-op. Physical removal from the Level on a successful collect is finalized
    // by the Model in task 13.3.
    //
    // Returns true (consumed) when this walk-over actually collected the item
    // (Unlocked and not previously collected), in which case it also pushes the
    // item's collectionMessage(); false when the item stays Locked/already
    // collected and is left in place. The owning Level removes it on true.
    bool onWalkOver(Model &model) override;

    // DD-4: walking onto a guarded-item tile is ALWAYS allowed regardless of
    // lock state, so guarded items never block the Arcanist. Collection stays
    // gated by the State machine (tryCollect), not by movement. This covers the
    // Shard Coffer, Aegis Cloak, and Cipher Gem (the Cipher Gem MUST be walked
    // over to be picked up, Requirement 9.3).
    bool blocksArcanist() const override { return false; }

protected:
    // The concrete per-item effect applied on a successful collection. Called by
    // tryCollect exactly once (on the transition to collected). Subclasses award
    // fragments / set carry flags / reveal the stairway here.
    virtual void applyCollectionEffect(Model &model) = 0;

    // The message announced when THIS item is collected via a walk-over. Kept
    // separate from applyCollectionEffect (which tryCollect runs) so the
    // Anchor-death in-place collection path (CombatVisitor, which calls
    // tryCollect directly) stays silent exactly as before — only the walk-over
    // path (GuardedItem::onWalkOver) emits it.
    virtual std::string collectionMessage() const = 0;

private:
    std::unique_ptr<GuardState> state_; // current guard state (Locked/Unlocked)
    bool collected_ = false;            // one-shot collection latch
};

// --- Concrete guarded items --------------------------------------------------

// The Shard Coffer: the level's guarded Rune Fragment worth 6 (Requirement 9.1
// / 9.5). Renders as `$` (DD-7). Starts Locked; on collection it adds 6 Rune
// Fragments to the Model's total.
class ShardCoffer : public GuardedItem {
public:
    ShardCoffer() : GuardedItem(/*startUnlocked=*/false) {}

    char symbol() const override { return glyphs::kShardCoffer; }

    // The Rune Fragment value of the coffer (Requirement 9.1: 6).
    int value() const { return fragmentValue(FragmentGrade::ShardCoffer); }

protected:
    // Requirement 9.5: collecting the coffer awards its 6 Rune Fragments.
    void applyCollectionEffect(Model &model) override;
    std::string collectionMessage() const override;
};

// The Aegis Cloak: a once-per-run guarded major item (Requirement 9.6/9.7).
// Renders as `G` (DD-7). Starts Locked; on collection the Model records that
// the Arcanist is carrying it (Requirement 9.8's damage halving already lives
// in the combat layer and keys off carryingAegisCloak()).
class AegisCloak : public GuardedItem {
public:
    AegisCloak() : GuardedItem(/*startUnlocked=*/false) {}

    char symbol() const override { return glyphs::kAegisCloak; }

protected:
    // Requirement 9.7: collection (only once Unlocked) sets the carry flag.
    void applyCollectionEffect(Model &model) override;
    std::string collectionMessage() const override;
};

// The Cipher Gem: the per-level Ward key (Requirement 9.2/9.3). Renders as `C`
// (DD-7). A DROPPED gem is immediately collectible, so it starts UNLOCKED
// (Requirement 9.3). On collection the Model records the gem as carried and the
// stairway is revealed. Task 13.3 wires the drop pipeline (placing the gem on
// the carrier's tile); this class supplies the item + its collection effect.
class CipherGem : public GuardedItem {
public:
    CipherGem() : GuardedItem(/*startUnlocked=*/true) {}

    char symbol() const override { return glyphs::kCipherGem; }

protected:
    // Requirement 9.3: carrying the gem lifts the Ward and reveals the stairway.
    void applyCollectionEffect(Model &model) override;
    std::string collectionMessage() const override;
};

} // namespace runevault

#endif // RUNEVAULT_GUARDED_ITEM_H
