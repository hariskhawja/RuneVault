#include "runevault/spectre.h"

#include "runevault/combat_visitor.h"
#include "runevault/view.h"

namespace runevault {

Spectre::Spectre(SpectreType t, int fp, int pwr, int wr, int spawnChamber,
                 Position pos)
    : type_(t), fp_(fp), pwr_(pwr), wr_(wr), spawnChamber_(spawnChamber),
      pos_(pos), guardedItemPos_(pos), carriesCipherGem_(false) {}

std::string spectreTypeName(SpectreType t) {
    switch (t) {
    case SpectreType::Wraith:
        return "Wraith";
    case SpectreType::Banshee:
        return "Banshee";
    case SpectreType::Revenant:
        return "Revenant";
    case SpectreType::Shade:
        return "Shade";
    case SpectreType::Lich:
        return "Lich";
    case SpectreType::VaultAnchor:
        return "Vault Anchor";
    case SpectreType::SpecterLord:
        return "Specter Lord";
    }
    // Unreachable for a valid SpectreType; keeps the compiler happy.
    return "Spectre";
}

SpectreType Spectre::type() const { return type_; }

int Spectre::fp() const { return fp_; }
int Spectre::pwr() const { return pwr_; }
int Spectre::wr() const { return wr_; }

void Spectre::takeDamage(int dmg) {
    if (dmg <= 0) {
        return; // this path never heals; ignore non-positive damage
    }
    fp_ -= dmg;
    if (fp_ < 0) {
        fp_ = 0; // floor at 0 — FP never goes negative
    }
}

bool Spectre::isDead() const { return fp_ == 0; }

int Spectre::spawnChamber() const { return spawnChamber_; }

Position Spectre::position() const { return pos_; }

void Spectre::moveTo(Position p) { pos_ = p; }

bool Spectre::isMobile() const { return true; }

Position Spectre::guardedItemPosition() const { return guardedItemPos_; }

void Spectre::setGuardedItemPosition(const Position &p) { guardedItemPos_ = p; }

int Spectre::runeDrop() const { return 1; }

bool Spectre::isDefaultHostile() const { return true; }

bool Spectre::carriesCipherGem() const { return carriesCipherGem_; }

void Spectre::setCarriesCipherGem(bool carries) { carriesCipherGem_ = carries; }

GenericSpectre::GenericSpectre(SpectreType t, int fp, int pwr, int wr,
                               int spawnChamber, Position pos, char symbol)
    : Spectre(t, fp, pwr, wr, spawnChamber, pos), symbol_(symbol) {}

char GenericSpectre::symbol() const { return symbol_; }

void GenericSpectre::accept(CombatVisitor& /*v*/) { /* intentional no-op */ }

// Wraith `W` — fp 60, pwr 20, wr 10; base defaults (mobile, drop 1, hostile).
Wraith::Wraith(int spawnChamber, Position pos)
    : Spectre(SpectreType::Wraith, 60, 20, 10, spawnChamber, pos) {}
char Wraith::symbol() const { return glyphs::kWraith; }
void Wraith::accept(CombatVisitor& v) { v.visit(*this); }

// Banshee `B` — fp 40, pwr 30, wr 5; base defaults.
Banshee::Banshee(int spawnChamber, Position pos)
    : Spectre(SpectreType::Banshee, 40, 30, 5, spawnChamber, pos) {}
char Banshee::symbol() const { return glyphs::kBanshee; }
void Banshee::accept(CombatVisitor& v) { v.visit(*this); }

// Revenant `R` — fp 100, pwr 18, wr 20; base defaults.
Revenant::Revenant(int spawnChamber, Position pos)
    : Spectre(SpectreType::Revenant, 100, 18, 20, spawnChamber, pos) {}
char Revenant::symbol() const { return glyphs::kRevenant; }
void Revenant::accept(CombatVisitor& v) { v.visit(*this); }

// Shade `S` — fp 50, pwr 8, wr 8; base defaults.
Shade::Shade(int spawnChamber, Position pos)
    : Spectre(SpectreType::Shade, 50, 8, 8, spawnChamber, pos) {}
char Shade::symbol() const { return glyphs::kShade; }
void Shade::accept(CombatVisitor& v) { v.visit(*this); }

// Lich `L` — fp 30, pwr 40, wr 5; dormant until attacked, drops a cache of 4.
Lich::Lich(int spawnChamber, Position pos)
    : Spectre(SpectreType::Lich, 30, 40, 5, spawnChamber, pos) {}
char Lich::symbol() const { return glyphs::kLich; }
int Lich::runeDrop() const { return 4; }
bool Lich::isDefaultHostile() const { return false; } // aggro only when hit
void Lich::accept(CombatVisitor& v) { v.visit(*this); }

// Vault Anchor `A` — fp 120, pwr 22, wr 18; stationary, passive, no drop.
VaultAnchor::VaultAnchor(int spawnChamber, Position pos)
    : Spectre(SpectreType::VaultAnchor, 120, 22, 18, spawnChamber, pos) {}
char VaultAnchor::symbol() const { return glyphs::kVaultAnchor; }
bool VaultAnchor::isMobile() const { return false; }         // never moves
int VaultAnchor::runeDrop() const { return 0; }              // no drop
bool VaultAnchor::isDefaultHostile() const { return false; } // guards passively
void VaultAnchor::accept(CombatVisitor& v) { v.visit(*this); }

// Specter Lord `X` — fp 80, pwr 28, wr 15; base defaults.
SpecterLord::SpecterLord(int spawnChamber, Position pos)
    : Spectre(SpectreType::SpecterLord, 80, 28, 15, spawnChamber, pos) {}
char SpecterLord::symbol() const { return glyphs::kSpecterLord; }
void SpecterLord::accept(CombatVisitor& v) { v.visit(*this); }

} // namespace runevault
