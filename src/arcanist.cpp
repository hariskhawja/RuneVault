// arcanist.cpp
// RuneVault — Arcanist stat container + mutators (task 5.1).
//
// See include/runevault/arcanist.h for the scope of this task. Each mutator
// enforces the class invariants: FP stays within [0, maxFp] and pwr_/wr_ never
// go negative.

#include "runevault/arcanist.h"

namespace runevault {

namespace {
// Clamp a raw stat value to be non-negative.
int floorAtZero(int v) { return v < 0 ? 0 : v; }
} // namespace

Arcanist::Arcanist(int fp, int pwr, int wr)
    : fp_(floorAtZero(fp)),
      maxFp_(floorAtZero(fp)), // Requirement 1.8: max FP equals starting FP.
      pwr_(floorAtZero(pwr)),
      wr_(floorAtZero(wr)),
      strategy_(nullptr) {}

Arcanist::Arcanist(int fp, int pwr, int wr,
                   std::unique_ptr<ArcanistClassStrategy> strategy)
    : fp_(floorAtZero(fp)),
      maxFp_(floorAtZero(fp)), // Requirement 1.8: max FP equals starting FP.
      pwr_(floorAtZero(pwr)),
      wr_(floorAtZero(wr)),
      strategy_(std::move(strategy)) {}

void Arcanist::reduceFp(int amount) {
    if (amount <= 0) {
        return;
    }
    fp_ = floorAtZero(fp_ - amount);
}

void Arcanist::applyDamage(int rawDamage) {
    // For this task damage is applied raw; the strategy hook and Aegis ordering
    // arrive in tasks 9.1 / 7.3.
    reduceFp(rawDamage);
}

void Arcanist::healUpTo(int amount) {
    if (amount <= 0) {
        return;
    }
    fp_ += amount;
    if (fp_ > maxFp_) {
        fp_ = maxFp_;
    }
}

void Arcanist::loseUpTo(int amount) {
    // Drain Focus (Requirement 8.4): permanent FP drain, NOT combat damage, so
    // it bypasses applyDamage's (future) mitigation and goes straight to the
    // raw primitive.
    reduceFp(amount);
}

void Arcanist::addPwr(int delta) { pwr_ = floorAtZero(pwr_ + delta); }

void Arcanist::addWr(int delta) { wr_ = floorAtZero(wr_ + delta); }

void Arcanist::applyPromotionBonus(int fpBonus, int pwrBonus, int wrBonus) {
    // Clamp each bonus to be non-negative — a promotion only ever buffs.
    fpBonus = floorAtZero(fpBonus);
    pwrBonus = floorAtZero(pwrBonus);
    wrBonus = floorAtZero(wrBonus);
    // Raise both max FP and current FP by the same amount so the FP <= maxFp
    // invariant is preserved and the extra Focus is immediately available.
    maxFp_ += fpBonus;
    fp_ += fpBonus;
    pwr_ += pwrBonus;
    wr_ += wrBonus;
}

} // namespace runevault
