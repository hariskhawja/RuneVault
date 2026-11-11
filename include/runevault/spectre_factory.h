#ifndef RUNEVAULT_SPECTRE_FACTORY_H
#define RUNEVAULT_SPECTRE_FACTORY_H

#include "runevault/geometry.h"
#include "runevault/spectre.h"

#include <memory>

namespace runevault {

class SpectreFactory {
public:
    std::unique_ptr<Spectre> create(SpectreType t, int chamberId,
                                    Position pos) const;

    std::unique_ptr<Spectre> createRandom(int chamberId, Position pos) const;
};

} // namespace runevault

#endif // RUNEVAULT_SPECTRE_FACTORY_H
