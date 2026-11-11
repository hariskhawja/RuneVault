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
    case 's':
        return make(120, 18, 18,
                    std::unique_ptr<ArcanistClassStrategy>(new SageStrategy()));
    case 'h':
        return make(
            100, 24, 14,
            std::unique_ptr<ArcanistClassStrategy>(new HexbladeStrategy()));
    case 'w':
        return make(
            150, 16, 22,
            std::unique_ptr<ArcanistClassStrategy>(new WardenStrategy()));
    case 'v':
        return make(
            110, 22, 12,
            std::unique_ptr<ArcanistClassStrategy>(new VoidwalkerStrategy()));
    default:
        return nullptr;
    }
}

} // namespace runevault
