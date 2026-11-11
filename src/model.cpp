#include "runevault/model.h"

#include "runevault/arcanist_component.h"
#include "runevault/combat.h"
#include "runevault/combat_visitor.h"
#include "runevault/guarded_item.h"
#include "runevault/item.h"
#include "runevault/layout_loader.h"
#include "runevault/notification_log.h"
#include "runevault/promotion_factory.h"
#include "runevault/scroll_type.h"
#include "runevault/spawner.h"
#include "shuffle.h"

#include <memory>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace runevault {

namespace {

enum class EffectAxis { FP, PWR, WR };

struct ScrollEffect {
    EffectAxis axis;
    int baseDelta;
};

ScrollEffect scrollEffectFor(ScrollType type) {
    switch (type) {
    case ScrollType::AttuneFocus: return ScrollEffect{EffectAxis::FP, +10};
    case ScrollType::DrainFocus:  return ScrollEffect{EffectAxis::FP, -10};
    case ScrollType::SurgePower:  return ScrollEffect{EffectAxis::PWR, +5};
    case ScrollType::SapPower:    return ScrollEffect{EffectAxis::PWR, -5};
    case ScrollType::FortifyWard: return ScrollEffect{EffectAxis::WR, +5};
    case ScrollType::ErodeWard:   return ScrollEffect{EffectAxis::WR, -5};
    }
    // Unreachable: every ScrollType is handled above. Return a harmless
    // no-magnitude FP effect to satisfy the compiler.
    return ScrollEffect{EffectAxis::FP, 0};
}

} // namespace

Model::Model()
    : baseArcanist_(nullptr),
      level_(nullptr),
      levelNumber_(1),
      runeFragmentTotal_(0),
      promoted_(false),
      bonusEnabled_(false),
      lichAggroTriggered_(false),
      carryingCipherGem_(false),
      carryingAegisCloak_(false),
      aegisSpawnedThisRun_(false),
      aegisLevel_(0),
      cipherGemDropped_(false),
      cipherGemDropPos_{0, 0},
      arcanistPos_{0, 0},
      runState_(RunState::Playing),
      layout_(nullptr) {}

void Model::addRuneFragments(int amount) {
    if (amount <= 0) {
        return; // never decreases the total through this helper
    }
    runeFragmentTotal_ += amount;
    maybePromote();
}

void Model::maybePromote() {
    if (!bonusEnabled_) {
        return;
    }
    // Guards: need a base Arcanist that has a class Strategy and has not already
    // promoted this run; and the RF total must have reached the threshold.
    if (baseArcanist_ == nullptr || promoted_ || !baseArcanist_->hasStrategy()) {
        return;
    }
    if (runeFragmentTotal_ < kPromotionRfThreshold) {
        return;
    }
    const std::string before = baseArcanist_->strategy().className();
    PromotionFactory::promote(*baseArcanist_);
    promoted_ = true;
    notify("Your Arcanist ascends! " + before + " becomes " +
           baseArcanist_->strategy().className() + ".");
}

int Model::finalScore() const {
    if (runState_ != RunState::Won) {
        return 0;
    }
    if (baseArcanist_ && baseArcanist_->hasStrategy()) {
        return baseArcanist_->strategy().scoreModifier(runeFragmentTotal_);
    }
    return runeFragmentTotal_;
}

bool Model::moveArcanist(Direction d) {
    // Movement is validated against the current level's board; with no level
    // set there is nowhere to move to.
    if (level_ == nullptr) {
        return false;
    }

    const Board &board = level_->board();
    const Position target = Geometry::step(arcanistPos_, d);

    if (!board.inBounds(target) || !board.at(target).isWalkable()) {
        return false;
    }

    if (Spectre *occupant = level_->spectreAt(target)) {
        if (!occupant->isDead()) {
            notify("The " + spectreTypeName(occupant->type()) +
                   " blocks your path -- attack it with a <dir>.");
            return false;
        }
    }

    arcanistPos_ = target;

    notify("You move " + directionName(d) + ".");

    if (Item *item = level_->itemAt(arcanistPos_)) {
        if (item->onWalkOver(*this)) {
            level_->removeItemAt(arcanistPos_);
        }
    }

    revealStairwayIfCarryingGem();

    if (carryingCipherGem_ && onStairway()) {
        tryDescend();
    }

    // The return value means "did the Arcanist move" — still true after a
    // fragment pickup or a stairway descent.
    return true;
}

