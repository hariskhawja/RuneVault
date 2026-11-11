#include "runevault/combat_visitor.h"

#include "runevault/geometry.h"
#include "runevault/guarded_item.h"
#include "runevault/level.h"
#include "runevault/model.h"

#include <vector>

namespace runevault {

CombatVisitor::CombatVisitor(Model& model) : model_(model) {}

void CombatVisitor::visit(Wraith& /*s*/) {
}

void CombatVisitor::visit(Banshee& /*s*/) {
}

void CombatVisitor::visit(Revenant& /*s*/) {
}

void CombatVisitor::visit(Shade& /*s*/) {
}

void CombatVisitor::visit(Lich& /*s*/) {
    model_.setLichAggroTriggered(true);
}

void CombatVisitor::visit(VaultAnchor& s) {
    if (!s.isDead()) {
        return;
    }

    Level* level = model_.level();
    if (level == nullptr) {
        return; // no level -> nothing to unlock (safe guard)
    }

    const Position anchorPos = s.position();
    const Position arcanistPos = model_.arcanistPosition();
    std::vector<Position> collectedTiles;
    for (const auto& itemPtr : level->items()) {
        GuardedItem* guarded = dynamic_cast<GuardedItem*>(itemPtr.get());
        if (guarded == nullptr) {
            continue; // only guarded Major Items respond to Anchor death
        }
        const Position itemPos = guarded->position();
        if (itemPos == anchorPos || Geometry::isAdjacent(anchorPos, itemPos)) {
            guarded->onAnchorDeath();

            if (itemPos == arcanistPos && guarded->tryCollect(model_)) {
                collectedTiles.push_back(itemPos);
            }
        }
    }
    // Defer removal so the item iteration above remains valid.
    for (const Position& tile : collectedTiles) {
        level->removeItemAt(tile);
    }
}

void CombatVisitor::visit(SpecterLord& /*s*/) {
}

} // namespace runevault
