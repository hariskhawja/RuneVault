#ifndef RUNEVAULT_COMBAT_VISITOR_H
#define RUNEVAULT_COMBAT_VISITOR_H

#include "runevault/spectre.h"

namespace runevault {

// Forward-declared to break the model.h <-> combat_visitor.h include cycle; the
// definition is pulled in by combat_visitor.cpp only.
class Model;

// Spectre::accept selects the type-specific attack side effect.
class CombatVisitor {
public:
    explicit CombatVisitor(Model& model);
    virtual ~CombatVisitor() = default;

    // One overload per concrete Spectre type (double dispatch via accept()).
    virtual void visit(Wraith& s);
    virtual void visit(Banshee& s);
    virtual void visit(Revenant& s);
    virtual void visit(Shade& s);
    virtual void visit(Lich& s);
    virtual void visit(VaultAnchor& s);
    virtual void visit(SpecterLord& s);

private:
    Model& model_;
};

} // namespace runevault

#endif // RUNEVAULT_COMBAT_VISITOR_H
