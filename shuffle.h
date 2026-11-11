#ifndef SHUFFLE_H
#define SHUFFLE_H

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <random>
#include <vector>

// Seed once at startup. All helpers share the same engine.
namespace rng {

// Equal seeds reproduce the same random sequence.
void seed(unsigned long s);

// Seed from the current wall-clock time (non-reproducible).
// Call this when no seed is given on the command line.
void seed_from_time();

// Direct access to the engine for use with standard library algorithms
// (e.g. std::shuffle).  Prefer the helpers below when possible.
std::mt19937 &get_engine();

// Uniform integer in [lo, hi] inclusive.
// Throws std::invalid_argument if lo > hi.
int range(int lo, int hi);

// Uniform double in [lo, hi).
// Throws std::invalid_argument if lo >= hi.
double real(double lo, double hi);

// Returns true with probability p.
// Throws std::invalid_argument if p is outside [0.0, 1.0].
bool chance(double p);

// Weights must be nonnegative with a positive sum; empty input throws.
std::size_t weighted(const std::vector<double> &weights);

template <typename RandomIt>
void shuffle(RandomIt first, RandomIt last) {
    std::shuffle(first, last, get_engine());
}

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
