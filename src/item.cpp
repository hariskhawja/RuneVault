// item.cpp
// RuneVault — out-of-line Item / RuneFragment definitions (task 5.2).
//
// See include/runevault/item.h for the scope of this task. The `Item` base and
// most of `RuneFragment` are inline in the header; the walk-over hook is
// defined here so its body can be fleshed out in task 6.3 without touching the
// header. A reference to the (still incomplete) `Model` is all this body needs,
// so model.h is intentionally not included yet.

#include "runevault/item.h"

#include "runevault/model.h"            // Model::addRuneFragments
#include "runevault/notification_log.h" // notify

#include <string> // std::to_string

namespace runevault {

bool RuneFragment::onWalkOver(Model &model) {
    // Requirement 5.4: a Rune Fragment is collected transiently on walk-over —
    // credit its value and announce the pickup. Return true so the owning Level
    // removes it (the Model, as owner, performs the physical removal; the
    // fragment never deletes itself).
    model.addRuneFragments(value_);
    notify("You collect a Rune Fragment (+" + std::to_string(value_) + " RF).");
    return true;
}

} // namespace runevault
