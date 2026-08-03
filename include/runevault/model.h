// model.h
// RuneVault — the Model skeleton: aggregated run-scoped state (task 5.4).
//
// The `Model` is the single owner of everything that makes up one run: the
// persistent base Arcanist, the current Level, the accumulated Rune Fragment
// total, and the assorted per-run flags the rules key off of (Lich aggro,
// which relics are being carried, which scrolls have been identified, and the
// Aegis-Cloak spawn bookkeeping). It is the aggregate root the Controller
// drives and the View reads (design "Model" — MVC).
//
// SCOPE OF THIS TASK: this is a **state container skeleton only**. It holds the
// aggregated run state and exposes simple accessors/mutators. It deliberately
// does NOT implement combat, movement, scroll effects, spawning, or level
// transitions — those arrive in later tasks (6/7/12/15/16). Requirement 1.8
// (max FP equals starting FP) is enforced inside `Arcanist`; the Model merely
// holds the Arcanist.
//
// DEPENDENCY NOTES:
//   * The design's `activeArcanist` decorator chain (shared_ptr<ArcanistComponent>)
//     is NOT added here — the Decorator/Component lives in task 12.1.
//   * Identified scrolls are tracked by the `ScrollType` enum in a
//     `std::set<ScrollType>` (task 12.6). Backwards-compatible int overloads of
//     markScrollIdentified/isScrollIdentified are retained for older call sites.

#ifndef RUNEVAULT_MODEL_H
#define RUNEVAULT_MODEL_H

#include <memory>
#include <set>
#include <string>

#include "runevault/arcanist.h"
#include "runevault/arcanist_component.h"
#include "runevault/geometry.h"
#include "runevault/level.h"
#include "runevault/scroll_type.h"
#include "runevault/spectre.h"

namespace runevault {

// Forward declaration of the parsed level-layout file representation (task
// 18.2, layout_loader.h). The Model holds a NON-OWNING pointer to a
// LoadedLayout when a layout file drives the run (task 19.1); the full type is
// only needed in model.cpp (buildCurrentLevel), so a forward declaration keeps
// this header lightweight and avoids a heavy include.
struct LoadedLayout;

// The high-level outcome state of a run (Requirements 10.4/10.5). A run starts
// in `Playing` and transitions exactly once: to `Lost` when the Arcanist's FP
// reaches 0 (Requirement 10.5, wired here in task 7.6) or to `Won` when the
// Arcanist descends the stairway on level 5 (Requirement 10.4, wired in task
// 16.3). This task only sets `Lost`; `Won` is reserved for that later task.
enum class RunState { Playing, Won, Lost };

// Aggregate root for one run's mutable state. All fields start empty/zeroed so
// a freshly constructed Model represents "before a run has been set up"; the
// Controller populates the Arcanist and first Level as the run begins.
class Model {
public:
    // Default-construct empty run state: no Arcanist, no Level, level number 1,
    // all totals zeroed and all flags false.
    Model();

    // --- Base Arcanist (persistent across levels) ------------------------
    // The Model shares ownership of the base Arcanist (design: shared_ptr).
    const std::shared_ptr<Arcanist> &arcanist() const { return baseArcanist_; }
    void setArcanist(std::shared_ptr<Arcanist> arcanist) {
        baseArcanist_ = std::move(arcanist);
        // (Re)build the decorator chain head as a fresh BaseArcanistComponent
        // wrapping the (new) base Arcanist (task 12.1). Any prior temporary
        // scroll decorators are dropped. When the base is null the chain head is
        // cleared too, so effectivePwr/effectiveWr fall back safely.
        if (baseArcanist_ != nullptr) {
            activeArcanist_ = std::make_shared<BaseArcanistComponent>(baseArcanist_);
        } else {
            activeArcanist_.reset();
        }
    }

    // --- Effective-stat decorator chain (task 12.1) ----------------------
    // `activeArcanist_` points at the HEAD of the temporary-scroll Decorator
    // chain (design "Arcane Scrolls with the Decorator pattern"). It starts as a
    // BaseArcanistComponent wrapping the base Arcanist (built in setArcanist);
    // scroll use (task 12.2) wraps it with a ScrollDecorator, and level
    // transition (task 16.1) resets it back to a fresh base component so the
    // current-level effects tear down.
    ArcanistComponent *activeArcanist() { return activeArcanist_.get(); }
    const ArcanistComponent *activeArcanist() const { return activeArcanist_.get(); }

