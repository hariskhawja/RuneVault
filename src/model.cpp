// model.cpp
// RuneVault — the Model skeleton: aggregated run-scoped state (task 5.4).
//
// See include/runevault/model.h for the component contract. This is a state
// container only; most accessors/mutators are inline in the header, so the
// implementation here is limited to the default constructor and the
// `addRuneFragments` accumulation helper.

#include "runevault/model.h"

#include "runevault/arcanist_component.h" // scroll Decorator chain (task 12.1)
#include "runevault/combat.h"
#include "runevault/combat_visitor.h" // CombatVisitor — type-specific attack effects (task 10.2)
#include "runevault/guarded_item.h"   // CipherGem drop + guarded-item walk-over (task 13.3)
#include "runevault/item.h"           // Scroll — the `u <dir>` target (task 12.2)
#include "runevault/layout_loader.h"  // LoadedLayout + buildLevel — file-driven levels (task 19.1)
#include "runevault/notification_log.h" // NotificationLog / notify — per-turn message log
#include "runevault/promotion_factory.h" // PromotionFactory — Prestige promotion (task 14.x)
#include "runevault/scroll_type.h"    // ScrollType (task 12.2)
#include "runevault/spawner.h"        // Spawner — repopulate the next level (task 16.1)
#include "shuffle.h"

#include <memory>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace runevault {

namespace {

// Task 12.4: the EFFECT AXIS a scroll acts on. FP is the permanent Focus-Point
// axis (Attune/Drain), PWR the temporary Power axis (Surge/Sap), and WR the
// temporary Ward-Rating axis (Fortify/Erode). The axis is fixed per scroll
// type; only the resulting SIGN (and, for Hexblade, magnitude) is shaped by the
// class Strategy's scrollEffectModifier.
enum class EffectAxis { FP, PWR, WR };

// A scroll's effect axis plus its SIGNED BASE DELTA (magnitude + direction),
// before any class-ability adjustment. Positive scrolls buff; the negative
// trio (Drain/Sap/Erode) debuff. These base magnitudes match task 12.2:
// FP scrolls ±10, stat scrolls ±5.
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
    // Prestige promotion trigger (bonus feature; Requirements 14.1/14.2): every
    // gameplay RF gain funnels through here, so check whether this gain crosses
    // the promotion threshold. maybePromote() is idempotent/once-only.
    maybePromote();
}

void Model::maybePromote() {
    // Bonus-feature gate (project spec: the base game must play WITHOUT bonus
    // features, no recompile). The Prestige promotion is a BONUS feature, so it
    // only ever triggers while bonus is enabled. This is the FIRST guard: with
    // bonus off, reaching the RF threshold never promotes.
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
    // Requirement 14.1: swap the class Strategy to the promoted class and buff
    // all stats (PromotionFactory). Requirement 14.2: other core behaviour is
    // preserved. Capture the pre-promotion class name for the notification.
    const std::string before = baseArcanist_->strategy().className();
    PromotionFactory::promote(*baseArcanist_);
    promoted_ = true;
    notify("Your Arcanist ascends! " + before + " becomes " +
           baseArcanist_->strategy().className() + ".");
}

