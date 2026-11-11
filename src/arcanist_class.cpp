#include "runevault/arcanist_class.h"

#include "shuffle.h"

namespace runevault {

namespace {
int applyFiftyPercentBonus(int raw) { return raw + raw / 2; }
} // namespace

int SageStrategy::scoreModifier(int rawFragmentTotal) const {
    return applyFiftyPercentBonus(rawFragmentTotal);
}
int SageStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta;
}
int SageStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage;
}
std::string SageStrategy::className() const { return "Sage"; }
char SageStrategy::classKey() const { return 's'; }

int HexbladeStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal;
}
int HexbladeStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta * 2;
}
int HexbladeStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage;
}
std::string HexbladeStrategy::className() const { return "Hexblade"; }
char HexbladeStrategy::classKey() const { return 'h'; }

int WardenStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal;
}
int WardenStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta;
}
int WardenStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage - 1;
}
std::string WardenStrategy::className() const { return "Warden"; }
char WardenStrategy::classKey() const { return 'w'; }

int VoidwalkerStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal;
}
int VoidwalkerStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    if (baseDelta < 0 && rng::chance(0.5)) {
        return -baseDelta; // positive counterpart, same magnitude
    }
    return baseDelta;
}
int VoidwalkerStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage;
}
std::string VoidwalkerStrategy::className() const { return "Voidwalker"; }
char VoidwalkerStrategy::classKey() const { return 'v'; }

int ArchmageStrategy::scoreModifier(int rawFragmentTotal) const {
    return applyFiftyPercentBonus(rawFragmentTotal);
}
int ArchmageStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta;
}
int ArchmageStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage;
}
std::string ArchmageStrategy::className() const { return "Archmage"; }
char ArchmageStrategy::classKey() const { return 's'; } // upgraded Sage

int WarlockStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal;
}
int WarlockStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta * 3;
}
int WarlockStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage;
}
std::string WarlockStrategy::className() const { return "Warlock"; }
char WarlockStrategy::classKey() const { return 'h'; } // upgraded Hexblade

int BastionStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal;
}
int BastionStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    return baseDelta;
}
int BastionStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage - 2;
}
std::string BastionStrategy::className() const { return "Bastion"; }
char BastionStrategy::classKey() const { return 'w'; } // upgraded Warden

int VoidlordStrategy::scoreModifier(int rawFragmentTotal) const {
    return rawFragmentTotal;
}
int VoidlordStrategy::scrollEffectModifier(ScrollType, int baseDelta) const {
    if (baseDelta < 0) {
        return -baseDelta; // positive counterpart, same magnitude
    }
    return baseDelta;
}
int VoidlordStrategy::damageTakenModifier(int incomingDamage) const {
    return incomingDamage;
}
std::string VoidlordStrategy::className() const { return "Voidlord"; }
char VoidlordStrategy::classKey() const { return 'v'; } // upgraded Voidwalker

} // namespace runevault