    // Access/replace the chain head as a shared_ptr. Scroll use wraps the
    // returned pointer and hands the new head back via setActiveArcanist; level
    // teardown replaces it with a fresh BaseArcanistComponent (or resetActiveArcanistToBase).
    const std::shared_ptr<ArcanistComponent> &activeArcanistPtr() const {
        return activeArcanist_;
    }
    void setActiveArcanist(std::shared_ptr<ArcanistComponent> component) {
        activeArcanist_ = std::move(component);
    }

    // Reset the chain head back to a fresh BaseArcanistComponent wrapping the
    // base Arcanist, discarding every temporary scroll decorator (task 16.1
    // level-transition teardown). No-op leaving a null head when there is no
    // base Arcanist.
    void resetActiveArcanistToBase() {
        if (baseArcanist_ != nullptr) {
            activeArcanist_ = std::make_shared<BaseArcanistComponent>(baseArcanist_);
        } else {
            activeArcanist_.reset();
        }
    }

    // Effective Power / Ward Rating the combat layer reads: the decorator
    // chain's effPwr()/effWr() when a chain head is present, else the base
    // Arcanist's stat, else 0 when there is no Arcanist at all (safe guard).
    int effectivePwr() const {
        if (activeArcanist_ != nullptr) {
            return activeArcanist_->effPwr();
        }
        return baseArcanist_ != nullptr ? baseArcanist_->pwr() : 0;
    }
    int effectiveWr() const {
        if (activeArcanist_ != nullptr) {
            return activeArcanist_->effWr();
        }
        return baseArcanist_ != nullptr ? baseArcanist_->wr() : 0;
    }

    // --- Current level ----------------------------------------------------
    // The Model uniquely owns the current Level (design: unique_ptr). Passed by
    // move so ownership transfers into the Model.
    Level *level() { return level_.get(); }
    const Level *level() const { return level_.get(); }
    void setLevel(std::unique_ptr<Level> level) { level_ = std::move(level); }

    // --- Optional level-layout file (task 19.1, Requirement 12.1) --------
    // When a layout file is supplied on the command line, the Controller wires
    // the parsed LoadedLayout into the Model via setLayout(). While set, every
    // level (the first and each descent) is built from the FILE's map + item
    // digits instead of the random default layout: buildCurrentLevel() uses
    // LayoutLoader::buildLevel for the board + scroll/fragment/coffer items and
    // then a Spawner ACTORS-ONLY pass to place the Arcanist, stairway, spectres,
    // Cipher Gem carrier, the guarding Vault Anchor for each file Shard Coffer,
    // and (on the chosen level) the Aegis Cloak — WITHOUT adding random
    // scrolls/fragments (those came from the file). The pointer is NON-OWNING:
    // the LoadedLayout is owned by the Controller (Game) and outlives the Model.
    // Null (the default) means the random default-layout path is used unchanged.
    void setLayout(const LoadedLayout *layout) { layout_ = layout; }
    const LoadedLayout *layout() const { return layout_; }

    // Build (or rebuild) the CURRENT level (for levelNumber()) and populate it.
    // The single entry point both the run start (Game::startNewRun) and a level
    // descent (Model::descend) funnel through so file-driven and random runs
    // share one construction path:
    //   * layout() != nullptr: build the file board + digit items via
    //     LayoutLoader::buildLevel(*layout(), levelNumber()-1), then run the
    //     Spawner's actors-only pass (populateActorsOnly) so the file's items
    //     are kept and only the actors/guards are placed on top.
    //   * layout() == nullptr: the default path — a fresh
    //     Level(makeDefaultBoard()) populated by the full Spawner pipeline.
    // Seeds must be set by the caller (main/tests) before this runs since both
    // paths consume rng for placement (Requirement 4.11).
    void buildCurrentLevel();

    // Current level number, 1..kChamberCount-worth of levels (starts at 1).
    int levelNumber() const { return levelNumber_; }
    void setLevelNumber(int n) { levelNumber_ = n; }

