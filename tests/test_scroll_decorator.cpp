// test_scroll_decorator.cpp
// RuneVault — Arcane Scrolls Decorator chain + Model effective stats (task 12.1).
//
// Covers the temporary-effect scroll decorators (Requirements 8.5/8.6/8.7/8.8)
// and their integration into the Model as `activeArcanist_` / effectivePwr() /
// effectiveWr(). Uses the tiny assert-based harness (test_util.h); no main()
// here — tests/test_main.cpp drives run_all().

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/arcanist_component.h"
#include "runevault/combat.h"
#include "runevault/level.h"
#include "runevault/model.h"
#include "runevault/spectre.h"

#include <memory>

using namespace runevault;

namespace {

std::shared_ptr<ArcanistComponent> makeBase(int pwr, int wr) {
    auto arc = std::make_shared<Arcanist>(/*fp*/ 20, pwr, wr);
    return std::make_shared<BaseArcanistComponent>(arc);
}

std::unique_ptr<GenericSpectre> makeSpectre(const Position &pos, int fp,
                                            int pwr, int wr) {
    return std::unique_ptr<GenericSpectre>(new GenericSpectre(
        SpectreType::Wraith, fp, pwr, wr, /*spawnChamber*/ 1, pos, 'W'));
}

} // namespace

// --- BaseArcanistComponent: identity ------------------------------------

RV_TEST(base_component_reports_base_pwr_and_wr) {
    auto base = makeBase(/*pwr*/ 12, /*wr*/ 7);
    RV_CHECK(base->effPwr() == 12);
    RV_CHECK(base->effWr() == 7);
    // base() reaches the underlying Arcanist.
    RV_CHECK(base->base().pwr() == 12);
    RV_CHECK(base->base().wr() == 7);
}

// --- Requirement 8.5: Surge Power adds 5 to Pwr, WR unchanged ------------

RV_TEST(surge_power_adds_five_to_pwr_only) {
    auto base = makeBase(/*pwr*/ 10, /*wr*/ 8);
    auto surged = std::make_shared<SurgePower>(base);
    RV_CHECK(surged->effPwr() == 15); // +5
    RV_CHECK(surged->effWr() == 8);   // unchanged
}

// --- Requirement 8.6: Fortify Ward adds 5 to WR, Pwr unchanged -----------

RV_TEST(fortify_ward_adds_five_to_wr_only) {
    auto base = makeBase(/*pwr*/ 10, /*wr*/ 8);
    auto fortified = std::make_shared<FortifyWard>(base);
    RV_CHECK(fortified->effWr() == 13); // +5
    RV_CHECK(fortified->effPwr() == 10); // unchanged
}

// --- Requirement 8.7: Sap Power subtracts 5 from Pwr, floored at 0 -------

RV_TEST(sap_power_subtracts_five_from_pwr_only) {
    auto base = makeBase(/*pwr*/ 10, /*wr*/ 8);
    auto sapped = std::make_shared<SapPower>(base);
    RV_CHECK(sapped->effPwr() == 5); // -5
    RV_CHECK(sapped->effWr() == 8);  // unchanged
}

RV_TEST(sap_power_floors_pwr_at_zero_not_negative) {
    auto base = makeBase(/*pwr*/ 3, /*wr*/ 8);
    auto sapped = std::make_shared<SapPower>(base);
    RV_CHECK(sapped->effPwr() == 0); // max(0, 3 - 5) == 0, not -2
}

// --- Requirement 8.8: Erode Ward subtracts 5 from WR, floored at 0 -------

RV_TEST(erode_ward_subtracts_five_from_wr_only) {
    auto base = makeBase(/*pwr*/ 10, /*wr*/ 8);
    auto eroded = std::make_shared<ErodeWard>(base);
    RV_CHECK(eroded->effWr() == 3);   // -5
    RV_CHECK(eroded->effPwr() == 10); // unchanged
}

RV_TEST(erode_ward_floors_wr_at_zero_not_negative) {
    auto base = makeBase(/*pwr*/ 10, /*wr*/ 2);
    auto eroded = std::make_shared<ErodeWard>(base);
    RV_CHECK(eroded->effWr() == 0); // max(0, 2 - 5) == 0, not -3
}

// --- Stacking + cancellation + parameterized delta -----------------------

RV_TEST(stacking_two_surge_power_adds_ten) {
    auto base = makeBase(/*pwr*/ 10, /*wr*/ 8);
    auto once = std::make_shared<SurgePower>(base);
    auto twice = std::make_shared<SurgePower>(once);
    RV_CHECK(twice->effPwr() == 20); // +5 +5
    RV_CHECK(twice->effWr() == 8);   // still unchanged
}

RV_TEST(surge_then_sap_cancels_out) {
    auto base = makeBase(/*pwr*/ 10, /*wr*/ 8);
    auto surged = std::make_shared<SurgePower>(base);
    auto sapped = std::make_shared<SapPower>(surged);
    RV_CHECK(sapped->effPwr() == 10); // +5 then -5 nets to base
}

