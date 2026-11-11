#ifndef RUNEVAULT_MODEL_H
#define RUNEVAULT_MODEL_H

#include <memory>
#include <set>
#include <string>

#include "runevault/arcanist.h"
#include "runevault/arcanist_component.h"
#include "runevault/geometry.h"
#include "runevault/level.h"
#include "runevault/scroll_type.h"
#include "runevault/spectre.h"

namespace runevault {

struct LoadedLayout;

enum class RunState { Playing, Won, Lost };

class Model {
public:
    // Default-construct empty run state: no Arcanist, no Level, level number 1,
    // all totals zeroed and all flags false.
    Model();

    const std::shared_ptr<Arcanist> &arcanist() const { return baseArcanist_; }
    void setArcanist(std::shared_ptr<Arcanist> arcanist) {
        baseArcanist_ = std::move(arcanist);
        if (baseArcanist_ != nullptr) {
            activeArcanist_ = std::make_shared<BaseArcanistComponent>(baseArcanist_);
        } else {
            activeArcanist_.reset();
        }
    }

    ArcanistComponent *activeArcanist() { return activeArcanist_.get(); }
    const ArcanistComponent *activeArcanist() const { return activeArcanist_.get(); }

    const std::shared_ptr<ArcanistComponent> &activeArcanistPtr() const {
        return activeArcanist_;
    }
    void setActiveArcanist(std::shared_ptr<ArcanistComponent> component) {
        activeArcanist_ = std::move(component);
    }

    // Clear level effects while keeping the persistent Arcanist.
    void resetActiveArcanistToBase() {
        if (baseArcanist_ != nullptr) {
            activeArcanist_ = std::make_shared<BaseArcanistComponent>(baseArcanist_);
        } else {
            activeArcanist_.reset();
        }
    }

    int effectivePwr() const {
        if (activeArcanist_ != nullptr) {
            return activeArcanist_->effPwr();
        }
        return baseArcanist_ != nullptr ? baseArcanist_->pwr() : 0;
    }
    int effectiveWr() const {
        if (activeArcanist_ != nullptr) {
            return activeArcanist_->effWr();
        }
        return baseArcanist_ != nullptr ? baseArcanist_->wr() : 0;
    }

    Level *level() { return level_.get(); }
    const Level *level() const { return level_.get(); }
    void setLevel(std::unique_ptr<Level> level) { level_ = std::move(level); }

    // Borrowed layout must outlive this Model.
    void setLayout(const LoadedLayout *layout) { layout_ = layout; }
    const LoadedLayout *layout() const { return layout_; }

    void buildCurrentLevel();

    // Current level number, 1..kChamberCount-worth of levels (starts at 1).
    int levelNumber() const { return levelNumber_; }
    void setLevelNumber(int n) { levelNumber_ = n; }

    Position arcanistPosition() const { return arcanistPos_; }
    void setArcanistPosition(const Position &p) { arcanistPos_ = p; }

    bool moveArcanist(Direction d);

    bool moveArcanist(const std::string &token);

    bool useScroll(Direction d);

    bool useScroll(const std::string &token);

    bool descend();

    bool onStairway() const;

    bool tryDescend();

    bool handleAttack(Direction d);

    int spectreDamageIfHit(const Spectre &spectre) const;

    bool resolveSpectreAttack(Spectre &spectre);

    bool isHostile(const Spectre &spectre) const;

    void advanceSpectres();

    int runeFragmentTotal() const { return runeFragmentTotal_; }
    void setRuneFragmentTotal(int total) { runeFragmentTotal_ = total; }
    void addRuneFragments(int amount);

    static constexpr int kPromotionRfThreshold = 10;

    bool promoted() const { return promoted_; }

    void setBonusEnabled(bool v) { bonusEnabled_ = v; }
    bool bonusEnabled() const { return bonusEnabled_; }

    bool lichAggroTriggered() const { return lichAggroTriggered_; }
    void setLichAggroTriggered(bool v) { lichAggroTriggered_ = v; }

    bool carryingCipherGem() const { return carryingCipherGem_; }
    void setCarryingCipherGem(bool v) { carryingCipherGem_ = v; }

    bool cipherGemDropped() const { return cipherGemDropped_; }
    Position cipherGemDropPosition() const { return cipherGemDropPos_; }

    // Record that the Cipher Gem was dropped on tile `p` (carrier died there).
    void setCipherGemDrop(const Position &p) {
        cipherGemDropped_ = true;
        cipherGemDropPos_ = p;
    }
    void clearCipherGemDrop() {
        cipherGemDropped_ = false;
        cipherGemDropPos_ = Position{0, 0};
    }

    void revealStairwayIfCarryingGem();

    bool carryingAegisCloak() const { return carryingAegisCloak_; }
    void setCarryingAegisCloak(bool v) { carryingAegisCloak_ = v; }

    bool aegisSpawnedThisRun() const { return aegisSpawnedThisRun_; }
    void setAegisSpawnedThisRun(bool v) { aegisSpawnedThisRun_ = v; }

    // The level number on which the Aegis Cloak spawned this run (0 = none yet).
    int aegisLevel() const { return aegisLevel_; }
    void setAegisLevel(int n) { aegisLevel_ = n; }

    void markScrollIdentified(ScrollType type) {
        identifiedScrolls_.insert(type);
    }
    bool isScrollIdentified(ScrollType type) const {
        return identifiedScrolls_.count(type) != 0;
    }

    void markScrollIdentified(int scrollType) {
        markScrollIdentified(static_cast<ScrollType>(scrollType));
    }
    bool isScrollIdentified(int scrollType) const {
        return isScrollIdentified(static_cast<ScrollType>(scrollType));
    }

    RunState runState() const { return runState_; }
    void setRunState(RunState state) { runState_ = state; }

    bool isRunLost() const { return runState_ == RunState::Lost; }

    void checkLoss();

    void maybePromote();

    bool hasScore() const { return runState_ == RunState::Won; }

    int finalScore() const;

private:
    std::shared_ptr<Arcanist> baseArcanist_;
    std::shared_ptr<ArcanistComponent> activeArcanist_;
    std::unique_ptr<Level> level_;
    int levelNumber_;
    int runeFragmentTotal_;
    bool promoted_ = false;
    bool bonusEnabled_ = false; // bonus features gate (Prestige promotion)
    bool lichAggroTriggered_;
    bool carryingCipherGem_;
    bool carryingAegisCloak_;
    bool aegisSpawnedThisRun_;
    int aegisLevel_;
    bool cipherGemDropped_;
    Position cipherGemDropPos_;  // tile the dropped Cipher Gem landed on
    Position arcanistPos_;
    std::set<ScrollType> identifiedScrolls_;
    RunState runState_;
    const LoadedLayout *layout_ = nullptr;
};

} // namespace runevault

#endif // RUNEVAULT_MODEL_H