    // --- Arcanist position (task 6.1) ------------------------------------
    // The Arcanist's board position lives on the Model, which aggregates all
    // run-scoped state; the `Arcanist` stat class stays position-agnostic. The
    // Controller/Spawner places the Arcanist on a floor tile as the level is
    // set up (later tasks); it defaults to (0,0) until then.
    Position arcanistPosition() const { return arcanistPos_; }
    void setArcanistPosition(const Position &p) { arcanistPos_ = p; }

    // --- Movement (task 6.1, Requirements 5.1/5.2/5.3) -------------------
    // Attempt to move the Arcanist one cell in direction `d`. Requires a Level
    // to be set (the target is validated against level()->board()).
    //
    // Behaviour:
    //   * Compute target = Geometry::step(arcanistPos_, d).
    //   * If the target is out of bounds (Requirement 5.3) OR the target cell
    //     is a vault wall (not walkable, Requirement 5.3), REJECT: leave the
    //     position unchanged and return false. Returning false signals to the
    //     Controller that the turn was NOT consumed.
    //   * Otherwise the target is walkable — a floor tile, archway, corridor,
    //     or stairway (Requirement 5.2). Move the Arcanist onto it, return true
    //     (the turn is consumed).
    //
    // Fragment walk-over collection (task 6.3) is handled after a successful
    // move. Guarded-item walk-over and stairway reveal (task 6.5) are also
    // handled here: movement is NEVER blocked by an item (DD-4 / Requirement
    // 5.5), and the stairway is revealed once the Cipher Gem is carried
    // (Requirements 2.6/2.7) via `revealStairwayIfCarryingGem`. Returns false
    // (no move) when no Level is set.
    bool moveArcanist(Direction d);

    // Convenience overload accepting a lowercase command token ("no", "so",
    // "ea", "we", "ne", "nw", "se", "sw"). Returns false for an unrecognized
    // token (treated as no move / turn not consumed) instead of throwing; the
    // Controller (task 17.2) owns full command-token parsing. Recognized tokens
    // delegate to the Direction-based overload above.
    bool moveArcanist(const std::string &token);

    // --- Scroll use (task 12.2, Requirements 8.1/8.2/8.3/8.4/8.9) --------
    // Resolve a `u <dir>` command: search the 1-block radius in direction `d`
    // for an Arcane Scroll and, if found, consume it.
    //
    // Behaviour:
    //   * Guard: with no Level or no Arcanist there is nothing to resolve —
    //     return false (turn not consumed).
    //   * Requirement 8.1: the target tile is the single cell one step in
    //     direction `d` (target = Geometry::step(arcanistPosition(), d)), which
    //     is by construction within the 1-block radius.
    //   * If a Scroll occupies that tile, apply its effect (see below), remove
    //     it from the board, mark its type identified (task 12.6 finalizes the
    //     identification hint/reporting), and return true (the turn is consumed).
    //   * Requirement 8.2: if there is NO scroll in that direction the command
    //     is a NO-OP — return false and do NOT consume the turn.
    //
    // Effect by ScrollType (BASE magnitudes only — the Hexblade doubling /
    // Voidwalker sign-flip route through the class Strategy's
    // scrollEffectModifier and are TASK 12.4; they are intentionally NOT applied
    // here):
    //   * AttuneFocus (8.3): permanent +10 FP, capped at maxFp (healUpTo(10)).
    //   * DrainFocus  (8.4): permanent -10 FP, floored at 0 (loseUpTo(10)).
    //   * SurgePower  (8.5): temporary — wrap the decorator chain in SurgePower(5).
    //   * FortifyWard (8.6): temporary — wrap in FortifyWard(5).
    //   * SapPower    (8.7): temporary — wrap in SapPower(5).
    //   * ErodeWard   (8.8): temporary — wrap in ErodeWard(5).
    // TASK 12.4 will route the magnitude/sign through
    // strategy.scrollEffectModifier(type, baseDelta) before constructing the
    // decorator / applying the FP change.
    bool useScroll(Direction d);

    // Convenience overload accepting a lowercase direction token ("no", "so",
    // ...), mirroring moveArcanist(const std::string&). Returns false for an
    // unrecognized token (no-op / turn not consumed); recognized tokens delegate
    // to the Direction-based overload above. Provided for the future controller.
    bool useScroll(const std::string &token);