bool Model::handleAttack(Direction d) {

    if (baseArcanist_ == nullptr || level_ == nullptr) {
        return false; // no-op — nothing to resolve
    }

    const Position target = Geometry::step(arcanistPos_, d);
    Spectre *spectre = level_->spectreAt(target);

    if (spectre == nullptr || spectre->isDead()) {
        return false;
    }

    const int damage = computeDamage(effectivePwr(), spectre->wr());
    spectre->takeDamage(damage);

    const SpectreType hitType = spectre->type();
    const int remainingFp = spectre->fp();
    notify("You attack the " + spectreTypeName(hitType) + " for " +
           std::to_string(damage) + " damage (" + spectreTypeName(hitType) +
           " FP: " + std::to_string(remainingFp) + ").");

    CombatVisitor visitor(*this);
    spectre->accept(visitor);

    if (spectre->isDead()) {
        notify("You defeat the " + spectreTypeName(hitType) + "!");

        if (hitType == SpectreType::Lich) {
            const Position dropTile = spectre->position();
            auto cache = std::unique_ptr<RuneFragment>(
                new RuneFragment(FragmentGrade::LichCache));
            cache->setPosition(dropTile);
            level_->addItem(std::move(cache));
            notify("The Lich's cache spills onto the floor -- walk over it to "
                   "collect it.");
        } else {
            const int runeDrop = spectre->runeDrop();
            addRuneFragments(runeDrop);
            if (runeDrop > 0) {
                notify("You gain " + std::to_string(runeDrop) +
                       " Rune Fragment(s).");
            }
        }

        if (hitType == SpectreType::VaultAnchor) {
            notify("The Vault Anchor falls -- its guarded treasure is "
                   "unlocked.");
        }

        if (spectre->carriesCipherGem()) {
            Position dropTile = spectre->position();

            // If the tile already has an item (e.g. the Lich cache), find an
            // adjacent free floor tile for the gem.
            if (level_->itemAt(dropTile) != nullptr) {
                const Board &board = level_->board();
                bool found = false;
                for (const Position &adj : Geometry::neighbours(dropTile)) {
                    if (board.inBounds(adj) && board.at(adj).canHost() &&
                        level_->itemAt(adj) == nullptr &&
                        level_->spectreAt(adj) == nullptr) {
                        dropTile = adj;
                        found = true;
                        break;
                    }
                }
                (void)found;
            }

            setCipherGemDrop(dropTile);

            auto gem = std::unique_ptr<CipherGem>(new CipherGem());
            gem->setPosition(dropTile);
            level_->addItem(std::move(gem));

            notify("The " + spectreTypeName(hitType) + " drops the Cipher "
                   "Gem!");
        }

        // The corpse has yielded its drop; clear it from the level.
        level_->removeDeadSpectres();
    }

    return true; // an attack resolved — the turn is consumed
}

int Model::spectreDamageIfHit(const Spectre &spectre) const {
    // Safe in isolation: with no Arcanist there is no defender, so a hit would
    // inflict nothing.
    if (baseArcanist_ == nullptr) {
        return 0;
    }

    const int base = computeDamage(spectre.pwr(), effectiveWr());

    const bool isWarden = false;
    return applyDamageTakenOrdering(base, isWarden, carryingAegisCloak());
}

bool Model::resolveSpectreAttack(Spectre &spectre) {
    // Guard: nothing to attack without an Arcanist.
    if (baseArcanist_ == nullptr) {
        return false;
    }

    if (rng::chance(0.5)) {
        notify("The " + spectreTypeName(spectre.type()) +
               " attacks but misses.");
        return false; // missed — no damage, but this was still the Spectre's action
    }

    const int damage = spectreDamageIfHit(spectre);
    baseArcanist_->applyDamage(damage);

    notify("The " + spectreTypeName(spectre.type()) + " hits you for " +
           std::to_string(damage) + " damage (FP: " +
           std::to_string(baseArcanist_->fp()) + "/" +
           std::to_string(baseArcanist_->maxFp()) + ").");

    checkLoss();

    if (baseArcanist_->isDead()) {
        notify("You have fallen.");
    }

    return true; // the attack hit and dealt damage
}

bool Model::isHostile(const Spectre &spectre) const {
    switch (spectre.type()) {
    case SpectreType::VaultAnchor:
        return Geometry::isAdjacent(arcanistPos_, spectre.guardedItemPosition());
    case SpectreType::Lich:
        return lichAggroTriggered_;
    default:
        // Wraith, Banshee, Revenant, Shade, Specter Lord: always hostile.
        return spectre.isDefaultHostile();
    }
}

