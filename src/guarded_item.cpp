// guarded_item.cpp
// RuneVault — out-of-line guarded-item / State-machine definitions (task 13.2).
//
// The guard lock uses the State pattern (DQ5): GuardedItem holds a
// unique_ptr<GuardState> that starts Locked (or Unlocked for the dropped Cipher
// Gem, Requirement 9.3) and flips to Unlocked exactly once when the guarding
// Vault Anchor dies (onAnchorDeath, Requirement 9.5). Collection is gated on
// that state (Requirements 9.4/9.5/9.7) while walking onto the tile is always
// allowed (DD-4).
//
// model.h is included HERE (not in the header) so the collection effects can
// call the Model's mutators; the header only needs the forward-declared Model
// it inherits from item.h.

#include "runevault/guarded_item.h"

#include "runevault/model.h"
#include "runevault/notification_log.h" // notify (walk-over collection message)

namespace runevault {

GuardedItem::GuardedItem(bool startUnlocked)
    : state_(startUnlocked
                 ? std::unique_ptr<GuardState>(new UnlockedState())
                 : std::unique_ptr<GuardState>(new LockedState())) {}

void GuardedItem::onAnchorDeath() {
    // Requirement 9.5: the guarding Vault Anchor is defeated -> unlock. This is
    // idempotent — re-unlocking an already-Unlocked item is a harmless swap.
    state_.reset(new UnlockedState());
}

bool GuardedItem::tryCollect(Model &model) {
    // One-shot: never award twice (Requirements 9.4/9.5/9.7).
    if (collected_) {
        return false;
    }
    // Requirement 9.4/9.7: while Locked, collection is prevented with no effect.
    if (!state_->canCollect()) {
        return false;
    }
    // Unlocked and not yet collected -> perform the subclass effect and latch.
    // Physical removal from the Level is the caller's responsibility (task 13.3).
    applyCollectionEffect(model);
    collected_ = true;
    return true;
}

bool GuardedItem::onWalkOver(Model &model) {
    // DD-4: walking onto the tile is always allowed; collection only happens
    // once Unlocked. On a successful collect, announce it and report consumed
    // (true) so the owning Level removes the item. While Locked / already
    // collected, tryCollect is a no-op and the item stays in place (false).
    if (tryCollect(model)) {
        notify(collectionMessage());
        return true;
    }
    return false;
}

// --- Concrete collection effects --------------------------------------------

void ShardCoffer::applyCollectionEffect(Model &model) {
    // Requirement 9.5: award the coffer's 6 Rune Fragments.
    model.addRuneFragments(value());
}

std::string ShardCoffer::collectionMessage() const {
    return "You claim the Shard Coffer (+6 RF).";
}

void AegisCloak::applyCollectionEffect(Model &model) {
    // Requirement 9.7: the Arcanist now carries the Aegis Cloak (the damage
    // halving of Requirement 9.8 lives in the combat layer via carryingAegisCloak()).
    model.setCarryingAegisCloak(true);
}

std::string AegisCloak::collectionMessage() const {
    return "You don the Aegis Cloak -- incoming damage is halved.";
}

void CipherGem::applyCollectionEffect(Model &model) {
    // Requirement 9.3: mark the gem carried, lift the Ward, reveal the stairway.
    model.setCarryingCipherGem(true);
    model.revealStairwayIfCarryingGem();
}

std::string CipherGem::collectionMessage() const {
    return "You pick up the Cipher Gem -- the stairway is revealed!";
}

} // namespace runevault
