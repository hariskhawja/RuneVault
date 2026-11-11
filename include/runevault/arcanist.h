#ifndef RUNEVAULT_ARCANIST_H
#define RUNEVAULT_ARCANIST_H

#include <memory>

#include "runevault/arcanist_class.h"

namespace runevault {

// The player character's stat block. Values are plain ints; every mutator
// keeps the invariants "FP in [0, maxFp]" and "pwr_/wr_ >= 0".
class Arcanist {
public:
    Arcanist(int fp, int pwr, int wr);

    Arcanist(int fp, int pwr, int wr,
             std::unique_ptr<ArcanistClassStrategy> strategy);

    // Stat accessors.
    int fp() const { return fp_; }
    int maxFp() const { return maxFp_; }
    int pwr() const { return pwr_; }
    int wr() const { return wr_; }

    bool isDead() const { return fp_ == 0; }

    // Subtract `amount` Focus Points, flooring at 0. A negative `amount` is a
    // no-op (it never heals).
    void reduceFp(int amount);

    void applyDamage(int rawDamage);

    void healUpTo(int amount);

    void loseUpTo(int amount);

    void addPwr(int delta);
    void addWr(int delta);

    void applyPromotionBonus(int fpBonus, int pwrBonus, int wrBonus);

    bool hasStrategy() const { return strategy_ != nullptr; }

    const ArcanistClassStrategy& strategy() const { return *strategy_; }

    void setStrategy(std::unique_ptr<ArcanistClassStrategy> strategy) {
        strategy_ = std::move(strategy);
    }

private:
    int fp_;
    int maxFp_;
    int pwr_;
    int wr_;
    std::unique_ptr<ArcanistClassStrategy> strategy_; // null until attached
};

} // namespace runevault

#endif // RUNEVAULT_ARCANIST_H
