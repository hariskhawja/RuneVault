// test_spectre.cpp
// RuneVault — tests for the abstract Spectre base via GenericSpectre (task 5.3).
//
// Exercises the base behaviour through the concrete placeholder: construction
// stores identity/stats/chamber/position; takeDamage reduces FP and floors at
// 0; isDead reports FP==0; moveTo updates position; the virtual policy defaults
// (isMobile, runeDrop, isDefaultHostile) hold; and symbol() returns the
// placeholder's glyph (Requirement 2.5).

#include "test_util.h"

#include "runevault/spectre.h"

RV_TEST(spectre_constructor_stores_type_stats_chamber_and_position) {
    using namespace runevault;
    GenericSpectre s(SpectreType::Wraith, 60, 20, 10, 3, Position{7, 4}, 'W');
    RV_CHECK(s.type() == SpectreType::Wraith);
    RV_CHECK(s.fp() == 60);
    RV_CHECK(s.pwr() == 20);
    RV_CHECK(s.wr() == 10);
    RV_CHECK(s.spawnChamber() == 3);
    RV_CHECK(s.position() == (Position{7, 4}));
}

RV_TEST(spectre_symbol_returns_placeholder_glyph) {
    using namespace runevault;
    GenericSpectre s(SpectreType::SpecterLord, 80, 28, 15, 1, Position{0, 0},
                     'X');
    RV_CHECK(s.symbol() == 'X');
}

RV_TEST(spectre_takedamage_reduces_fp) {
    using namespace runevault;
    GenericSpectre s(SpectreType::Revenant, 100, 18, 20, 2, Position{1, 1}, 'R');
    s.takeDamage(30);
    RV_CHECK(s.fp() == 70);
    RV_CHECK(!s.isDead());
    s.takeDamage(30);
    RV_CHECK(s.fp() == 40);
}

RV_TEST(spectre_takedamage_floors_fp_at_zero) {
    using namespace runevault;
    GenericSpectre s(SpectreType::Banshee, 40, 30, 5, 4, Position{2, 2}, 'B');
    s.takeDamage(1000); // overkill must not drive FP negative
    RV_CHECK(s.fp() == 0);
    RV_CHECK(s.isDead());
}

RV_TEST(spectre_isdead_true_exactly_at_zero_fp) {
    using namespace runevault;
    GenericSpectre s(SpectreType::Shade, 50, 8, 8, 0, Position{3, 3}, 'S');
    RV_CHECK(!s.isDead());
    s.takeDamage(50);
    RV_CHECK(s.fp() == 0);
    RV_CHECK(s.isDead());
}

RV_TEST(spectre_moveto_updates_position) {
    using namespace runevault;
    GenericSpectre s(SpectreType::Lich, 30, 40, 5, 2, Position{5, 5}, 'L');
    s.moveTo(Position{6, 7});
    RV_CHECK(s.position() == (Position{6, 7}));
}

RV_TEST(spectre_default_policy_hooks) {
    using namespace runevault;
    GenericSpectre s(SpectreType::Wraith, 60, 20, 10, 1, Position{4, 4}, 'W');
    RV_CHECK(s.isMobile() == true);
    RV_CHECK(s.runeDrop() == 1);
    RV_CHECK(s.isDefaultHostile() == true);
}
