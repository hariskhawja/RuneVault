// test_fragment_grade.cpp
// RuneVault — Rune Fragment grades and their point values (task 13.1).
//
// Verifies Requirement 9.1's value table: common shard 1, resonant shard 2,
// Lich cache 4, Shard Coffer 6 — both via `fragmentValue(FragmentGrade)` and
// via the `RuneFragment(FragmentGrade)` convenience constructor (which must
// still render as `*` and leave the existing int constructor intact).
//
// Validates: Requirements 9.1

#include "test_util.h"

#include "runevault/fragment_grade.h"
#include "runevault/item.h"

namespace {
using namespace runevault;
} // namespace

RV_TEST(fragment_value_matches_grade_table) {
    RV_CHECK(fragmentValue(FragmentGrade::CommonShard) == 1);
    RV_CHECK(fragmentValue(FragmentGrade::ResonantShard) == 2);
    RV_CHECK(fragmentValue(FragmentGrade::LichCache) == 4);
    RV_CHECK(fragmentValue(FragmentGrade::ShardCoffer) == 6);
}

RV_TEST(rune_fragment_from_grade_takes_grade_value) {
    RV_CHECK(RuneFragment(FragmentGrade::CommonShard).value() == 1);
    RV_CHECK(RuneFragment(FragmentGrade::ResonantShard).value() == 2);
    RV_CHECK(RuneFragment(FragmentGrade::LichCache).value() == 4);
    RV_CHECK(RuneFragment(FragmentGrade::ShardCoffer).value() == 6);
}

RV_TEST(rune_fragment_from_grade_still_renders_as_star) {
    RV_CHECK(RuneFragment(FragmentGrade::ShardCoffer).symbol() ==
             glyphs::kRuneFragment);
    RV_CHECK(RuneFragment(FragmentGrade::CommonShard).symbol() == '*');
}

RV_TEST(rune_fragment_int_constructor_still_works) {
    // The original int constructor must remain usable (spawner/tests rely on it).
    RV_CHECK(RuneFragment(6).value() == 6);
    RV_CHECK(RuneFragment(6).value() ==
             fragmentValue(FragmentGrade::ShardCoffer));
}
