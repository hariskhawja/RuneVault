// arcanist_factory.h
// RuneVault — the Arcanist creation Factory Method (task 9.1, design
// "Arcanist: Factory Method + Strategy").
//
// `ArcanistFactory::create` centralizes Arcanist construction: given a class
// selection key it builds a base Arcanist with the correct base stats
// (Requirements 1.2–1.5, 1.8) AND attaches the matching class Strategy
// (arcanist_class.h). Adding a fifth class means adding one strategy subclass
// and one branch here — no changes to combat/scoring/scroll logic (design DQ1).

#ifndef RUNEVAULT_ARCANIST_FACTORY_H
#define RUNEVAULT_ARCANIST_FACTORY_H

#include <memory>

#include "runevault/arcanist.h"

namespace runevault {

class ArcanistFactory {
public:
    // Build a base Arcanist for the given selection key with the matching
    // strategy attached:
    //   's' -> Sage       120 FP / 18 Pwr / 18 WR   (Requirement 1.2)
    //   'h' -> Hexblade    100 FP / 24 Pwr / 14 WR  (Requirement 1.3)
    //   'w' -> Warden      150 FP / 16 Pwr / 22 WR  (Requirement 1.4)
    //   'v' -> Voidwalker  110 FP / 22 Pwr / 12 WR  (Requirement 1.5)
    // Max FP is set equal to starting FP (Requirement 1.8, handled by Arcanist).
    //
    // Invalid key handling (documented decision): any key other than the four
    // above returns `nullptr`. The class-selection loop (Requirement 1.7, task
    // 17.1) re-prompts on invalid input, so a null return cleanly signals
    // "unrecognized class" without throwing.
    static std::shared_ptr<Arcanist> create(char classKey);
};

} // namespace runevault

#endif // RUNEVAULT_ARCANIST_FACTORY_H