int Model::finalScore() const {
    // Requirement 13.3: a run ending by loss, restart, or quit produces no
    // score. Only a won run (RunState::Won) has a score; everything else
    // (Playing / Lost, or a restart/quit that never reached Won) yields 0.
    if (runState_ != RunState::Won) {
        return 0;
    }
    // Requirement 13.1: on a win the score is the total Rune Fragments
    // collected. Requirement 13.2: the Sage (and its Archmage promotion) applies
    // a +50% bonus, which the class scoreModifier encapsulates; all other
    // classes leave the total unchanged. Guard against a missing Strategy by
    // falling back to the raw total (identity).
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

    // Requirement 5.3: reject moves that leave the playable region or step onto
    // a vault wall, without consuming the turn (position unchanged).
    if (!board.inBounds(target) || !board.at(target).isWalkable()) {
        return false;
    }

    // Occupancy vs. movement (Piazza Q4): the spec's "no two objects share a
    // cell" rule is a SPAWN-time invariant, NOT a movement gate. Per Q4,
    // "nothing in the spec blocks movement onto an item tile; only collection
    // is gated." So the Arcanist may freely walk onto ANY item tile — scrolls,
    // guarded Major Items, and fragments alike — without the move being
    // rejected. Fragments still collect on walk-over (below); scrolls/guarded
    // items are simply occupied. Movement is therefore validated ONLY against
    // cell walkability (above), never against item occupancy.
    //
    // A LIVING Spectre is the one exception that blocks: the Arcanist attacks
    // it with `a <dir>` and never walks onto/through it (Q4 addresses items,
    // not Spectres; standing on a Spectre is nonsensical). A dead Spectre
    // (corpse) does NOT block — in practice handleAttack already removes dead
    // Spectres, so only living ones are present.
    if (Spectre *occupant = level_->spectreAt(target)) {
        if (!occupant->isDead()) {
            notify("The " + spectreTypeName(occupant->type()) +
                   " blocks your path -- attack it with a <dir>.");
            return false;
        }
    }

    // Requirement 5.2: the target is walkable — floor, archway, corridor, or
    // stairway. Move the Arcanist onto it; the turn is consumed.
    arcanistPos_ = target;

    // Notification log: report the successful move. Pushed here (at the moment
    // the move actually happens) so any pickup/descent messages triggered below
    // read in natural chronological order after it.
    notify("You move " + directionName(d) + ".");

    // Requirement 5.4: Rune Fragments are the sole item collected transiently
    // by walking over them — collection is immediate on entry. If the tile the
    // Arcanist just stepped onto holds a RuneFragment, add its value to the RF
    // total and remove it from the level.
    //
    // DD-4 / Requirement 5.5: walking onto a guarded-item tile is ALWAYS
    // allowed — movement is validated above against cell walkability only, never
    // against item occupancy, so the Arcanist may occupy a Shard Coffer / Aegis
    // Cloak tile regardless of whether the guarding Vault Anchor is defeated.
    // Only *collection* of those items is gated (behind the guarded-item State
    // machine, task 13.x). Walk-over is dispatched UNIFORMLY through the virtual
    // Item::onWalkOver — no RTTI/type-switch here: each item performs its own
    // effect + notification and reports whether it was CONSUMED. As the owner,
    // the Model does the physical removal (the item never deletes itself):
    //   * RuneFragment      -> credits its value, announces the pickup, true.
    //   * GuardedItem        -> collects only while Unlocked (dropped Cipher Gem
    //                           lifts the Ward + reveals the stairway; a Locked
    //                           Shard Coffer / Aegis Cloak is a no-op), true iff
    //                           it was actually claimed.
    //   * Scroll / anything else -> no-op, false (left in place; scrolls are
    //                           used via `u <dir>`).
    if (Item *item = level_->itemAt(arcanistPos_)) {
        if (item->onWalkOver(*this)) {
            level_->removeItemAt(arcanistPos_);
        }
    }

    // Requirements 2.6/2.7: reveal the stairway as soon as the Cipher Gem is
    // carried. Idempotent and a no-op while the gem is not carried.
    revealStairwayIfCarryingGem();

    // Task 16.3 (Requirements 10.1/10.2/10.3/10.4): stairway walk-over is the
    // descent/win trigger. When this successful move ends on the stairway tile
    // WHILE carrying the current level's Cipher Gem, invoke tryDescend() to
    // advance (levels 1..4) or win (level 5). tryDescend() itself re-checks
    // onStairway() && carryingCipherGem(), so gating here is a fast-path guard,
    // not the source of truth. DECISION: descent is triggered from moveArcanist
    // (rather than deferred to the Controller) to keep task 16.3 self-contained
    // and directly testable via a single move. Re-entrancy is safe: on a levels
    // 1..4 descent, descend() builds a fresh level with the stairway hidden, the
    // gem un-carried, and the Arcanist repositioned off the stairway by the
    // Spawner, so tryDescend() cannot fire a second time from this call. Note we
    // do NOT touch arcanistPos_ after this point — descend() has already moved
    // the Arcanist — avoiding any use-after-move on the old position.
    if (carryingCipherGem_ && onStairway()) {
        tryDescend();
    }

    // The return value means "did the Arcanist move" — still true after a
    // fragment pickup or a stairway descent.
    return true;
}

