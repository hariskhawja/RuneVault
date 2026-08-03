// arcanist_component.h
// RuneVault — Arcane Scrolls with the Decorator pattern (task 12.1).
//
// This header defines the Decorator chain that supplies the EFFECTIVE Power
// (Pwr) and Ward Rating (WR) the combat layer reads while temporary scroll
// effects are active for the CURRENT LEVEL (design "Arcane Scrolls with the
// Decorator pattern"; Requirements 8.5-8.8). The chain is:
//
//     head -> [ScrollDecorator...] -> BaseArcanistComponent -> Arcanist
//
//   * `ArcanistComponent` is the abstract component interface. It exposes
//     `effPwr()` / `effWr()` (effective stats including any active buffs /
//     debuffs) and `base()` (reach the underlying persistent Arcanist for FP /
//     strategy access, which are NOT part of the effective-stat chain).
//   * `BaseArcanistComponent` is the concrete leaf at the bottom of the chain.
//     It adapts a `shared_ptr<Arcanist>` and reports the Arcanist's base
//     pwr()/wr() as the effective stats. This is the identity of the chain.
//   * `ScrollDecorator` is the abstract decorator. It holds a
//     `shared_ptr<ArcanistComponent> inner_` and, by default, forwards every
//     call to the wrapped component. A concrete decorator overrides only the
//     stat it changes; the other passes through unchanged.
//   * `SurgePower` / `FortifyWard` / `SapPower` / `ErodeWard` are the four
//     temporary-effect decorators (Requirements 8.5/8.6/8.7/8.8). The debuff
//     decorators (`SapPower`, `ErodeWard`) FLOOR the effective stat at 0 inside
//     `effPwr()`/`effWr()` (`max(0, ...)`), so a decorated chain NEVER reports a
//     negative Pwr/WR (design note under the decorator table).
//
// DESIGN NOTE — parameterized magnitude (DQ4): the +5 / -5 magnitudes here are
// the BASE scroll magnitudes. The class-ability adjustments (Hexblade's doubled
// magnitude, Voidwalker's flipped sign — task 12.4) route through the class
// Strategy's `scrollEffectModifier` and adjust the DELTA (and, for a flip, WHICH
// decorator) BEFORE the decorator is constructed. To support that cleanly each
// decorator's constructor takes the delta amount, defaulting to 5, so task 12.4
// can pass e.g. 10 for a Hexblade. The decorators themselves stay dumb: they add
// or subtract exactly the delta they were given and never look at the class.
//
// OWNERSHIP / TEARDOWN: the whole chain is held together by shared_ptr, and the
// base Arcanist is separately owned by `Model::baseArcanist_`. On a level
// transition (task 16.1) the Model resets the chain head back to a fresh
// BaseArcanistComponent (or to the base), dropping every decorator's refcount to
// zero so the temporary effects tear down deterministically while the base
// Arcanist — and its permanent FP changes — survive.

#ifndef RUNEVAULT_ARCANIST_COMPONENT_H
#define RUNEVAULT_ARCANIST_COMPONENT_H

#include <algorithm> // std::max
#include <memory>
#include <utility>

#include "runevault/arcanist.h"

namespace runevault {

// Abstract component interface shared by the adapted base Arcanist and every
// ScrollDecorator. Callers read `effPwr()`/`effWr()` for combat and reach the
// underlying persistent Arcanist through `base()` for FP / strategy access.
class ArcanistComponent {
public:
    virtual ~ArcanistComponent() = default;

    // Effective Power / Ward Rating including any active current-level scroll
    // buffs or debuffs. Never negative (debuff decorators floor at 0).
    virtual int effPwr() const = 0;
    virtual int effWr() const = 0;

    // Reach the underlying base Arcanist (for FP, class strategy, etc.). These
    // stats are permanent and deliberately sit OUTSIDE the effective-stat chain.
    virtual Arcanist &base() = 0;
    virtual const Arcanist &base() const = 0;
};

// Concrete leaf: the bottom of the decorator chain. Adapts a shared base
// Arcanist and reports its base pwr()/wr() as the effective stats (identity).
class BaseArcanistComponent : public ArcanistComponent {
public:
    explicit BaseArcanistComponent(std::shared_ptr<Arcanist> arcanist)
        : arcanist_(std::move(arcanist)) {}