    // --- Level transition / descent (task 16.1, Requirements 10.3/10.6/10.7/10.8) --
    // Advance the run from the current level (1..4) to the next level. This is
    // the transition PRIMITIVE the Controller (task 17) invokes once the
    // Arcanist walks onto the revealed stairway while carrying the current
    // level's Cipher Gem; the win on level 5 is a SEPARATE concern (task 16.3)
    // and is deliberately NOT handled here.
    //
    // Guards (no transition, returns false, nothing mutated):
    //   * Requirement 10.1 (enforced upstream): if !carryingCipherGem() there is
    //     no descent — return false.
    //   * levelNumber() >= 5: descending from level 5 is the WIN (task 16.3), so
    //     descend() refuses here and does NOT build a level 6 — return false.
    //
    // On a valid descent (carrying the gem, on level 1..4):
    //   * Requirement 10.6: tear down the current-level-only scroll Decorator
    //     chain via resetActiveArcanistToBase() so effective Pwr/WR return to
    //     their base values.
    //   * Requirement 10.7: the base Arcanist (arcanist()) is retained untouched,
    //     so all permanent FP changes made on prior levels persist.
    //   * Requirement 10.8: reset the carried Cipher Gem state
    //     (setCarryingCipherGem(false), clearCipherGemDrop()) and RETAIN the
    //     Aegis Cloak (carryingAegisCloak() is left unchanged).
    //   * Increment the level number and build the next level: a fresh
    //     Level(makeDefaultBoard()) is set on the Model and populated by a
    //     Spawner (which places the Arcanist, stairway, items, and spectres, and
    //     the Aegis on the chosen level if applicable). The run-scoped Aegis
    //     bookkeeping (aegisLevel()/aegisSpawnedThisRun()) is preserved — descend
    //     never re-chooses it. The new level starts with its stairway hidden and
    //     the gem not carried (correct: the new level's gem must be found again).
    //
    // Callers should seed the rng before descend() since it repopulates a level.
    // Returns true on a successful transition, false otherwise.
    bool descend();

    // --- Stairway occupancy + win/descent trigger (task 16.3, Requirements 10.1/10.2/10.4) --
    // True while the Arcanist is standing on the current level's stairway tile
    // (arcanistPosition() == level()->stairway()). Requires a Level to be set;
    // returns false when level() == nullptr. This is the geometric predicate the
    // descent trigger keys off of — it does NOT consider whether the gem is
    // carried, so callers gate on carryingCipherGem() separately.
    bool onStairway() const;

    // The descent/win TRIGGER: the single entry point that decides what stepping
    // onto the stairway does. Built on top of descend() (task 16.1), it enforces
    // Requirements 10.1/10.2/10.4:
    //   * Requirements 10.1/10.2: if the Arcanist is NOT on the stairway, or is
    //     on it but does NOT carry the current level's Cipher Gem, this is a
    //     NO-OP — nothing is mutated and it returns false. Walking onto the
    //     stairway without the gem therefore has no effect.
    //   * Requirement 10.4: on the stairway WITH the gem on level 5
    //     (levelNumber() == kChamberCount), the run ends as a WIN
    //     (setRunState(RunState::Won)) and returns true; no level 6 is built.
    //   * Requirement 10.3 (delegated): on the stairway WITH the gem on levels
    //     1..4, this calls descend() and returns its result, advancing the run.
    //
    // moveArcanist invokes this automatically after a successful move lands the
    // Arcanist on the stairway while carrying the gem, so the Controller need
    // not; it is also exposed for direct/testable invocation.
    bool tryDescend();

    // --- Arcanist attack (task 7.5, Requirements 6.1/6.2/6.3) ------------
    // Resolve an `a <dir>` command: the Arcanist strikes the cell one step in
    // direction `d`.
    //
    // Behaviour:
    //   * Compute target = Geometry::step(arcanistPosition(), d). Because a
    //     single step is always one of the 8 adjacent cells (Chebyshev distance
    //     1), the target is by construction immediately adjacent (Requirement
    //     6.1).
    //   * Look for a LIVING Spectre on that tile via level()->spectreAt(target).
    //     If there is no Spectre, or the Spectre found is already dead, treat
    //     the command as a NO-OP: return false and do NOT consume the turn
    //     (Requirement 6.3).
    //   * Otherwise the Arcanist ALWAYS hits, with initiative — before any
    //     Spectre acts (Requirement 6.1; the Spectre action phase is task
    //     10.3). Compute damage = computeDamage(arcanist()->pwr(),
    //     spectre->wr()) (Requirement 6.2) and apply it via takeDamage. Return
    //     true (the turn is consumed).
    //
    // SCOPE: this uses the base arcanist()->pwr(); the effective (decorator)
    // Power is task 12.1, and the CombatVisitor special abilities / Lich aggro
    // trigger / Vault Anchor unlock are tasks 10.2/10.5 — none are handled
    // here. Drops and dead-Spectre removal are task 7.8: a slain Spectre is
    // deliberately LEFT in place so 7.8 can award its drop, so this method does
    // not call removeDeadSpectres().
    //
    // Returns false (no-op) when there is no Arcanist or no Level set.
    bool handleAttack(Direction d);

