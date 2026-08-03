// spectre.h
// RuneVault — abstract Spectre base + generic placeholder (task 5.3).
//
// A `Spectre` is a non-playable vault enemy (Requirement 2.5 lists the seven
// concrete kinds: Wraith `W`, Banshee `B`, Revenant `R`, Shade `S`, Lich `L`,
// Vault Anchor `A`, Specter Lord `X`). This header defines the ABSTRACT base
// class described in design.md ("Spectres: Factory Method + Visitor") that
// carries the per-instance stats, board position, and the movement / drop /
// hostility policy hooks every concrete type shares.
//
// Scope note (deliberate, per the build order in design.md):
//   * The concrete typed subclasses (Wraith, Banshee, ...) are task 10.1 and
//     the double-dispatch `accept(CombatVisitor&)` / `visit(...)` machinery is
//     task 10.2. Neither exists yet, so this header intentionally OMITS
//     `accept()` — introducing it now would force a dependency on a visitor
//     type that has not been designed in code. `accept()` is added in task 10.2
//     when the real subclasses land.
//   * To keep the base standalone-usable so the movement/combat scaffolding
//     (tasks 6/7) can operate on a `Spectre*` before the specialised hierarchy
//     exists, this header also provides `GenericSpectre`: a concrete placeholder
//     that stores an explicit render symbol. Task 10.1 replaces it with the
//     seven real typed subclasses.
//
// `Spectre` is made abstract by leaving `symbol()` pure virtual; `GenericSpectre`
// supplies it. Movement is filtered to the spawn chamber elsewhere (the Model);
// this class only records the chamber id and the current position.

#ifndef RUNEVAULT_SPECTRE_H
#define RUNEVAULT_SPECTRE_H

#include "runevault/geometry.h"

namespace runevault {

// Forward declaration for the Visitor double-dispatch entry point (task 10.2,
// design DQ3 "CombatVisitor"). The full definition lives in combat_visitor.h;
// forward-declaring here avoids a header cycle (combat_visitor.h includes
// spectre.h). The accept() definitions in spectre.cpp include combat_visitor.h.
class CombatVisitor;

// The seven concrete Spectre kinds (design.md). Ordering matches the design's
// enum; the render symbols and stats are defined per Requirement 2.5 and the
// design's "Stats & drops table" when the concrete subclasses arrive (task
// 10.1).
enum class SpectreType {
    Wraith,
    Banshee,
    Revenant,
    Shade,
    Lich,
    VaultAnchor,
    SpecterLord
};

// Readable, human-facing name for a Spectre type ("Wraith", "Banshee",
// "Revenant", "Shade", "Lich", "Vault Anchor", "Specter Lord"). Pure and
// standalone — used by the notification log to phrase combat messages
// naturally.
std::string spectreTypeName(SpectreType t);

// Abstract base for every Spectre. Holds identity, stats, position, and the
// spawn chamber a Spectre may never leave. Subclasses override `symbol()` (and,
// from task 10.2, `accept()`); the movement / drop / hostility hooks are
// `virtual` with sensible defaults so most types need not override them.
class Spectre {
public:
    Spectre(SpectreType t, int fp, int pwr, int wr, int spawnChamber,
            Position pos);
    virtual ~Spectre() = default;

    // ---- Visitor double-dispatch (task 10.2, design DQ3) ----
    // The CombatVisitor double-dispatch entry point. Each concrete subclass
    // overrides this to call `v.visit(*this)`, selecting the type-specific
    // `visit` overload (double dispatch). This is pure virtual, which (together
    // with `symbol()`) keeps `Spectre` abstract. See combat_visitor.h.
    virtual void accept(CombatVisitor& v) = 0;

    // ---- Identity & rendering ----
    // Pure virtual: this is what makes `Spectre` abstract. Concrete types return
    // their fixed glyph (Requirement 2.5).
    virtual char symbol() const = 0;
    SpectreType type() const;

    // ---- Stats ----
    int fp() const;
    int pwr() const;
    int wr() const;

    // Reduce FP by `dmg`, clamped so FP never drops below 0. Negative `dmg` is
    // treated as 0 (no healing via this path).
    void takeDamage(int dmg);

    // True once FP has reached 0.
    bool isDead() const;

    // ---- Movement constraints ----
    int spawnChamber() const; // the Spectre may never leave this chamber
    Position position() const;
    void moveTo(Position p);

    // Default true; the (later) Vault Anchor overrides to false — it is
    // stationary.
    virtual bool isMobile() const;

    // ---- Guarded item position (task 10.5, Requirements 7.5/7.6) --------
    // The board tile of the item this Spectre guards. Only meaningful for the
    // Vault Anchor, whose hostility depends on whether the Arcanist is adjacent
    // to the Coffer it guards (Requirement 7.5 hostile when adjacent; 7.6
    // non-hostile otherwise). Defaults to the Spectre's OWN spawn position so
    // an Anchor with no explicit Coffer wired behaves as "guarding its own
    // tile". The Spawner (task 15.1) places the Anchor within a 1-block radius
    // of its Coffer and the guarded-item wiring (tasks 13.2/13.3) call
    // setGuardedItemPosition to point this at the actual Coffer tile.
    Position guardedItemPosition() const;
    void setGuardedItemPosition(const Position &p);

    // ---- Drops ----
    // Default 1 Rune Fragment. Overridden later: Lich cache = 4, Vault Anchor =
    // 0 (none).
    virtual int runeDrop() const;

