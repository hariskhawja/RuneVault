// test_arcanist.cpp
// RuneVault — assert-based tests for the Arcanist stat container (task 5.1).
//
// Covers construction (max FP == starting FP, Requirement 1.8), FP mutation
// (reduceFp / applyDamage / healUpTo / loseUpTo) and their clamping, base stat
// mutation (addPwr / addWr) with a non-negative floor, and the isDead flag.

#include "test_util.h"

#include "runevault/arcanist.h"

RV_TEST(arcanist_construction_sets_fp_equal_to_max) {
    using namespace runevault;
    Arcanist a{40, 6, 3};
    RV_CHECK(a.fp() == 40);
    RV_CHECK(a.maxFp() == 40);
    RV_CHECK(a.fp() == a.maxFp());
    RV_CHECK(a.pwr() == 6);
    RV_CHECK(a.wr() == 3);
    RV_CHECK(!a.isDead());
}

RV_TEST(arcanist_construction_floors_negatives_at_zero) {
    using namespace runevault;
    Arcanist a{-5, -2, -1};
    RV_CHECK(a.fp() == 0);
    RV_CHECK(a.maxFp() == 0);
    RV_CHECK(a.pwr() == 0);
    RV_CHECK(a.wr() == 0);
    RV_CHECK(a.isDead()); // fp == 0
}

RV_TEST(arcanist_reduce_fp_subtracts_and_floors_at_zero) {
    using namespace runevault;
    Arcanist a{20, 5, 5};
    a.reduceFp(8);
    RV_CHECK(a.fp() == 12);
    a.reduceFp(100); // over-reduce floors at 0, never negative
    RV_CHECK(a.fp() == 0);
    a.reduceFp(-3); // negative amount never heals
    RV_CHECK(a.fp() == 0);
}

RV_TEST(arcanist_apply_damage_floors_at_zero) {
    using namespace runevault;
    Arcanist a{15, 5, 5};
    a.applyDamage(10);
    RV_CHECK(a.fp() == 5);
    a.applyDamage(50);
    RV_CHECK(a.fp() == 0);
    RV_CHECK(a.isDead());
}

RV_TEST(arcanist_heal_up_to_caps_at_max_fp) {
    using namespace runevault;
    Arcanist a{30, 5, 5};
    a.reduceFp(25); // fp == 5
    a.healUpTo(10); // 5 + 10 = 15, below max
    RV_CHECK(a.fp() == 15);
    a.healUpTo(100); // caps at maxFp (30)
    RV_CHECK(a.fp() == 30);
    RV_CHECK(a.maxFp() == 30);
    a.healUpTo(-5); // negative amount is a no-op
    RV_CHECK(a.fp() == 30);
}

RV_TEST(arcanist_lose_up_to_floors_at_zero) {
    using namespace runevault;
    Arcanist a{12, 5, 5};
    a.loseUpTo(4);
    RV_CHECK(a.fp() == 8);
    a.loseUpTo(100); // floors at 0
    RV_CHECK(a.fp() == 0);
    a.loseUpTo(-2); // negative amount is a no-op
    RV_CHECK(a.fp() == 0);
}

RV_TEST(arcanist_add_pwr_and_wr_adjust_and_floor_at_zero) {
    using namespace runevault;
    Arcanist a{20, 5, 3};
    a.addPwr(5);
    RV_CHECK(a.pwr() == 10);
    a.addWr(2);
    RV_CHECK(a.wr() == 5);
    a.addPwr(-100); // floors at 0
    RV_CHECK(a.pwr() == 0);
    a.addWr(-100); // floors at 0
    RV_CHECK(a.wr() == 0);
}

RV_TEST(arcanist_is_dead_only_when_fp_hits_zero) {
    using namespace runevault;
    Arcanist a{3, 5, 5};
    RV_CHECK(!a.isDead());
    a.reduceFp(2);
    RV_CHECK(!a.isDead()); // fp == 1
    a.reduceFp(1);
    RV_CHECK(a.isDead()); // fp == 0
}