bool Model::handleAttack(Direction d) {
    // Requires both an Arcanist to attack with and a Level to hold Spectres.
    if (baseArcanist_ == nullptr || level_ == nullptr) {
        return false; // no-op — nothing to resolve
    }

    // Requirement 6.1: the target is the single cell one step in direction `d`,
    // which is always immediately adjacent (Chebyshev distance 1).
    const Position target = Geometry::step(arcanistPos_, d);
    Spectre *spectre = level_->spectreAt(target);

    // Requirement 6.3: no adjacent Spectre in that direction (or only a corpse)
    // means the command is a no-op and the turn is NOT consumed.
    if (spectre == nullptr || spectre->isDead()) {
        return false;
    }

    // Requirement 6.2: the Arcanist always hits, reducing the Spectre's FP by
    // ceil( (100 / (100 + WR_defender)) * Pwr_attacker ). Task 12.1: use the
    // EFFECTIVE Power from the decorator chain so active Surge/Sap scroll effects
    // shape combat (effectivePwr() == base pwr() when no decorators are active).
    const int damage = computeDamage(effectivePwr(), spectre->wr());
    spectre->takeDamage(damage);

    // Notification log: capture the Spectre's type and post-hit FP NOW, before
    // any death handling below removes the corpse (removeDeadSpectres). Reading
    // these after removal would be a use-after-free.
    const SpectreType hitType = spectre->type();
    const int remainingFp = spectre->fp();
    notify("You attack the " + spectreTypeName(hitType) + " for " +
           std::to_string(damage) + " damage (" + spectreTypeName(hitType) +
           " FP: " + std::to_string(remainingFp) + ").");

    // Task 10.2 / design DQ3: resolve type-specific combat effects via the
    // CombatVisitor (double dispatch on the concrete Spectre type). This fires
    // AFTER damage is applied so death-triggered effects can key off isDead():
    //   * visit(Lich&) trips the global Lich aggro flag on ANY attack
    //     (Requirement 7.7), whether the hit was lethal or not.
    //   * visit(VaultAnchor&) unlocks the guarded item(s) the Anchor protects
    //     but ONLY once the Anchor is dead (Requirements 9.2/9.5, task 13.3).
    // TODO(task 12.1): may pass the decorated ArcanistComponent to the visitor.
    CombatVisitor visitor(*this);
    spectre->accept(visitor);

    // Task 7.8: award drops for a Spectre this hit slew, then remove the corpse.
    // Task 7.5 deliberately left the slain Spectre in place so this step can
    // read its drop while it still exists — so the drop MUST be awarded BEFORE
    // removeDeadSpectres() runs below.
    if (spectre->isDead()) {
        // Notification log: the Spectre was defeated by this hit.
        notify("You defeat the " + spectreTypeName(hitType) + "!");

        // Requirements 6.10/6.11 + Piazza Q13: award the Spectre's Rune
        // Fragment drop. There are two distinct paths:
        //
        //   * A defeated LICH does NOT auto-credit its cache. Per Q13 the Lich
        //     cache "is dropped rather than auto-collected" — it appears as a
        //     PHYSICAL RuneFragment (FragmentGrade::LichCache, worth 4) on the
        //     Lich's own tile, and the Arcanist must walk over it to collect,
        //     exactly like the Cipher Gem. So we place a fragment item here
        //     instead of calling addRuneFragments.
        //   * Every OTHER type auto-credits runeDrop() immediately (Requirement
        //     6.10): a generic defeat = 1 RF; a Vault Anchor overrides
        //     runeDrop() to 0 (no auto-credit — its treasure is the guarded
        //     Coffer, collected separately).
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

        // Notification log: a defeated Vault Anchor lifts the Ward on the
        // treasure it guarded (the CombatVisitor performs the actual unlock).
        if (hitType == SpectreType::VaultAnchor) {
            notify("The Vault Anchor falls -- its guarded treasure is "
                   "unlocked.");
        }

        // Requirement 6.12: a defeated carrier drops the Cipher Gem on its tile.
        // Task 13.3 wires the ACTUAL item: place a CipherGem Item (which starts
        // Unlocked, Requirement 9.3) on the carrier's tile so walking onto it
        // lifts the Ward and reveals the stairway (Requirements 9.2/9.3/10.1/
        // 10.2). The model-side drop record (setCipherGemDrop) is retained as a
        // lightweight "a gem is on the floor here" marker for status/reporting.
        // Only the (at most one) carrier per level triggers this.
        //
        // EDGE CASE (Piazza Q13 follow-up): if the carrier is a Lich, the Lich
        // cache item was already placed on the Lich's tile above. Dropping the
        // Cipher Gem on the same cell would violate "no two objects may ever
        // share a space." Resolution: if the Lich's tile is already occupied by
        // an item, place the Cipher Gem on the nearest unoccupied adjacent floor
        // tile instead (the piazza answer allows either item to be bumped; here
        // we bump the gem since the cache was placed first).
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
                // If no adjacent tile is free (extremely unlikely), fall back to
                // the original tile — it still works gameplay-wise, just briefly
                // stacks until one is collected.
                (void)found;
            }

            setCipherGemDrop(dropTile);

            auto gem = std::unique_ptr<CipherGem>(new CipherGem());
            gem->setPosition(dropTile);
            level_->addItem(std::move(gem));

            // Notification log: the slain carrier dropped the Cipher Gem.
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

    // Requirement 6.6: base = ceil( (100 / (100 + WR_Arcanist)) * Pwr_Spectre ).
    // Task 12.1: use the EFFECTIVE Ward Rating from the decorator chain so active
    // Fortify/Erode scroll effects shape incoming damage (effectiveWr() == base
    // wr() when no decorators are active).
    const int base = computeDamage(spectre.pwr(), effectiveWr());

    // Route through the strict damage-taken ordering (task 7.3): Warden -1 then
    // Aegis halving then floor at 1. `isWarden` is false here — the class
    // Strategy (task 9.1) will supply the Warden modifier via
    // strategy.damageTakenModifier(); the Aegis Cloak halving already applies
    // through carryingAegisCloak() (Requirements 6.8/9.8).
    const bool isWarden = false; // task 9.1 supplies this from the class Strategy
    return applyDamageTakenOrdering(base, isWarden, carryingAegisCloak());
}

