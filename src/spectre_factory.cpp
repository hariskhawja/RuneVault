#include "runevault/spectre_factory.h"

#include "shuffle.h"

#include <vector>

namespace runevault {

std::unique_ptr<Spectre> SpectreFactory::create(SpectreType t, int chamberId,
                                                Position pos) const {
    switch (t) {
    case SpectreType::Wraith:
        return std::unique_ptr<Spectre>(new Wraith(chamberId, pos));
    case SpectreType::Banshee:
        return std::unique_ptr<Spectre>(new Banshee(chamberId, pos));
    case SpectreType::Revenant:
        return std::unique_ptr<Spectre>(new Revenant(chamberId, pos));
    case SpectreType::Shade:
        return std::unique_ptr<Spectre>(new Shade(chamberId, pos));
    case SpectreType::Lich:
        return std::unique_ptr<Spectre>(new Lich(chamberId, pos));
    case SpectreType::VaultAnchor:
        return std::unique_ptr<Spectre>(new VaultAnchor(chamberId, pos));
    case SpectreType::SpecterLord:
        return std::unique_ptr<Spectre>(new SpecterLord(chamberId, pos));
    }
    // Unreachable for a valid SpectreType; return a Wraith as a defensive
    // default so the function is total.
    return std::unique_ptr<Spectre>(new Wraith(chamberId, pos));
}

std::unique_ptr<Spectre> SpectreFactory::createRandom(int chamberId,
                                                      Position pos) const {
    static const std::vector<double> kSpawnWeights = {4, 3, 5, 2, 2, 2};
    static const SpectreType kSpawnTypes[] = {
        SpectreType::Wraith,   SpectreType::Banshee,
        SpectreType::Shade,    SpectreType::Revenant,
        SpectreType::SpecterLord, SpectreType::Lich};

    const std::size_t idx = rng::weighted(kSpawnWeights);
    return create(kSpawnTypes[idx], chamberId, pos);
}

} // namespace runevault
