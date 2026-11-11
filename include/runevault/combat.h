#ifndef RUNEVAULT_COMBAT_H
#define RUNEVAULT_COMBAT_H

namespace runevault {

int computeDamage(int attackerPwr, int defenderWr);

int applyDamageTakenOrdering(int baseDamage, bool isWarden, bool carryingAegis);

} // namespace runevault

#endif // RUNEVAULT_COMBAT_H