bool Model::resolveSpectreAttack(Spectre &spectre) {
    // Guard: nothing to attack without an Arcanist.
    if (baseArcanist_ == nullptr) {
        return false;
    }

    // Requirement 6.5: 50% chance to miss. Convention (documented in model.h):
    // a true roll is a MISS and deals no damage; a false roll is a HIT.
    if (rng::chance(0.5)) {
        // Notification log: the Spectre's action was a miss.
        notify("The " + spectreTypeName(spectre.type()) +
               " attacks but misses.");
        return false; // missed — no damage, but this was still the Spectre's action
    }

    // Requirements 6.6 + damage-taken ordering: compute the FP loss for this hit
    // and apply it to the Arcanist (applyDamage floors FP at 0).
    const int damage = spectreDamageIfHit(spectre);
    baseArcanist_->applyDamage(damage);

    // Notification log: the hit landed — report the FP loss and remaining FP.
    notify("The " + spectreTypeName(spectre.type()) + " hits you for " +
           std::to_string(damage) + " damage (FP: " +
           std::to_string(baseArcanist_->fp()) + "/" +
           std::to_string(baseArcanist_->maxFp()) + ").");

    // Requirement 10.5: FP reaching 0 ends the run as a loss (task 16.3 routes
    // this through the shared checkLoss() helper).
    checkLoss();

    // Notification log: a lethal hit ends the run (reportOutcome also prints a
    // final line for the play-again flow).
    if (baseArcanist_->isDead()) {
        notify("You have fallen.");
    }

    return true; // the attack hit and dealt damage
}

