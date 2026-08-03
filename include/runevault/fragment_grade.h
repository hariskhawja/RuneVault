// fragment_grade.h
// RuneVault — Rune Fragment grades and their point values (task 13.1).
//
// Requirement 9.1 fixes the four Rune Fragment grades and the number of Rune
// Fragments each is worth:
//
//     common shard   = 1
//     resonant shard = 2
//     Lich cache     = 4
//     Shard Coffer   = 6
//
// These are captured here as a named enum plus a `fragmentValue` lookup so the
// rest of the code (drops, the guarded Shard Coffer in task 13.2, and the
// spawner in task 15.3) references grades by name rather than magic numbers.
// The Lich cache value (4) matches Lich::runeDrop() (task 10.1).
//
// Header-only: the enum and the tiny `fragmentValue` function are both defined
// inline here so any translation unit can include this without a .cpp.

#ifndef RUNEVAULT_FRAGMENT_GRADE_H
#define RUNEVAULT_FRAGMENT_GRADE_H

namespace runevault {

// The four Rune Fragment grades (Requirement 9.1). The Shard Coffer is also a
// GUARDED item (its State machine is task 13.2); this enum only names the grade
// so its value (6) can be looked up alongside the others.
enum class FragmentGrade {
    CommonShard,   // worth 1
    ResonantShard, // worth 2
    LichCache,     // worth 4
    ShardCoffer,   // worth 6
};

// The number of Rune Fragments a fragment of the given grade is worth
// (Requirement 9.1: 1 / 2 / 4 / 6).
inline int fragmentValue(FragmentGrade grade) {
    switch (grade) {
    case FragmentGrade::CommonShard:
        return 1;
    case FragmentGrade::ResonantShard:
        return 2;
    case FragmentGrade::LichCache:
        return 4;
    case FragmentGrade::ShardCoffer:
        return 6;
    }
    return 0; // unreachable; keeps -Wreturn-type quiet on all compilers
}

} // namespace runevault

#endif // RUNEVAULT_FRAGMENT_GRADE_H