    int effPwr() const override { return arcanist_->pwr(); }
    int effWr() const override { return arcanist_->wr(); }

    Arcanist &base() override { return *arcanist_; }
    const Arcanist &base() const override { return *arcanist_; }

    // Access the shared base pointer (used by the Model when it needs the
    // shared_ptr itself, not just a reference).
    const std::shared_ptr<Arcanist> &arcanist() const { return arcanist_; }

private:
    std::shared_ptr<Arcanist> arcanist_;
};

// Abstract decorator: wraps an inner component and, by default, forwards every
// call. A concrete decorator overrides only the stat it modifies. `base()`
// always delegates down the chain to the real Arcanist.
class ScrollDecorator : public ArcanistComponent {
public:
    explicit ScrollDecorator(std::shared_ptr<ArcanistComponent> inner)
        : inner_(std::move(inner)) {}

    // Default: no change — pass both stats through untouched.
    int effPwr() const override { return inner_->effPwr(); }
    int effWr() const override { return inner_->effWr(); }

    Arcanist &base() override { return inner_->base(); }
    const Arcanist &base() const override { return inner_->base(); }

protected:
    std::shared_ptr<ArcanistComponent> inner_; // shared_ptr chain toward base
};

// Surge Power (Requirement 8.5): temporarily ADD to effective Power for the
// current level; Ward Rating passes through. `delta` defaults to the base +5
// magnitude; task 12.4 passes a larger delta (e.g. 10) for a Hexblade.
class SurgePower : public ScrollDecorator {
public:
    explicit SurgePower(std::shared_ptr<ArcanistComponent> inner, int delta = 5)
        : ScrollDecorator(std::move(inner)), delta_(delta) {}

    int effPwr() const override { return inner_->effPwr() + delta_; }
    // effWr() inherited (passthrough).

private:
    int delta_;
};

// Fortify Ward (Requirement 8.6): temporarily ADD to effective Ward Rating for
// the current level; Power passes through. `delta` defaults to +5.
class FortifyWard : public ScrollDecorator {
public:
    explicit FortifyWard(std::shared_ptr<ArcanistComponent> inner, int delta = 5)
        : ScrollDecorator(std::move(inner)), delta_(delta) {}

    int effWr() const override { return inner_->effWr() + delta_; }
    // effPwr() inherited (passthrough).

private:
    int delta_;
};

// Sap Power (Requirement 8.7): temporarily SUBTRACT from effective Power for
// the current level, FLOORED AT 0 (never negative); Ward Rating passes through.
// `delta` defaults to the base -5 magnitude.
class SapPower : public ScrollDecorator {
public:
    explicit SapPower(std::shared_ptr<ArcanistComponent> inner, int delta = 5)
        : ScrollDecorator(std::move(inner)), delta_(delta) {}

    int effPwr() const override {
        return std::max(0, inner_->effPwr() - delta_); // floor at 0 (8.7)
    }
    // effWr() inherited (passthrough).

private:
    int delta_;
};

// Erode Ward (Requirement 8.8): temporarily SUBTRACT from effective Ward Rating
// for the current level, FLOORED AT 0 (never negative); Power passes through.
// `delta` defaults to the base -5 magnitude.
class ErodeWard : public ScrollDecorator {
public:
    explicit ErodeWard(std::shared_ptr<ArcanistComponent> inner, int delta = 5)
        : ScrollDecorator(std::move(inner)), delta_(delta) {}

    int effWr() const override {
        return std::max(0, inner_->effWr() - delta_); // floor at 0 (8.8)
    }
    // effPwr() inherited (passthrough).

private:
    int delta_;
};

} // namespace runevault

#endif // RUNEVAULT_ARCANIST_COMPONENT_H