bool Model::isHostile(const Spectre &spectre) const {
    // Design "Hostility resolution" pseudocode (Requirements 7.5-7.9).
    switch (spectre.type()) {
    case SpectreType::VaultAnchor:
        // Requirements 7.5/7.6: a Vault Anchor is hostile ONLY while the
        // Arcanist stands within a 1-block radius of the Coffer it guards, and
        // non-hostile otherwise. The guarded tile defaults to the Anchor's own
        // position until the Spawner/guarded-item wiring points it at the real
        // Coffer (task 15.1 / tasks 13.2/13.3).
        return Geometry::isAdjacent(arcanistPos_, spectre.guardedItemPosition());
    case SpectreType::Lich:
        // Requirements 7.7/7.8/7.9: a Lich is hostile iff the global Lich aggro
        // flag is set. The flag is tripped on attack by the CombatVisitor
        // (task 10.2); here we only READ it, so all Liches (already spawned and
        // future) are hostile while set (7.8) and non-hostile while clear (7.9).
        return lichAggroTriggered_;
    default:
        // Wraith, Banshee, Revenant, Shade, Specter Lord: always hostile.
        return spectre.isDefaultHostile();
    }
}

void Model::advanceSpectres() {
    // Requirement 7.1: with no Level there are no Spectres to advance.
    if (level_ == nullptr) {
        return; // no-op
    }

    const Board &board = level_->board();

    // Requirement 7.1: process Spectres row-by-row, left-to-right, moving each
    // AT MOST ONCE. Snapshot the live Spectre pointers into a local vector and
    // sort THAT copy by current (row, col) ascending — the Level's owning
    // vector is left untouched, so ownership/order there is never disturbed.
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
        // Skip corpses (Requirement 7.1: only living Spectres act).
        if (spectre->isDead()) {
            continue;
        }

        // ---------------------------------------------------------------
        // task 10.5: hostile-in-radius attack-instead-of-move decision
        // (Requirements 6.4/7.5-7.9). A Spectre that is hostile (per
        // isHostile) AND within a 1-block radius of the Arcanist ATTACKS this
        // action instead of moving; resolveSpectreAttack handles the 50% miss
        // roll (task 7.6). This runs BEFORE the isMobile() stationary check on
        // purpose: a Vault Anchor is stationary but MUST still attack when the
        // Arcanist is adjacent to the Coffer it guards. A hostile adjacent
        // Anchor attacks here and `continue`s; a non-hostile Anchor falls
        // through to the isMobile() guard below and stays put.
        // ---------------------------------------------------------------
        if (isHostile(*spectre) &&
            Geometry::isAdjacent(spectre->position(), arcanistPos_)) {
            resolveSpectreAttack(*spectre); // 50% miss handled inside (task 7.6)
            continue; // attacked this action instead of moving (Requirement 6.4)
        }

        // Requirement 7.4: Vault Anchors are stationary — never move them.
        if (!spectre->isMobile()) {
            continue;
        }

        // Requirements 7.2/7.3/7.10: the wandering candidate set is the FLOOR
        // neighbours within the 1-block radius that belong to the Spectre's
        // spawn chamber. floorNeighboursInChamber already restricts to the
        // spawn chamber's floor tiles, so a Spectre can NEVER leave its spawn
        // chamber.
        const std::vector<Position> inChamber = board.floorNeighboursInChamber(
            spectre->position(), spectre->spawnChamber());

        // Filter to UNOCCUPIED tiles. Occupancy is checked against CURRENT
        // positions (so a Spectre that already moved this pass blocks its new
        // tile). A tile is occupied if another LIVING Spectre stands on it
        // (the moving Spectre's own tile does not count — it is leaving it) or
        // if it is the Arcanist's tile (never move onto the Arcanist).
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

        // Requirement 7.2: move to a uniformly-random unoccupied candidate. If
        // none remain (boxed in), the Spectre stays put.
        if (!candidates.empty()) {
            spectre->moveTo(rng::pick(candidates));
        }
    }
}

