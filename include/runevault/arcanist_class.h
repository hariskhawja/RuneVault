#ifndef RUNEVAULT_ARCANIST_CLASS_H
#define RUNEVAULT_ARCANIST_CLASS_H

#include <string>

#include "runevault/scroll_type.h"

namespace runevault {

// Class abilities vary independently of the Arcanist and its temporary effects.
class ArcanistClassStrategy {
public:
    virtual ~ArcanistClassStrategy() = default;

    virtual int scoreModifier(int rawFragmentTotal) const = 0;

    virtual int scrollEffectModifier(ScrollType type, int baseDelta) const = 0;

    virtual int damageTakenModifier(int incomingDamage) const = 0;

    virtual std::string className() const = 0;

    virtual char classKey() const = 0;
};

class SageStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

class HexbladeStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

class WardenStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

class VoidwalkerStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

class ArchmageStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

class WarlockStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

class BastionStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

class VoidlordStrategy : public ArcanistClassStrategy {
public:
    int scoreModifier(int rawFragmentTotal) const override;
    int scrollEffectModifier(ScrollType type, int baseDelta) const override;
    int damageTakenModifier(int incomingDamage) const override;
    std::string className() const override;
    char classKey() const override;
};

} // namespace runevault

#endif // RUNEVAULT_ARCANIST_CLASS_H