void Model::advanceSpectres() {
    if (level_ == nullptr) {
        return; // no-op
    }

    const Board &board = level_->board();

    std::vector<Spectre *> ordered;
    ordered.reserve(level_->spectres().size());
    for (const auto &s : level_->spectres()) {
        ordered.push_back(s.get());
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const Spectre *a, const Spectre *b) {
                  const Position pa = a->position();
                  const Position pb = b->position();
                  if (pa.row != pb.row) {
                      return pa.row < pb.row; // lower row first (top-to-bottom)
                  }
                  return pa.col < pb.col; // then lower col first (left-to-right)
              });

    for (Spectre *spectre : ordered) {
        if (spectre->isDead()) {
            continue;
        }

        if (isHostile(*spectre) &&
            Geometry::isAdjacent(spectre->position(), arcanistPos_)) {
            resolveSpectreAttack(*spectre);
            continue;
        }

        if (!spectre->isMobile()) {
            continue;
        }

        const std::vector<Position> inChamber = board.floorNeighboursInChamber(
            spectre->position(), spectre->spawnChamber());

        std::vector<Position> candidates;
        candidates.reserve(inChamber.size());
        for (const Position &tile : inChamber) {
            if (tile == arcanistPos_) {
                continue; // do not step onto the Arcanist
            }
            Spectre *occupant = level_->spectreAt(tile);
            if (occupant != nullptr && occupant != spectre &&
                !occupant->isDead()) {
                continue; // occupied by another living Spectre
            }
            candidates.push_back(tile);
        }

        if (!candidates.empty()) {
            spectre->moveTo(rng::pick(candidates));
        }
    }
}

void Model::revealStairwayIfCarryingGem() {
    if (carryingCipherGem_ && level_ != nullptr) {
        level_->setStairwayRevealed(true);
    }
}

bool Model::moveArcanist(const std::string &token) {
    Direction d;
    try {
        d = Geometry::parse(token);
    } catch (const std::invalid_argument &) {
        return false;
    }
    return moveArcanist(d);
}

bool Model::useScroll(Direction d) {
    if (level_ == nullptr || baseArcanist_ == nullptr) {
        return false;
    }

    const Position target = Geometry::step(arcanistPos_, d);

    // A scroll is consumed only if one actually sits on the target tile.
    Scroll *scroll = dynamic_cast<Scroll *>(level_->itemAt(target));
    if (scroll == nullptr) {
        return false;
    }

    const ScrollType type = scroll->scrollType();

    const ScrollEffect effect = scrollEffectFor(type);

    int delta = effect.baseDelta;
    if (baseArcanist_->hasStrategy()) {
        delta = baseArcanist_->strategy().scrollEffectModifier(type, effect.baseDelta);
    }

    switch (effect.axis) {
    case EffectAxis::FP:
        if (delta >= 0) {
            baseArcanist_->healUpTo(delta);
        } else {
            baseArcanist_->loseUpTo(-delta);
        }
        break;
    case EffectAxis::PWR:
        if (delta >= 0) {
            activeArcanist_ = std::make_shared<SurgePower>(activeArcanist_, delta);
        } else {
            activeArcanist_ = std::make_shared<SapPower>(activeArcanist_, -delta);
        }
        break;
    case EffectAxis::WR:
        if (delta >= 0) {
            activeArcanist_ = std::make_shared<FortifyWard>(activeArcanist_, delta);
        } else {
            activeArcanist_ = std::make_shared<ErodeWard>(activeArcanist_, -delta);
        }
        break;
    }

    checkLoss();

    // Remove the consumed scroll from the board (it is used up).
    level_->removeItemAt(target);

    markScrollIdentified(type);

    notify("You use " + scrollTypeName(type) + ".");

    return true; // a scroll was consumed — the turn is consumed
}

bool Model::descend() {
    if (!carryingCipherGem_) {
        return false;
    }

    if (levelNumber_ >= kChamberCount) {
        return false;
    }

    resetActiveArcanistToBase();

    setCarryingCipherGem(false);
    clearCipherGemDrop();

    setLevelNumber(levelNumber_ + 1);

    buildCurrentLevel();

    notify("You descend to level " + std::to_string(levelNumber_) + ".");

    return true;
}

void Model::buildCurrentLevel() {
    Spawner spawner;

    if (layout_ != nullptr) {
        std::unique_ptr<Level> fileLevel =
            LayoutLoader::buildLevel(*layout_, levelNumber_ - 1);
        if (fileLevel != nullptr) {
            setLevel(std::move(fileLevel));
            spawner.populateActorsOnly(*this);
            return;
        }
    }

    setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    spawner.populate(*this);
}

bool Model::onStairway() const {
    // The geometric predicate: is the Arcanist standing on the stairway tile?
    // With no Level there is no stairway to stand on.
    if (level_ == nullptr) {
        return false;
    }
    return arcanistPos_ == level_->stairway();
}

bool Model::tryDescend() {
    if (!onStairway() || !carryingCipherGem_) {
        return false;
    }

    if (levelNumber_ >= kChamberCount) {
        setRunState(RunState::Won);
        notify("You escaped the vault -- victory!");
        return true;
    }

    return descend();
}

void Model::checkLoss() {
    if (baseArcanist_ != nullptr && baseArcanist_->isDead()) {
        runState_ = RunState::Lost;
    }
}

bool Model::useScroll(const std::string &token) {
    Direction d;
    try {
        d = Geometry::parse(token);
    } catch (const std::invalid_argument &) {
        return false;
    }
    return useScroll(d);
}

} // namespace runevault
