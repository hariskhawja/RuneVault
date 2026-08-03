// shuffle.cc
// RuneVault — pseudo-random number generation utilities.
//
// The generator is seeded once at program startup (or with a user-supplied
// seed via the command line).  Every module that needs randomness should call
// the free functions below rather than calling rand() / srand() directly.
// This guarantees a single, reproducible sequence for a given seed, which is
// required for the -testing flag and for save/load reproducibility.
//
// Usage:
//   #include "shuffle.h"
//   rng::seed(42);                        // seed once at startup
//   int d = rng::range(1, 6);             // roll a die  [1, 6]
//   bool hit = rng::chance(0.5);          // 50 % coin flip
//   rng::shuffle(vec.begin(), vec.end()); // shuffle a container

#include "shuffle.h"

#include <algorithm>   // std::shuffle
#include <chrono>      // std::chrono::steady_clock
#include <stdexcept>   // std::invalid_argument

namespace rng {

// ── Internal state ─────────────────────────────────────────────────────────

namespace {

// Mersenne Twister — seeded in seed() below.
std::mt19937 engine;

// Track whether seed() has been called so we can warn in debug builds.
bool seeded = false;

} // anonymous namespace

// ── Public API ─────────────────────────────────────────────────────────────

void seed(unsigned long s) {
    engine.seed(static_cast<std::mt19937::result_type>(s));
    seeded = true;
}

void seed_from_time() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    seed(static_cast<unsigned long>(now));
}

std::mt19937 &get_engine() {
    return engine;
}

// ── Integer range [lo, hi] inclusive ───────────────────────────────────────

int range(int lo, int hi) {
    if (lo > hi) {
        throw std::invalid_argument(
            "rng::range — lo must be <= hi");
    }
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(engine);
}

// ── Floating-point range [lo, hi) ──────────────────────────────────────────

double real(double lo, double hi) {
    if (lo >= hi) {
        throw std::invalid_argument(
            "rng::real — lo must be < hi");
    }
    std::uniform_real_distribution<double> dist(lo, hi);
    return dist(engine);
}

// ── Boolean with probability p of returning true  [0.0, 1.0] ──────────────

bool chance(double p) {
    if (p < 0.0 || p > 1.0) {
        throw std::invalid_argument(
            "rng::chance — probability must be in [0.0, 1.0]");
    }
    std::bernoulli_distribution dist(p);
    return dist(engine);
}

// ── Weighted selection ─────────────────────────────────────────────────────
//
// Returns an index into the weights vector, sampled proportionally.
// Example:
//   rng::weighted({1, 2, 3}) → 0 with prob 1/6, 1 with prob 2/6, 2 with 3/6

std::size_t weighted(const std::vector<double> &weights) {
    if (weights.empty()) {
        throw std::invalid_argument(
            "rng::weighted — weights vector must not be empty");
    }
    std::discrete_distribution<std::size_t> dist(
        weights.begin(), weights.end());
    return dist(engine);
}

} // namespace rng
