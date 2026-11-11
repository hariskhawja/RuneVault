#ifndef RUNEVAULT_ARCANIST_COMPONENT_H
#define RUNEVAULT_ARCANIST_COMPONENT_H

#include <algorithm>
#include <memory>
#include <utility>

#include "runevault/arcanist.h"

namespace runevault {

// Scroll decorators wrap the base Arcanist; dropping the chain clears temporary stats.
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

class SurgePower : public ScrollDecorator {
public:
    explicit SurgePower(std::shared_ptr<ArcanistComponent> inner, int delta = 5)
        : ScrollDecorator(std::move(inner)), delta_(delta) {}

    int effPwr() const override { return inner_->effPwr() + delta_; }
    // effWr() inherited (passthrough).

private:
    int delta_;
};

class FortifyWard : public ScrollDecorator {
public:
    explicit FortifyWard(std::shared_ptr<ArcanistComponent> inner, int delta = 5)
        : ScrollDecorator(std::move(inner)), delta_(delta) {}

    int effWr() const override { return inner_->effWr() + delta_; }
    // effPwr() inherited (passthrough).

private:
    int delta_;
};

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