    // --- Spectre attack (task 7.6, Requirements 6.5/6.6/10.5) ------------
    // Pure, side-effect-free core of a Spectre's hit against the Arcanist: the
    // final FP loss a hit WOULD inflict, before the 50% miss roll and before it
    // is applied. Requires arcanist() != nullptr (returns 0 otherwise so the
    // helper is safe in isolation).
    //
    //   base  = computeDamage(spectre.pwr(), effectiveWr)   (Requirement 6.6)
    //   final = applyDamageTakenOrdering(base, isWarden, carryingAegisCloak())
    //
    // The effective Ward Rating is arcanist()->wr() for now — the scroll
    // Decorator's effective WR is task 12.1. `isWarden` is passed as false here:
    // the Arcanist class Strategy is task 9.1, which will supply the Warden -1
    // damage-taken modifier via strategy.damageTakenModifier(); the Aegis Cloak
    // halving (carryingAegisCloak()) DOES apply already. Exposed so tests can
    // assert the exact damage math without any randomness.
    int spectreDamageIfHit(const Spectre &spectre) const;

    // Resolve a single Spectre's attack against the Arcanist. This is the
    // per-attack resolution the Spectre action phase (task 10.3/10.5) calls once
    // it has decided a hostile Spectre within a 1-block radius attacks instead
    // of moving (Requirement 6.4 — wired there, not here).
    //
    // Behaviour:
    //   * Requires arcanist() != nullptr; returns false (no attack) otherwise.
    //   * Requirement 6.5: roll rng::chance(0.5). CONVENTION: a true roll is a
    //     MISS (no damage) and returns false; a false roll is a HIT. Either
    //     mapping satisfies "50% chance to miss"; this one is fixed so tests can
    //     reason about seeded sequences.
    //   * On a hit (Requirements 6.6 + damage-taken ordering): compute the FP
    //     loss via spectreDamageIfHit() and apply it with
    //     arcanist()->applyDamage(final) (floored at 0).
    //   * Requirement 10.5: if the hit brings the Arcanist's FP to 0
    //     (arcanist()->isDead()), end the run as a loss by setting
    //     runState_ = RunState::Lost.
    //
    // Returns true if the attack HIT (damage dealt), false on a miss or when
    // there is no Arcanist.
    bool resolveSpectreAttack(Spectre &spectre);

    // --- Hostility resolution (task 10.5, Requirements 7.5/7.6/7.7/7.8/7.9) --
    // Decide whether `spectre` is currently hostile toward the Arcanist. This
    // is the single source of truth the Spectre action phase (advanceSpectres,
    // Requirement 6.4) consults to choose "attack" over "wander". Per the
    // design "Hostility resolution" pseudocode:
    //
    //   * Vault Anchor (Requirements 7.5/7.6): hostile ONLY while the Arcanist
    //     is adjacent to the Coffer it guards — i.e.
    //     Geometry::isAdjacent(arcanistPosition(), spectre.guardedItemPosition())
    //     — and non-hostile otherwise. The guarded-Coffer tile is carried per
    //     Anchor via Spectre::guardedItemPosition() (defaults to the Anchor's
    //     own tile until the Spawner/guarded-item wiring points it at the real
    //     Coffer).
    //   * Lich (Requirements 7.7/7.8/7.9): hostile iff lichAggroTriggered().
    //     The global flag is tripped on attack by the CombatVisitor (task 10.2)
    //     — this only READS it, so every already-spawned and future Lich is
    //     hostile while the flag is set (7.8) and all Liches are non-hostile
    //     while it is clear (7.9).
    //   * Every other type: isDefaultHostile() (always true for Wraith,
    //     Banshee, Revenant, Shade, and Specter Lord).
    bool isHostile(const Spectre &spectre) const;

