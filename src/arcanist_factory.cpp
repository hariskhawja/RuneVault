// arcanist_factory.cpp
// RuneVault — the Arcanist creation Factory Method (task 9.1).
//
// See include/runevault/arcanist_factory.h. The base-stats table below is the
// authoritative source for Requirements 1.2–1.5; each row builds an Arcanist
// with the matching class Strategy attached.

#include "runevault/arcanist_factory.h"

#include <memory>

#include "runevault/arcanist_class.h"

namespace runevault {

namespace {
// Helper: build a base Arcanist with the given stats and strategy.
std::shared_ptr<Arcanist>
make(int fp, int pwr, int wr, std::unique_ptr<ArcanistClassStrategy> strategy) {
    return std::make_shared<Arcanist>(fp, pwr, wr, std::move(strategy));
}
} // namespace

std::shared_ptr<Arcanist> ArcanistFactory::create(char classKey) {
    switch (classKey) {
    case 's': // Sage — Requirement 1.2
        return make(120, 18, 18,
                    std::unique_ptr<ArcanistClassStrategy>(new SageStrategy()));
    case 'h': // Hexblade — Requirement 1.3
        return make(
            100, 24, 14,
            std::unique_ptr<ArcanistClassStrategy>(new HexbladeStrategy()));
    case 'w': // Warden — Requirement 1.4
        return make(
            150, 16, 22,
            std::unique_ptr<ArcanistClassStrategy>(new WardenStrategy()));
    case 'v': // Voidwalker — Requirement 1.5
        return make(
            110, 22, 12,
            std::unique_ptr<ArcanistClassStrategy>(new VoidwalkerStrategy()));
    default:
        // Unrecognized class key — see header: return nullptr so the selection
        // loop can re-prompt (Requirement 1.7).
        return nullptr;
    }
}

} // namespace runevault
