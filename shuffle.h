// shuffle.h
// RuneVault — pseudo-random number generation utilities.
//
// Seed once at program startup:
//
//   if (user supplied seed)  rng::seed(user_seed);
//   else                     rng::seed_from_time();
//
// Then use the free functions anywhere randomness is needed.

#ifndef SHUFFLE_H
#define SHUFFLE_H

#include <algorithm>    // std::shuffle
#include <cstddef>      // std::size_t
#include <iterator>     // std::iterator_traits
#include <random>       // std::mt19937, distributions
#include <vector>

namespace rng {

// ── Seeding ────────────────────────────────────────────────────────────────

// Seed with an explicit value.  Games started with the same seed produce
// identical sequences — required for -testing mode and save/load.
void seed(unsigned long s);

// Seed from the current wall-clock time (non-reproducible).
// Call this when no seed is given on the command line.
void seed_from_time();

// Direct access to the engine for use with standard library algorithms
// (e.g. std::shuffle).  Prefer the helpers below when possible.
std::mt19937 &get_engine();

// ── Random values ──────────────────────────────────────────────────────────

// Uniform integer in [lo, hi] inclusive.
// Throws std::invalid_argument if lo > hi.
int range(int lo, int hi);

// Uniform double in [lo, hi).
// Throws std::invalid_argument if lo >= hi.
double real(double lo, double hi);

// Returns true with probability p.
// Throws std::invalid_argument if p is outside [0.0, 1.0].
bool chance(double p);

// Weighted random index.  Returns an index into `weights`, selected with
// probability proportional to weights[i].  All weights must be >= 0 and
// their sum must be > 0.
// Throws std::invalid_argument if weights is empty.
std::size_t weighted(const std::vector<double> &weights);

// ── Container shuffle ──────────────────────────────────────────────────────

// Shuffle the elements in [first, last) using the module's engine.
// Works with any RandomAccessIterator (std::vector, std::array, raw arrays).
//
// Example:
//   std::vector<int> v = {1, 2, 3, 4, 5};
//   rng::shuffle(v.begin(), v.end());
template <typename RandomIt>
void shuffle(RandomIt first, RandomIt last) {
    std::shuffle(first, last, get_engine());
}

// ── Convenience: pick one element at random ────────────────────────────────

// Returns a reference to a uniformly random element of the container.
// Container must not be empty and must support operator[] and .size().
template <typename Container>
auto pick(Container &c) -> decltype(c[0]) {
    return c[static_cast<typename Container::size_type>(
        range(0, static_cast<int>(c.size()) - 1))];
}

template <typename Container>
auto pick(const Container &c) -> decltype(c[0]) {
    return c[static_cast<typename Container::size_type>(
        range(0, static_cast<int>(c.size()) - 1))];
}

} // namespace rng

#endif // SHUFFLE_H