    // --- Spectre action phase (task 10.3, Requirements 7.1/7.2/7.3/7.4/7.10) --
    // Advance every Spectre on the current level exactly once, in the spec's
    // deterministic order, applying the wandering movement rules. Called once
    // per Arcanist action, after the Arcanist has acted (design "Spectre
    // movement algorithm").
    //
    // Behaviour:
    //   * Requirement 7.1: Spectres are processed row-by-row, left-to-right —
    //     ordered by current (row, col) ascending — and each is moved AT MOST
    //     ONCE. Ordering is computed over a snapshot copy of the Spectre
    //     pointers so the Level's owning vector is never reordered.
    //   * Dead Spectres are skipped.
    //   * Requirement 7.4: Vault Anchors are stationary and are never moved
    //     (isMobile() == false).
    //   * Requirements 7.2/7.3/7.10 (the wandering branch): a mobile Spectre
    //     that is not attacking moves to a uniformly-random UNOCCUPIED floor
    //     tile within its spawn chamber selected from its 1-block radius. The
    //     candidate set is Board::floorNeighboursInChamber(pos, spawnChamber)
    //     — already restricted to the spawn chamber's floor tiles, so a Spectre
    //     can never leave its spawn chamber — filtered to tiles not occupied by
    //     another living Spectre and not the Arcanist's tile. If no candidate
    //     remains the Spectre stays put.
    //
    // SCOPE (task 10.3): this implements the MOVEMENT (wandering) branch only.
    // The hostility / attack-instead-of-move decision (Requirements
    // 6.4/7.5-7.9) is task 10.5, which inserts its check at the marked point in
    // advanceSpectres so that a hostile Spectre within a 1-block radius of the
    // Arcanist attacks (resolveSpectreAttack) rather than wandering. Until then
    // every mobile Spectre wanders. No-op when level() == nullptr.
    void advanceSpectres();

    // --- Rune Fragments ---------------------------------------------------
    int runeFragmentTotal() const { return runeFragmentTotal_; }
    void setRuneFragmentTotal(int total) { runeFragmentTotal_ = total; }
    // Accumulate `amount` fragments onto the running total. Negative amounts are
    // ignored so the total never decreases through this helper. This is the
    // single gameplay RF-gain funnel (fragment walk-over, spectre drops, Shard
    // Coffer collect), so the Prestige promotion trigger (maybePromote) is
    // driven from here — NOT from setRuneFragmentTotal (a raw test/utility
    // setter).
    void addRuneFragments(int amount);

    // --- Prestige promotion (bonus feature; Requirements 14.1/14.2) -------
    // Rune-Fragment total at which the Arcanist promotes ONCE this run.
    static constexpr int kPromotionRfThreshold = 10;

    // True once the Arcanist has promoted this run. A fresh Model (used by a
    // new run / restart) starts false, so promotion does not persist across
    // restarts; it DOES persist across level descents since the same Model and
    // base Arcanist are retained.
    bool promoted() const { return promoted_; }

    // --- Bonus feature gate (project spec: WITH/WITHOUT bonus, no recompile) --
    // The Prestige promotion is a BONUS feature and must NOT trigger in the
    // spec-compliant base game. `bonusEnabled_` gates maybePromote(): promotion
    // only ever happens while it is true. It defaults to false (base game); the
    // Controller (Game) copies its own `-bonus` setting onto the Model each run
    // via setBonusEnabled (a fresh Model, built by resetRun(), resets it to
    // false, so it must be re-applied on every new/restarted run).
    void setBonusEnabled(bool v) { bonusEnabled_ = v; }
    bool bonusEnabled() const { return bonusEnabled_; }

    // --- Run flags --------------------------------------------------------
    bool lichAggroTriggered() const { return lichAggroTriggered_; }
    void setLichAggroTriggered(bool v) { lichAggroTriggered_ = v; }

    bool carryingCipherGem() const { return carryingCipherGem_; }
    void setCarryingCipherGem(bool v) { carryingCipherGem_ = v; }

