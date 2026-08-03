// combat_visitor.h
// RuneVault — the CombatVisitor: Spectre-specific combat resolution via the
// Visitor pattern (task 10.2, design DQ3 "Spectre abilities: Visitor").
//
// Combat outcomes vary by the CONCRETE Spectre type without permanently
// mutating the Arcanist or spawning many short-lived decorator objects
// (design DQ3 tradeoff). Each concrete Spectre overrides `accept(CombatVisitor&)`
// to call back `v.visit(*this)`, selecting the matching `visit` overload below
// (double dispatch on the concrete type).
//
// WHEN IS THE VISITOR INVOKED? It runs when the ARCANIST ATTACKS a Spectre
// (Model::handleAttack, task 7.5). That is precisely when Lich aggro must trip
// (Requirement 7.7: "WHEN any Lich is attacked ..."), so `visit(Lich&)` sets
// the global `lichAggroTriggered` flag. The visitor fires on every attack,
// whether or not the hit is lethal.
//
// SCOPE (this task):
//   * visit(Lich&)       — trips the run-scoped Lich aggro flag (Requirement 7.7 / DD-2).
//   * visit(VaultAnchor&) — documented hook for unlocking guarded items on Anchor
//                           death (Requirement 6.12 / design). The guarded-item
//                           State machine (Locked->Unlocked) is task 13.2 and the
//                           actual wiring is task 13.3; for now this is a no-op hook.
//   * all other visits    — no-ops. The design "e.g." abilities (Wraith FP drain,
//                           Banshee ward reduction, Shade rune-fragment theft) are
//                           optional flavor, not required by 6.12/7.7, so they are
//                           left as TODOs.
//
// DEPENDENCY NOTES:
//   * design DQ3 shows `CombatVisitor(Model&, ArcanistComponent&)`. The scroll
//     Decorator `ArcanistComponent` is task 12.1 and does NOT exist yet, so this
//     visitor takes ONLY `Model&` for now (it can reach the Arcanist via
//     model.arcanist()). Task 12.1 may later add/pass the decorated
//     ArcanistComponent.
//   * To avoid a header cycle we forward-declare `class Model;` here and include
//     model.h only in combat_visitor.cpp. spectre.h is included for the concrete
//     Spectre types named in the overloads.

#ifndef RUNEVAULT_COMBAT_VISITOR_H
#define RUNEVAULT_COMBAT_VISITOR_H

#include "runevault/spectre.h"

namespace runevault {

// Forward-declared to break the model.h <-> combat_visitor.h include cycle; the
// definition is pulled in by combat_visitor.cpp only.
class Model;

// Resolves a Spectre's combat effects by concrete type (double-dispatch). One
// `visit` overload per concrete Spectre kind (design DQ3). `virtual` so a later
// bonus could subclass it; the base implementation is the game's default.
class CombatVisitor {
public:
    // Construct around the run's Model. The visitor observes/mutates run-scoped
    // state (e.g. the Lich aggro flag) through it. TODO(task 12.1): may also
    // accept the decorated ArcanistComponent once the scroll Decorator exists.
    explicit CombatVisitor(Model& model);
    virtual ~CombatVisitor() = default;

    // One overload per concrete Spectre type (double dispatch via accept()).
    virtual void visit(Wraith& s);
    virtual void visit(Banshee& s);
    virtual void visit(Revenant& s);
    virtual void visit(Shade& s);
    virtual void visit(Lich& s);        // trips lichAggroTriggered (Requirement 7.7)
    virtual void visit(VaultAnchor& s); // unlock-guarded-items hook (Requirement 6.12)
    virtual void visit(SpecterLord& s);

private:
    Model& model_;
};

} // namespace runevault

#endif // RUNEVAULT_COMBAT_VISITOR_H
