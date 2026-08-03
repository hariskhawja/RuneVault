// promotion_factory.cpp
// RuneVault — the Prestige PROMOTION factory (bonus feature; Requirements
// 14.1/14.2). See include/runevault/promotion_factory.h for the contract and
// the base-class -> promotion mapping table.

#include "runevault/promotion_factory.h"

#include <memory>

#include "runevault/arcanist.h"
#include "runevault/arcanist_class.h"

namespace runevault {

void PromotionFactory::promote(Arcanist& a) {
    // No strategy attached -> nothing to promote (guard for stat-only Arcanists).
    if (!a.hasStrategy()) {
        return;
    }

    // Dispatch on the CURRENT class key. Each base class swaps to its promoted
    // Strategy and buffs FP/Pwr/WR (Requirement 14.1 swap + "all stats buffed").
    switch (a.strategy().classKey()) {
    case 's': // Sage -> Archmage
        a.setStrategy(
            std::unique_ptr<ArcanistClassStrategy>(new ArchmageStrategy()));
        a.applyPromotionBonus(30, 6, 6);
        break;
    case 'h': // Hexblade -> Warlock
        a.setStrategy(
            std::unique_ptr<ArcanistClassStrategy>(new WarlockStrategy()));
        a.applyPromotionBonus(20, 10, 4);
        break;
    case 'w': // Warden -> Bastion
        a.setStrategy(
            std::unique_ptr<ArcanistClassStrategy>(new BastionStrategy()));
        a.applyPromotionBonus(40, 4, 8);
        break;
    case 'v': // Voidwalker -> Voidlord
        a.setStrategy(
            std::unique_ptr<ArcanistClassStrategy>(new VoidlordStrategy()));
        a.applyPromotionBonus(20, 10, 4);
        break;
    default:
        // Already promoted (Archmage/Warlock/Bastion/Voidlord keep the base
        // key) or an unknown key — no-op. The Model guards against calling
        // promote() twice, so this branch is defensive.
        return;
    }
}

} // namespace runevault