    // --- Dropped Cipher Gem (task 7.8, Requirement 6.12) -----------------
    // When the Spectre carrying the level's Cipher Gem is defeated, the gem
    // drops on that Spectre's tile (Requirement 6.12). Until the concrete
    // CipherGem Item class and its walk-over pickup exist (tasks 13.2/13.3),
    // the drop is represented model-side by recording that a gem has been
    // dropped and the tile it landed on. `handleAttack` sets this when a
    // carrier dies at its position.
    //
    // NOTE (task 13.3): this will be replaced/augmented by placing an actual
    // CipherGem Item at cipherGemDropPosition() and wiring walk-over pickup
    // (which sets carryingCipherGem + reveals the stairway). For now, recording
    // the drop position is the model-side stand-in for "the gem is on the floor
    // at that tile".
    bool cipherGemDropped() const { return cipherGemDropped_; }
    Position cipherGemDropPosition() const { return cipherGemDropPos_; }

    // Record that the Cipher Gem was dropped on tile `p` (carrier died there).
    void setCipherGemDrop(const Position &p) {
        cipherGemDropped_ = true;
        cipherGemDropPos_ = p;
    }
    // Clear the recorded drop (e.g. after the gem is picked up, task 13.3).
    void clearCipherGemDrop() {
        cipherGemDropped_ = false;
        cipherGemDropPos_ = Position{0, 0};
    }

    // --- Stairway reveal (task 6.5, Requirements 2.6/2.7) ----------------
    // The current level's stairway renders as a floor tile `.` until the Cipher
    // Gem is carried, after which the View renders it as `\` by reading
    // Level::stairwayRevealed(). This helper performs the model-side reveal:
    // when the Arcanist is carrying the Cipher Gem and a Level is set, it marks
    // the level's stairway as revealed. It is idempotent (safe to call every
    // turn) and does nothing while the gem is not carried, so a mistaken reveal
    // can never leak. `moveArcanist` calls it after every successful move, and
    // the future Cipher-Gem pickup code (task 13.3) can call it immediately on
    // pickup so the stairway is revealed the moment the gem is claimed.
    void revealStairwayIfCarryingGem();

    bool carryingAegisCloak() const { return carryingAegisCloak_; }
    void setCarryingAegisCloak(bool v) { carryingAegisCloak_ = v; }

    bool aegisSpawnedThisRun() const { return aegisSpawnedThisRun_; }
    void setAegisSpawnedThisRun(bool v) { aegisSpawnedThisRun_ = v; }

    // The level number on which the Aegis Cloak spawned this run (0 = none yet).
    int aegisLevel() const { return aegisLevel_; }
    void setAegisLevel(int n) { aegisLevel_ = n; }

    // --- Identified scrolls (task 12.6, Requirements 8.9/8.10, DD-3) -----
    // Requirement 8.10: the first time a scroll of a given type is used its
    // type becomes "remembered" (identified) for the REMAINDER of the run.
    // useScroll marks the used scroll's type on every successful use; because
    // the backing store is a std::set that never removes entries, a type stays
    // identified once marked — it persists across all subsequent actions until
    // the run ends. Identification is keyed by `ScrollType` (the intended,
    // type-safe key).
    //
    // Requirement 8.9 / DD-3: identification is purely bookkeeping — it NEVER
    // affects the board glyph. A Scroll always renders as `?` regardless of
    // whether its type has been identified (see Scroll::symbol()). The
    // identified set is what the status area (surfaced by the Controller in
    // task 17.x) can consult to show which scroll effects the player has
    // learned; the board rendering is untouched.
    void markScrollIdentified(ScrollType type) {
        identifiedScrolls_.insert(type);
    }
    bool isScrollIdentified(ScrollType type) const {
        return identifiedScrolls_.count(type) != 0;
    }

    // Backwards-compatible int overloads. `ScrollType` is the intended key
    // (above); these thin wrappers keep pre-existing call sites that pass a raw
    // scroll-type id (e.g. static_cast<int>(ScrollType::...)) compiling. The int
    // is interpreted as the underlying ScrollType enumerator value.
    void markScrollIdentified(int scrollType) {
        markScrollIdentified(static_cast<ScrollType>(scrollType));
    }
    bool isScrollIdentified(int scrollType) const {
        return isScrollIdentified(static_cast<ScrollType>(scrollType));
    }

