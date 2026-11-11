#include "runevault/item.h"

#include "runevault/model.h"
#include "runevault/notification_log.h"

#include <string>

namespace runevault {

bool RuneFragment::onWalkOver(Model &model) {
    model.addRuneFragments(value_);
    notify("You collect a Rune Fragment (+" + std::to_string(value_) + " RF).");
    return true;
}

} // namespace runevault