RV_TEST(parameterized_delta_adds_and_subtracts_the_given_amount) {
    auto base = makeBase(/*pwr*/ 10, /*wr*/ 10);
    // Hexblade-style doubled magnitude (delta = 10), routed by task 12.4.
    auto surged = std::make_shared<SurgePower>(base, /*delta*/ 10);
    RV_CHECK(surged->effPwr() == 20); // +10

    auto sapped = std::make_shared<SapPower>(base, /*delta*/ 10);
    RV_CHECK(sapped->effPwr() == 0); // max(0, 10 - 10) == 0

    auto fortified = std::make_shared<FortifyWard>(base, /*delta*/ 10);
    RV_CHECK(fortified->effWr() == 20); // +10

    auto eroded = std::make_shared<ErodeWard>(base, /*delta*/ 10);
    RV_CHECK(eroded->effWr() == 0); // max(0, 10 - 10) == 0
}

RV_TEST(base_reachable_through_a_decorator_chain) {
    auto base = makeBase(/*pwr*/ 10, /*wr*/ 8);
    auto chain = std::make_shared<FortifyWard>(std::make_shared<SurgePower>(base));
    // base() drills all the way down to the real Arcanist regardless of depth.
    RV_CHECK(chain->base().pwr() == 10);
    RV_CHECK(chain->base().wr() == 8);
}

// --- Model integration: effectivePwr()/effectiveWr() ---------------------

RV_TEST(model_effective_stats_default_to_base_after_set_arcanist) {
    Model model;
    model.setArcanist(std::make_shared<Arcanist>(/*fp*/ 20, /*pwr*/ 14, /*wr*/ 9));
    RV_CHECK(model.activeArcanist() != nullptr); // chain head built by setArcanist
    RV_CHECK(model.effectivePwr() == 14); // no decorators => effective == base
    RV_CHECK(model.effectiveWr() == 9);
}

RV_TEST(model_effective_stats_guard_when_no_arcanist) {
    Model model;
    RV_CHECK(model.activeArcanist() == nullptr);
    RV_CHECK(model.effectivePwr() == 0); // safe fallback
    RV_CHECK(model.effectiveWr() == 0);
}

RV_TEST(model_wrapping_active_arcanist_reflects_in_effective_stats) {
    Model model;
    model.setArcanist(std::make_shared<Arcanist>(/*fp*/ 20, /*pwr*/ 14, /*wr*/ 9));

    // Wrap the current head with a Surge Power decorator (as scroll-use 12.2 will).
    model.setActiveArcanist(
        std::make_shared<SurgePower>(model.activeArcanistPtr()));
    RV_CHECK(model.effectivePwr() == 19); // +5
    RV_CHECK(model.effectiveWr() == 9);   // unchanged

    // Reset to base (as level teardown 16.1 will) removes the effect.
    model.resetActiveArcanistToBase();
    RV_CHECK(model.effectivePwr() == 14);
    RV_CHECK(model.effectiveWr() == 9);
}

RV_TEST(model_attack_uses_effective_pwr_after_surge) {
    // Arcanist at (2,3); target one step East is (2,4).
    const Position arc{2, 3};
    const int basePwr = 30;
    const int spWr = 50;

    Model model;
    model.setArcanist(std::make_shared<Arcanist>(/*fp*/ 20, basePwr, /*wr*/ 0));
    model.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model.setArcanistPosition(arc);

    const Position target{2, 4};
    model.level()->addSpectre(makeSpectre(target, /*fp*/ 200, /*pwr*/ 5, spWr));

    // Apply Surge Power: effective Pwr becomes basePwr + 5.
    model.setActiveArcanist(
        std::make_shared<SurgePower>(model.activeArcanistPtr()));
    RV_CHECK(model.effectivePwr() == basePwr + 5);

    const int before = model.level()->spectreAt(target)->fp();
    RV_CHECK(model.handleAttack(Direction::Ea) == true);
    const int expected = computeDamage(basePwr + 5, spWr); // effective Pwr drives it
    const int after = model.level()->spectreAt(target)->fp();
    RV_CHECK(after == before - expected);
    // And the buffed hit exceeds the base-Pwr hit.
    RV_CHECK(expected > computeDamage(basePwr, spWr));
}

RV_TEST(model_spectre_damage_uses_effective_wr_after_fortify) {
    const Position arc{2, 3};
    Model model;
    model.setArcanist(std::make_shared<Arcanist>(/*fp*/ 50, /*pwr*/ 5, /*wr*/ 0));
    model.setLevel(std::unique_ptr<Level>(new Level(makeDefaultBoard())));
    model.setArcanistPosition(arc);

    auto spectre = makeSpectre(Position{2, 4}, /*fp*/ 100, /*pwr*/ 40, /*wr*/ 0);
    const int baseDmg = model.spectreDamageIfHit(*spectre);

    // Fortify Ward raises effective WR, which should reduce incoming damage.
    model.setActiveArcanist(
        std::make_shared<FortifyWard>(model.activeArcanistPtr(), /*delta*/ 50));
    RV_CHECK(model.effectiveWr() == 50);
    const int fortifiedDmg = model.spectreDamageIfHit(*spectre);
    RV_CHECK(fortifiedDmg < baseDmg); // more ward => less damage taken
}