    // --- Run outcome (task 7.6, Requirements 10.4/10.5) ------------------
    // The current run outcome. Starts `Playing`; set to `Lost` when the
    // Arcanist's FP reaches 0 (task 7.6). `Won` is reserved for task 16.3.
    RunState runState() const { return runState_; }
    void setRunState(RunState state) { runState_ = state; }

    // Convenience: true once the run has ended in a loss (Requirement 10.5).
    bool isRunLost() const { return runState_ == RunState::Lost; }

    // --- Loss check (task 16.3, Requirement 10.5) ------------------------
    // End the run as a loss whenever the Arcanist's FP has reached 0. This is
    // the single helper every FP-reducing path funnels through: the Spectre
    // attack resolver (resolveSpectreAttack) and permanent FP drains
    // (useScroll's Drain Focus) call it after applying damage. It only ever
    // SETS RunState::Lost (never clears it), so once the run is Lost it stays
    // Lost (monotonic). A no-op while there is no Arcanist or the Arcanist is
    // still alive.
    void checkLoss();

    // --- Prestige promotion trigger (bonus feature; Requirements 14.1/14.2) -
    // Promote the base Arcanist ONCE when the run's Rune-Fragment total first
    // reaches kPromotionRfThreshold. Called at the end of addRuneFragments (the
    // sole gameplay RF-gain funnel). Guards: does nothing when there is no base
    // Arcanist, when the Arcanist has already promoted this run, when it has no
    // class Strategy, or while the total is still below the threshold. On a
    // promotion it swaps the class Strategy + buffs all stats via
    // PromotionFactory::promote, sets promoted_, and pushes a notification.
    void maybePromote();

    // --- Scoring (task 9.2, Requirements 13.1/13.2/13.3) -----------------
    // A score exists ONLY when the run has ended in a win. Requirement 13.3
    // states that a run ending by loss, restart, or quit produces NO score:
    // loss leaves runState() == RunState::Lost, and restart/quit are
    // Controller-level actions (tasks 17.x) that never reach RunState::Won, so
    // the model-side contract is simply "a score exists iff runState() == Won".
    bool hasScore() const { return runState_ == RunState::Won; }

    // The final score for the run. When the run has been won (hasScore()), this
    // is the total Rune Fragments collected (Requirement 13.1) passed through
    // the Arcanist class scoreModifier — the Sage/Archmage +50% bonus
    // (Requirement 13.2) is applied by the strategy, all other classes leave it
    // unchanged. When the run is not in the Won state (Playing/Lost, or a
    // restart/quit that never reached Won), no score is produced and this
    // returns 0 (Requirement 13.3). If a won Arcanist has no attached class
    // Strategy, the raw fragment total is returned unmodified (identity).
    int finalScore() const;

private:
    std::shared_ptr<Arcanist> baseArcanist_;
    // Head of the temporary-scroll effective-stat Decorator chain (task 12.1).
    // Rebuilt as a BaseArcanistComponent whenever the base Arcanist is set.
    std::shared_ptr<ArcanistComponent> activeArcanist_;
    std::unique_ptr<Level> level_;
    int levelNumber_;
    int runeFragmentTotal_;
    bool promoted_ = false; // Prestige promotion fired this run (task 14.x)
    bool bonusEnabled_ = false; // bonus features gate (Prestige promotion)
    bool lichAggroTriggered_;
    bool carryingCipherGem_;
    bool carryingAegisCloak_;
    bool aegisSpawnedThisRun_;
    int aegisLevel_;
    bool cipherGemDropped_;      // a defeated carrier dropped the gem (task 7.8)
    Position cipherGemDropPos_;  // tile the dropped Cipher Gem landed on
    Position arcanistPos_; // Arcanist's board position (task 6.1)
    // Scroll types identified (remembered) this run (task 12.6, Requirement
    // 8.10). Keyed by ScrollType; a std::set never un-marks, so a type stays
    // identified for the remainder of the run once used.
    std::set<ScrollType> identifiedScrolls_;
    RunState runState_; // run outcome (task 7.6); starts Playing
    // Non-owning pointer to the parsed layout file when a file drives the run
    // (task 19.1). Null = random default-layout path. Owned by the Controller.
    const LoadedLayout *layout_ = nullptr;
};

} // namespace runevault

#endif // RUNEVAULT_MODEL_H