void Model::revealStairwayIfCarryingGem() {
    // Requirements 2.6/2.7: the stairway is hidden (rendered as floor `.`) until
    // the Cipher Gem for the current level is carried, then rendered `\`. Mark
    // the level's stairway revealed only while the gem is carried; the guard on
    // carryingCipherGem_ keeps this from ever leaking the stairway early, and
    // setting an already-true flag makes repeated calls harmless.
    if (carryingCipherGem_ && level_ != nullptr) {
        level_->setStairwayRevealed(true);
    }
}

bool Model::moveArcanist(const std::string &token) {
    Direction d;
    try {
        d = Geometry::parse(token);
    } catch (const std::invalid_argument &) {
        // Unrecognized token: treat as no move / turn not consumed. Full
        // command-token parsing is the Controller's job (task 17.2).
        return false;
    }
    return moveArcanist(d);
}

bool Model::useScroll(Direction d) {
    // Guard: resolving a scroll needs both a Level (to hold the scroll) and an
    // Arcanist (to receive the effect). With either missing this is a no-op and
    // the turn is not consumed.
    if (level_ == nullptr || baseArcanist_ == nullptr) {
        return false;
    }

    // Requirement 8.1: the target is the single cell one step in direction `d`,
    // which is by construction within the Arcanist's 1-block radius.
    const Position target = Geometry::step(arcanistPos_, d);

    // A scroll is consumed only if one actually sits on the target tile.
    Scroll *scroll = dynamic_cast<Scroll *>(level_->itemAt(target));
    if (scroll == nullptr) {
        // Requirement 8.2: no scroll in that direction -> NO-OP, turn not
        // consumed.
        return false;
    }

    const ScrollType type = scroll->scrollType();

    // Resolve the scroll's effect AXIS and SIGNED BASE DELTA (task 12.2 base
    // magnitudes: FP ±10, stat ±5).
    const ScrollEffect effect = scrollEffectFor(type);

    // TASK 12.4 — route the signed base delta through the class Strategy's
    // scrollEffectModifier BEFORE the change is applied / the decorator is
    // constructed (Requirements 8.11, 8.12):
    //   * Hexblade (8.11): doubles the MAGNITUDE while preserving the sign, so a
    //     +5 Surge becomes +10 and a -10 Drain becomes -20 — same axis and
    //     direction, bigger magnitude.
    //   * Voidwalker (8.12): for a NEGATIVE base delta, a 50% chance the sign is
    //     flipped to its positive counterpart (e.g. Sap -5 -> +5 becomes a Power
    //     BUFF, Drain -10 -> +10 becomes a heal). The axis is unchanged; only
    //     the sign flips. A positive base delta is never altered.
    // With no class Strategy attached the delta is left at its base value
    // (identity), preserving the task 12.2 ±10/±5 behaviour for the
    // no-strategy / Sage / Warden / Archmage paths.
    int delta = effect.baseDelta;
    if (baseArcanist_->hasStrategy()) {
        delta = baseArcanist_->strategy().scrollEffectModifier(type, effect.baseDelta);
    }

    // Apply the EFFECTIVE delta by axis + resulting SIGN. A Voidwalker-flipped
    // debuff therefore applies the BUFF decorator (Surge/Fortify) with the
    // flipped magnitude, and a Hexblade buff applies its doubled magnitude —
    // both by reusing the existing parameterized decorators / FP mutators.
    switch (effect.axis) {
    case EffectAxis::FP:
        if (delta >= 0) {
            // Requirement 8.3: permanent restore of up to `delta` FP, capped at
            // maxFp (delta == 0 is a harmless no-op).
            baseArcanist_->healUpTo(delta);
        } else {
            // Requirement 8.4: permanent reduction of up to `-delta` FP, floored
            // at 0.
            baseArcanist_->loseUpTo(-delta);
        }
        break;
    case EffectAxis::PWR:
        if (delta >= 0) {
            // Requirement 8.5: temporary +delta Power for the current level.
            activeArcanist_ = std::make_shared<SurgePower>(activeArcanist_, delta);
        } else {
            // Requirement 8.7: temporary -(-delta) Power (floored at 0).
            activeArcanist_ = std::make_shared<SapPower>(activeArcanist_, -delta);
        }
        break;
    case EffectAxis::WR:
        if (delta >= 0) {
            // Requirement 8.6: temporary +delta Ward Rating for the level.
            activeArcanist_ = std::make_shared<FortifyWard>(activeArcanist_, delta);
        } else {
            // Requirement 8.8: temporary -(-delta) Ward Rating (floored at 0).
            activeArcanist_ = std::make_shared<ErodeWard>(activeArcanist_, -delta);
        }
        break;
    }

    // Requirement 10.5 (task 16.3): a permanent FP drain (Drain Focus) can bring
    // the Arcanist to 0 FP outside combat, which also ends the run as a loss.
    // Route through the shared helper; it is a no-op for buffs and non-FP axes.
    checkLoss();

    // Remove the consumed scroll from the board (it is used up).
    level_->removeItemAt(target);

    // Requirement 8.10 / task 12.6: mark this scroll's TYPE identified
    // ("remembered") on use, keyed by ScrollType. A std::set never un-marks, so
    // the type stays identified for the remainder of the run. Requirement 8.9 /
    // DD-3: this is pure bookkeeping and never changes the board glyph — the
    // Scroll always renders as `?`. The status-area hint that surfaces the
    // learned effects is wired by the Controller later (task 17.x).
    markScrollIdentified(type);

    // Notification log: report the scroll use by its readable type name (the
    // effect magnitude is already applied above; the type message is truthful
    // to what happened).
    notify("You use " + scrollTypeName(type) + ".");

    return true; // a scroll was consumed — the turn is consumed
}

