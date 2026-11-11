#include "shuffle.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace rng {

namespace {

// Mersenne Twister — seeded in seed() below.
std::mt19937 engine;

bool seeded = false;

} // anonymous namespace

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

int range(int lo, int hi) {
    if (lo > hi) {
        throw std::invalid_argument(
            "rng::range — lo must be <= hi");
    }
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(engine);
}

double real(double lo, double hi) {
    if (lo >= hi) {
        throw std::invalid_argument(
            "rng::real — lo must be < hi");
    }
    std::uniform_real_distribution<double> dist(lo, hi);
    return dist(engine);
}

bool chance(double p) {
    if (p < 0.0 || p > 1.0) {
        throw std::invalid_argument(
            "rng::chance — probability must be in [0.0, 1.0]");
    }
    std::bernoulli_distribution dist(p);
    return dist(engine);
}

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
