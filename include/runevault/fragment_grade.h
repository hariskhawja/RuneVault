#ifndef RUNEVAULT_FRAGMENT_GRADE_H
#define RUNEVAULT_FRAGMENT_GRADE_H

namespace runevault {

enum class FragmentGrade {
    CommonShard,   // worth 1
    ResonantShard, // worth 2
    LichCache,     // worth 4
    ShardCoffer,   // worth 6
};

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
