#ifndef RUNEVAULT_ARCANIST_FACTORY_H
#define RUNEVAULT_ARCANIST_FACTORY_H

#include <memory>

#include "runevault/arcanist.h"

namespace runevault {

class ArcanistFactory {
public:
    static std::shared_ptr<Arcanist> create(char classKey);
};

} // namespace runevault

#endif // RUNEVAULT_ARCANIST_FACTORY_H