bool Model::descend() {
    // Requirement 10.1 (Ward gating): descent is possible ONLY while the
    // Arcanist carries the current level's Cipher Gem. Without it there is no
    // transition — return false and leave everything unchanged.
    if (!carryingCipherGem_) {
        return false;
    }

    // Descending the stairway on level 5 ends the run as a WIN — that is task
    // 16.3's concern (Requirement 10.4), not this transition primitive. Refuse
    // here so no level 6 is ever built; the level number is left untouched.
    if (levelNumber_ >= kChamberCount) {
        return false;
    }

    // Requirement 10.6: remove all current-level-only scroll effects. Resetting
    // the Decorator chain head back to a fresh BaseArcanistComponent drops every
    // temporary Surge/Fortify/Sap/Erode decorator, so effectivePwr()/
    // effectiveWr() return to the base Arcanist's values.
    resetActiveArcanistToBase();

    // Requirement 10.7: permanent FP changes persist across levels. The base
    // Arcanist (baseArcanist_) is retained AS-IS — we deliberately do not touch
    // its FP/maxFp — so any Attune/Drain scroll effects from prior levels carry
    // forward.

    // Requirement 10.8: reset the carried Cipher Gem state for the new level
    // (the new level's gem must be found again) and clear any dropped-gem
    // marker. The Aegis Cloak is RETAINED: carryingAegisCloak_ is left
    // untouched, so a cloak collected earlier stays with the Arcanist.
    setCarryingCipherGem(false);
    clearCipherGemDrop();

    // Advance to the next level. The run-scoped Aegis bookkeeping
    // (aegisLevel_/aegisSpawnedThisRun_) is intentionally preserved — descent
    // never re-chooses it; the Spawner's placeAegisCloak keys off levelNumber()
    // == aegisLevel().
    setLevelNumber(levelNumber_ + 1);

    // Build the next level and populate it. buildCurrentLevel() picks the source
    // for the new level number: the layout FILE (board + digit items + actors)
    // when a layout is set (task 19.1, Requirement 12.1), or the random default
    // layout otherwise (same fixed layout on every level, Requirement 3.6). The
    // new Level starts with its stairway hidden (Requirements 2.6/2.7).
    buildCurrentLevel();

    // Notification log: report the successful descent to the new level number.
    notify("You descend to level " + std::to_string(levelNumber_) + ".");

    return true;
}

