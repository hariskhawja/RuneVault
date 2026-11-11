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
        return;
    }
}

} // namespace runevault
