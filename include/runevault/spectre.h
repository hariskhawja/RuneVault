#ifndef RUNEVAULT_SPECTRE_H
#define RUNEVAULT_SPECTRE_H

#include "runevault/geometry.h"

namespace runevault {

class CombatVisitor;

enum class SpectreType {
    Wraith,
    Banshee,
    Revenant,
    Shade,
    Lich,
    VaultAnchor,
    SpecterLord
};

std::string spectreTypeName(SpectreType t);

class Spectre {
public:
    Spectre(SpectreType t, int fp, int pwr, int wr, int spawnChamber,
            Position pos);
    virtual ~Spectre() = default;

    virtual void accept(CombatVisitor& v) = 0;

    virtual char symbol() const = 0;
    SpectreType type() const;

    int fp() const;
    int pwr() const;
    int wr() const;

    // Reduce FP by `dmg`, clamped so FP never drops below 0. Negative `dmg` is
    // treated as 0 (no healing via this path).
    void takeDamage(int dmg);

    // True once FP has reached 0.
    bool isDead() const;

    int spawnChamber() const; // the Spectre may never leave this chamber
    Position position() const;
    void moveTo(Position p);

    // Default true; Vault Anchor overrides to false — it is
    // stationary.
    virtual bool isMobile() const;

    Position guardedItemPosition() const;
    void setGuardedItemPosition(const Position &p);

    virtual int runeDrop() const;

    virtual bool isDefaultHostile() const;

    bool carriesCipherGem() const;
    void setCarriesCipherGem(bool carries);

private:
    SpectreType type_;
    int fp_;
    int pwr_;
    int wr_;
    int spawnChamber_;
    Position pos_;
    Position guardedItemPos_;
    bool carriesCipherGem_;
};

class GenericSpectre : public Spectre {
public:
    GenericSpectre(SpectreType t, int fp, int pwr, int wr, int spawnChamber,
                   Position pos, char symbol);

    char symbol() const override;

    void accept(CombatVisitor& v) override;

private:
    char symbol_;
};

// Wraith `W` — fp 60, pwr 20, wr 10; mobile, hostile, drops 1 RF (base
// defaults).
class Wraith : public Spectre {
public:
    Wraith(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override;
};

// Banshee `B` — fp 40, pwr 30, wr 5; mobile, hostile, drops 1 RF.
class Banshee : public Spectre {
public:
    Banshee(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override;
};

// Revenant `R` — fp 100, pwr 18, wr 20; mobile, hostile, drops 1 RF.
class Revenant : public Spectre {
public:
    Revenant(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override;
};

// Shade `S` — fp 50, pwr 8, wr 8; mobile, hostile, drops 1 RF.
class Shade : public Spectre {
public:
    Shade(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override;
};

class Lich : public Spectre {
public:
    Lich(int spawnChamber, Position pos);
    char symbol() const override;
    int runeDrop() const override;         // Lich cache = 4
    bool isDefaultHostile() const override; // false — dormant until attacked
    void accept(CombatVisitor& v) override;
};

class VaultAnchor : public Spectre {
public:
    VaultAnchor(int spawnChamber, Position pos);
    char symbol() const override;
    bool isMobile() const override;         // false — never moves
    int runeDrop() const override;          // 0 — no drop
    bool isDefaultHostile() const override; // false — guards passively
    void accept(CombatVisitor& v) override;
};

// Specter Lord `X` — fp 80, pwr 28, wr 15; mobile, hostile, drops 1 RF.
class SpecterLord : public Spectre {
public:
    SpecterLord(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override;
};

} // namespace runevault

#endif // RUNEVAULT_SPECTRE_H
