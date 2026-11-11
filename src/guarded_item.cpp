#include "runevault/guarded_item.h"

#include "runevault/model.h"
#include "runevault/notification_log.h"

namespace runevault {

GuardedItem::GuardedItem(bool startUnlocked)
    : state_(startUnlocked
                 ? std::unique_ptr<GuardState>(new UnlockedState())
                 : std::unique_ptr<GuardState>(new LockedState())) {}

void GuardedItem::onAnchorDeath() {
    state_.reset(new UnlockedState());
}

bool GuardedItem::tryCollect(Model &model) {
    if (collected_) {
        return false;
    }
    if (!state_->canCollect()) {
        return false;
    }
    applyCollectionEffect(model);
    collected_ = true;
    return true;
}

bool GuardedItem::onWalkOver(Model &model) {
    if (tryCollect(model)) {
        notify(collectionMessage());
        return true;
    }
    return false;
}

void ShardCoffer::applyCollectionEffect(Model &model) {
    model.addRuneFragments(value());
}

std::string ShardCoffer::collectionMessage() const {
    return "You claim the Shard Coffer (+6 RF).";
}

void AegisCloak::applyCollectionEffect(Model &model) {
    model.setCarryingAegisCloak(true);
}

std::string AegisCloak::collectionMessage() const {
    return "You don the Aegis Cloak -- incoming damage is halved.";
}

void CipherGem::applyCollectionEffect(Model &model) {
    model.setCarryingCipherGem(true);
    model.revealStairwayIfCarryingGem();
}

std::string CipherGem::collectionMessage() const {
    return "You pick up the Cipher Gem -- the stairway is revealed!";
}

} // namespace runevault