    // ---- Hostility ----
    // Default true; overridden later: Lich (non-hostile until attacked) and
    // Vault Anchor (hostile only when the Arcanist is adjacent to its Coffer)
    // return false.
    virtual bool isDefaultHostile() const;

    // ---- Cipher Gem carrier (task 7.8, Requirement 6.12) ----------------
    // Exactly one non-Vault-Anchor Spectre per level carries the level's Cipher
    // Gem (the Spawner picks it in task 15.7). When such a carrier is defeated,
    // the Cipher Gem drops on its tile (Requirement 6.12). Defaults to false;
    // the Spawner flips it on the chosen carrier.
    bool carriesCipherGem() const;
    void setCarriesCipherGem(bool carries);

private:
    SpectreType type_;
    int fp_;
    int pwr_;
    int wr_;
    int spawnChamber_;
    Position pos_;
    Position guardedItemPos_; // tile of the guarded item (task 10.5); default = pos_
    bool carriesCipherGem_; // Cipher Gem carrier flag (task 7.8); default false
};

// The following 'GenericSpectre' is only for testing
// Concrete placeholder Spectre (task 5.3). Stores an explicit render symbol so
// the movement/combat scaffolding can instantiate and manipulate a `Spectre`
// before the typed hierarchy (task 10.1) exists. It uses every base default for
// mobility, drops, and hostility.
//
// NOTE: task 10.1 adds the seven real typed subclasses BELOW; GenericSpectre is
// retained (not deleted) because the base-class tests (task 5.3) exercise the
// base behaviour through it.
class GenericSpectre : public Spectre {
public:
    GenericSpectre(SpectreType t, int fp, int pwr, int wr, int spawnChamber,
                   Position pos, char symbol);

    char symbol() const override;

    // GenericSpectre is a test-only placeholder with NO dedicated CombatVisitor
    // `visit` overload — it does not participate in type-specific visitor
    // dispatch. Its accept() is therefore a documented no-op (see spectre.cpp),
    // which keeps the base abstract-class contract satisfied and lets existing
    // GenericSpectre-based tests continue to compile.
    void accept(CombatVisitor& v) override;

private:
    char symbol_;
};

// ---- Concrete typed Spectre subclasses (task 10.1) --------------------------
//
// Each subclass forwards its fixed stats (design "Stats & drops table",
// Requirement 2.5) and its `SpectreType` to the base `Spectre` constructor and
// overrides `symbol()` with its DD-7 glyph (see runevault::glyphs in view.h).
// Only the hooks that differ from the base defaults (mobile=true, drop=1,
// hostile=true) are overridden.
//
// NOTE (task 10.2): the Visitor double-dispatch `accept(CombatVisitor&)` is NOT
// declared here yet — mirroring the base class, it is added alongside the
// CombatVisitor in task 10.2, which will add `accept()` to the base and every
// subclass below.

// Wraith `W` — fp 60, pwr 20, wr 10; mobile, hostile, drops 1 RF (base
// defaults).
class Wraith : public Spectre {
public:
    Wraith(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override; // double-dispatch -> visit(Wraith&)
};

// Banshee `B` — fp 40, pwr 30, wr 5; mobile, hostile, drops 1 RF.
class Banshee : public Spectre {
public:
    Banshee(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override; // double-dispatch -> visit(Banshee&)
};

// Revenant `R` — fp 100, pwr 18, wr 20; mobile, hostile, drops 1 RF.
class Revenant : public Spectre {
public:
    Revenant(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override; // double-dispatch -> visit(Revenant&)
};

// Shade `S` — fp 50, pwr 8, wr 8; mobile, hostile, drops 1 RF.
class Shade : public Spectre {
public:
    Shade(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override; // double-dispatch -> visit(Shade&)
};

// Lich `L` — fp 30, pwr 40, wr 5; mobile, NOT hostile until attacked, drops a
// Lich cache of 4 RF (Requirement 2.5 / design table).
class Lich : public Spectre {
public:
    Lich(int spawnChamber, Position pos);
    char symbol() const override;
    int runeDrop() const override;         // Lich cache = 4
    bool isDefaultHostile() const override; // false — dormant until attacked
    void accept(CombatVisitor& v) override; // double-dispatch -> visit(Lich&)
};

// Vault Anchor `A` — fp 120, pwr 22, wr 18; STATIONARY, non-hostile by default
// (only fights when the Arcanist is adjacent to its Coffer), drops nothing. It
// is placed explicitly to guard coffers and is never produced by createRandom.
class VaultAnchor : public Spectre {
public:
    VaultAnchor(int spawnChamber, Position pos);
    char symbol() const override;
    bool isMobile() const override;         // false — never moves
    int runeDrop() const override;          // 0 — no drop
    bool isDefaultHostile() const override; // false — guards passively
    void accept(CombatVisitor& v) override; // double-dispatch -> visit(VaultAnchor&)
};

// Specter Lord `X` — fp 80, pwr 28, wr 15; mobile, hostile, drops 1 RF.
class SpecterLord : public Spectre {
public:
    SpecterLord(int spawnChamber, Position pos);
    char symbol() const override;
    void accept(CombatVisitor& v) override; // double-dispatch -> visit(SpecterLord&)
};

} // namespace runevault

#endif // RUNEVAULT_SPECTRE_H
