#ifndef RUNEVAULT_GUARDED_ITEM_H
#define RUNEVAULT_GUARDED_ITEM_H

#include "runevault/fragment_grade.h"
#include "runevault/item.h"
#include "runevault/view.h"

#include <memory>
#include <string>

namespace runevault {

class GuardState {
public:
    virtual ~GuardState() = default;

    virtual bool canCollect() const = 0;
};

class LockedState : public GuardState {
public:
    bool canCollect() const override { return false; }
};

class UnlockedState : public GuardState {
public:
    bool canCollect() const override { return true; }
};

// Anchor death unlocks collection; each item can be collected only once.
class GuardedItem : public Item {
public:
    explicit GuardedItem(bool startUnlocked = false);

    bool isUnlocked() const { return state_->canCollect(); }
    bool isLocked() const { return !state_->canCollect(); }

    void onAnchorDeath();

    bool tryCollect(Model &model);

    // True once this item has been successfully collected.
    bool collected() const { return collected_; }

    bool onWalkOver(Model &model) override;

    bool blocksArcanist() const override { return false; }

protected:
    virtual void applyCollectionEffect(Model &model) = 0;

    virtual std::string collectionMessage() const = 0;

private:
    std::unique_ptr<GuardState> state_; // current guard state (Locked/Unlocked)
    bool collected_ = false;            // one-shot collection latch
};

class ShardCoffer : public GuardedItem {
public:
    ShardCoffer() : GuardedItem(/*startUnlocked=*/false) {}

    char symbol() const override { return glyphs::kShardCoffer; }

    int value() const { return fragmentValue(FragmentGrade::ShardCoffer); }

protected:
    void applyCollectionEffect(Model &model) override;
    std::string collectionMessage() const override;
};

class AegisCloak : public GuardedItem {
public:
    AegisCloak() : GuardedItem(/*startUnlocked=*/false) {}

    char symbol() const override { return glyphs::kAegisCloak; }

protected:
    void applyCollectionEffect(Model &model) override;
    std::string collectionMessage() const override;
};

class CipherGem : public GuardedItem {
public:
    CipherGem() : GuardedItem(/*startUnlocked=*/true) {}

    char symbol() const override { return glyphs::kCipherGem; }

protected:
    void applyCollectionEffect(Model &model) override;
    std::string collectionMessage() const override;
};

} // namespace runevault

#endif // RUNEVAULT_GUARDED_ITEM_H
