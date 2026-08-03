// promotion_factory.h
// RuneVault — the Prestige PROMOTION factory (bonus feature; Requirements
// 14.1/14.2, design_strategy.txt DQ1's "second factory").
//
// The design's Arcanist creation uses two factories: ArcanistFactory builds a
// fresh base Arcanist with its base-class Strategy attached (arcanist_factory.h),
// and PromotionFactory here PROMOTES an existing Arcanist in place — swapping
// its class Strategy to the promoted class and buffing all of its stats
// (Requirement 14.1 "upgrade the class ability by swapping the strategy at
// runtime", Requirement 14.2 "preserve other core behaviour"). Each of the four
// base classes maps to exactly one promotion:
//
//   Sage       ('s') -> Archmage  (+30 FP, +6 Pwr, +6 WR)
//   Hexblade   ('h') -> Warlock   (+20 FP, +10 Pwr, +4 WR)
//   Warden     ('w') -> Bastion   (+40 FP, +4 Pwr, +8 WR)
//   Voidwalker ('v') -> Voidlord  (+20 FP, +10 Pwr, +4 WR)
//
// Every promotion buffs FP, Power AND Ward Rating (all stats up), satisfying
// "all stats get buffed up". The trigger (Rune-Fragment threshold, once-only
// guard) lives in the Model (model.cpp).

#ifndef RUNEVAULT_PROMOTION_FACTORY_H
#define RUNEVAULT_PROMOTION_FACTORY_H

namespace runevault {

class Arcanist;

class PromotionFactory {
public:
    // Promote `a` in place: swap its class Strategy to the promoted class based
    // on its current classKey, and buff all stats. No-op if `a` has no strategy
    // or an unrecognized class key. (Design DQ1: the 2nd factory takes an
    // Arcanist and swaps its Strategy.)
    static void promote(Arcanist& a);
};

} // namespace runevault

#endif // RUNEVAULT_PROMOTION_FACTORY_H
