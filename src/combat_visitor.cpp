// combat_visitor.cpp
// RuneVault — CombatVisitor implementation (task 10.2, design DQ3).
//
// See include/runevault/combat_visitor.h for the design rationale, the invoke
// semantics (fires when the Arcanist attacks a Spectre), and the deliberate
// scope: only Requirement 7.7 (Lich aggro) is wired here; Requirement 6.12
// (Vault Anchor -> unlock guarded items) is a documented hook filled in by
// task 13.3.

#include "runevault/combat_visitor.h"

#include "runevault/geometry.h"     // Geometry::isAdjacent (Anchor↔guarded-item radius)
#include "runevault/guarded_item.h" // GuardedItem — Locked->Unlocked on Anchor death (task 13.3)
#include "runevault/level.h"        // Level::items() (walk the guarded items)
#include "runevault/model.h" // full Model definition (forward-declared in the header)

#include <vector> // collectedTiles (Q4 follow-up in-place collection)

namespace runevault {

CombatVisitor::CombatVisitor(Model& model) : model_(model) {}

// --- Type-specific combat resolution (double-dispatch targets) --------------

void CombatVisitor::visit(Wraith& /*s*/) {
    // No-op for this task. Design "e.g." flavor (Wraith FP drain) is optional
    // and not required by Requirements 6.12/7.7. TODO: add drain here if the
    // ability is implemented later.
}

void CombatVisitor::visit(Banshee& /*s*/) {
    // No-op for this task. Optional flavor (temporary Warding reduction) —
    // not required by 6.12/7.7.
}

void CombatVisitor::visit(Revenant& /*s*/) {
    // No-op for this task. No special ability required by 6.12/7.7.
}

void CombatVisitor::visit(Shade& /*s*/) {
    // No-op for this task. Optional flavor (rune-fragment theft) — not required
    // by 6.12/7.7.
}

void CombatVisitor::visit(Lich& /*s*/) {
    // Requirement 7.7 / DD-2: attacking ANY Lich trips the single run-scoped
    // Lich aggro flag, making that Lich, every already-spawned Lich, and every
    // future Lich permanently hostile until a restart. The visitor runs on the
    // attack (Model::handleAttack), so this models "the Lich was attacked"
    // regardless of whether the hit was lethal.
    model_.setLichAggroTriggered(true);
}

void CombatVisitor::visit(VaultAnchor& s) {
    // Requirements 9.2/9.5 (design "CombatVisitor::visit(VaultAnchor&) on Anchor
    // death"): a defeated Vault Anchor unlocks the guarded item(s) it protects
    // (transition Locked -> Unlocked). Like visit(Lich&), the visitor fires on
    // EVERY attack (Model::handleAttack), so guard on death: a non-lethal hit
    // must leave the guards Locked. Only unlock once the Anchor is actually dead.
    if (!s.isDead()) {
        return;
    }

    Level* level = model_.level();
    if (level == nullptr) {
        return; // no level -> nothing to unlock (safe guard)
    }

    // ASSOCIATION (documented): the Spawner (task 15) places a Vault Anchor
    // within a 1-block radius of the item(s) it guards (Requirements 4.6/9.6).
    // Given the current structures — the Anchor does not yet hold pointers to
    // its guarded items — the cleanest, forward-compatible association is
    // geometric: on the Anchor's death, unlock every GuardedItem on the level
    // that sits on the Anchor's own tile or within its 1-block radius. This is
    // exactly the region the Spawner will place guarded items in, so task 15
    // needs no change here.
    const Position anchorPos = s.position();
    const Position arcanistPos = model_.arcanistPosition();
    // Collect the tiles of any guarded items that get collected in-place so we
    // can detach them from the Level AFTER the loop (mutating level->items()
    // while iterating it would invalidate the iterator).
    std::vector<Position> collectedTiles;
    for (const auto& itemPtr : level->items()) {
        GuardedItem* guarded = dynamic_cast<GuardedItem*>(itemPtr.get());
        if (guarded == nullptr) {
            continue; // only guarded Major Items respond to Anchor death
        }
        const Position itemPos = guarded->position();
        if (itemPos == anchorPos || Geometry::isAdjacent(anchorPos, itemPos)) {
            guarded->onAnchorDeath(); // Locked -> Unlocked (Requirement 9.5)

            // Piazza Q4 Follow-up: the Coffer "is only added to the Arcanist's
            // total once the guarding Vault Anchor has been defeated" — that
            // condition is unconditional, not "defeated AND then walked over
            // again." So if the Arcanist is ALREADY STANDING ON the guarded
            // item's tile at the moment its Anchor dies, collect it IMMEDIATELY
            // rather than requiring the player to step off and back on.
            if (itemPos == arcanistPos && guarded->tryCollect(model_)) {
                collectedTiles.push_back(itemPos);
            }
        }
    }
    // Physically detach any items collected in-place (mirrors the walk-over
    // path in Model::moveArcanist, where the caller removes the item on a
    // successful collect).
    for (const Position& tile : collectedTiles) {
        level->removeItemAt(tile);
    }
}

void CombatVisitor::visit(SpecterLord& /*s*/) {
    // No-op for this task. No special ability required by 6.12/7.7.
}

} // namespace runevault