void Model::buildCurrentLevel() {
    // Task 19.1 (Requirements 12.1 / 4.1): the single level-construction entry
    // point shared by the run start (Game::startNewRun) and level descent.
    Spawner spawner;

    if (layout_ != nullptr) {
        // File-driven level (Requirement 12.1): build the board + scroll /
        // fragment / Shard Coffer items straight from the parsed file for the
        // CURRENT level number (levelNumber()-1 is the 0-based file index), then
        // run the Spawner's ACTORS-ONLY pass. That pass places the Arcanist,
        // stairway, spectres, the Cipher Gem carrier, a guarding Vault Anchor for
        // each file Shard Coffer, and (on the run-chosen level) the Aegis Cloak —
        // WITHOUT re-adding random scrolls/fragments, since the file already
        // supplied those. This is the documented HYBRID: the file fixes the map +
        // items, the Spawner fills the actors that a raw map cannot describe.
        std::unique_ptr<Level> fileLevel =
            LayoutLoader::buildLevel(*layout_, levelNumber_ - 1);
        if (fileLevel != nullptr) {
            setLevel(std::move(fileLevel));
            spawner.populateActorsOnly(*this);
            return;
        }
        // Defensive: an out-of-range level index (should not happen — a valid
        // layout carries exactly kLayoutLevelCount == kChamberCount levels)
        // falls through to the random default path so a run never stalls.
    }

    // Default random path (unchanged): a fresh default-layout board populated by
    // the full Spawner pipeline (Arcanist, stairway, scrolls, fragments, Aegis,
    // spectres, carrier).
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
    // Requirements 10.1/10.2: descent happens ONLY by standing on the stairway
    // while carrying the current level's Cipher Gem. If either is false this is
    // a no-op — in particular, walking onto the stairway without the gem has no
    // effect (10.2) and descent is prevented without the gem (10.1).
    if (!onStairway() || !carryingCipherGem_) {
        return false;
    }

    // Requirement 10.4: descending the stairway with the gem on level 5 ends the
    // run as a WIN. We stop here without building a level 6 (descend() would
    // refuse anyway); the level number stays at kChamberCount and finalScore()
    // becomes available via RunState::Won.
    if (levelNumber_ >= kChamberCount) {
        setRunState(RunState::Won);
        // Notification log: the level-5 stairway descent wins the run.
        notify("You escaped the vault -- victory!");
        return true;
    }

    // Requirement 10.3: on levels 1..4 delegate to the descent primitive (task
    // 16.1), which tears down current-level scroll effects, resets the gem,
    // advances the level number, and repopulates the next level.
    return descend();
}

void Model::checkLoss() {
    // Requirement 10.5: FP reaching 0 ends the run as a loss. Only ever sets
    // Lost (monotonic — never clears it), and is a no-op with no Arcanist or a
    // living Arcanist.
    if (baseArcanist_ != nullptr && baseArcanist_->isDead()) {
        runState_ = RunState::Lost;
    }
}

bool Model::useScroll(const std::string &token) {
    Direction d;
    try {
        d = Geometry::parse(token);
    } catch (const std::invalid_argument &) {
        // Unrecognized token: no-op / turn not consumed. Full command-token
        // parsing is the Controller's job (task 17.2).
        return false;
    }
    return useScroll(d);
}

} // namespace runevault
