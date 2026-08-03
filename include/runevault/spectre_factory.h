// spectre_factory.h
// RuneVault — Factory Method for Spectres (task 10.1).
//
// `SpectreFactory` centralizes construction of the seven concrete Spectre types
// (design "Spectre hierarchy (DQ2)" / "SpectreFactory"). It is a SEPARATE
// concrete factory from `ArcanistFactory` (DD-6): it takes rng/level context
// (a chamber id and a board Position), not a player class choice, so a shared
// abstract-factory base would muddy both interfaces.
//
// Two creation paths:
//   * `create(type, ...)` — explicit construction of a specific type. Used by
//     layout files and by Vault Anchor placement (Anchors guard coffers and are
//     placed explicitly, never spawned at random).
//   * `createRandom(...)` — a random spawn honouring the spec distribution
//     (Requirement 4.7). It uses `rng::weighted` with the design weights and
//     NEVER produces a Vault Anchor.

#ifndef RUNEVAULT_SPECTRE_FACTORY_H
#define RUNEVAULT_SPECTRE_FACTORY_H

#include "runevault/geometry.h"
#include "runevault/spectre.h"

#include <memory>

namespace runevault {

class SpectreFactory {
public:
    // Explicit creation of any concrete type (Vault Anchor included, for
    // guarded-coffer placement). Returns a heap-owned Spectre of the matching
    // subclass positioned at `pos` and bound to `chamberId`.
    std::unique_ptr<Spectre> create(SpectreType t, int chamberId,
                                    Position pos) const;

    // Random spawn honouring the Requirement 4.7 distribution. Uses
    // `rng::weighted` with the weights {4, 3, 5, 2, 2, 2} (sum 18) in the order
    // {Wraith, Banshee, Shade, Revenant, SpecterLord, Lich}. NEVER returns a
    // Vault Anchor — Anchors are placed explicitly to guard coffers.
    std::unique_ptr<Spectre> createRandom(int chamberId, Position pos) const;
};

} // namespace runevault

#endif // RUNEVAULT_SPECTRE_FACTORY_H
