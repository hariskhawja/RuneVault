// test_scoring.cpp
// RuneVault — scoring wired through the class score modifier (task 9.2,
// Requirements 13.1/13.2/13.3).
//
// Verifies Model::finalScore() / Model::hasScore():
//   * 13.1: on a win the score equals the total Rune Fragments collected,
//           passed through the Arcanist class scoreModifier.
//   * 13.2: a Sage applies a +50% bonus on a win (raw + raw/2).
//   * 13.3: a run ending by loss/restart/quit (i.e. any non-Won state) produces
//           no score.

#include "test_util.h"

#include "runevault/arcanist.h"
#include "runevault/arcanist_factory.h"
#include "runevault/model.h"

#include <memory>

namespace {

using namespace runevault;

// 13.1 + 13.2: a Sage win applies the +50% bonus (100 -> 100 + 50 = 150).
RV_TEST(scoring_sage_win_applies_fifty_percent_bonus) {
    Model m;
    m.setArcanist(ArcanistFactory::create('s')); // Sage
    m.setRuneFragmentTotal(100);
    m.setRunState(RunState::Won);

    RV_CHECK(m.hasScore());
    RV_CHECK(m.finalScore() == 150);
}

// 13.2: the bonus uses integer raw + raw/2 (odd totals truncate: 101 -> 151).
RV_TEST(scoring_sage_win_bonus_is_raw_plus_half) {
    Model m;
    m.setArcanist(ArcanistFactory::create('s')); // Sage
    m.setRuneFragmentTotal(101);
    m.setRunState(RunState::Won);

    RV_CHECK(m.finalScore() == 101 + 101 / 2); // 151
}

// 13.1: a non-Sage class win scores the raw total (no bonus).
RV_TEST(scoring_nonsage_win_has_no_bonus) {
    Model m;
    m.setArcanist(ArcanistFactory::create('w')); // Warden
    m.setRuneFragmentTotal(100);
    m.setRunState(RunState::Won);

    RV_CHECK(m.hasScore());
    RV_CHECK(m.finalScore() == 100);
}

// 13.3: a run ended by loss produces no score.
RV_TEST(scoring_loss_produces_no_score) {
    Model m;
    m.setArcanist(ArcanistFactory::create('s')); // Sage
    m.setRuneFragmentTotal(100);
    m.setRunState(RunState::Lost);

    RV_CHECK(!m.hasScore());
    RV_CHECK(m.finalScore() == 0);
}

// 13.3: a run still in progress (Playing) produces no score.
RV_TEST(scoring_in_progress_produces_no_score) {
    Model m;
    m.setArcanist(ArcanistFactory::create('s')); // Sage
    m.setRuneFragmentTotal(100);
    // runState() defaults to Playing.

    RV_CHECK(!m.hasScore());
    RV_CHECK(m.finalScore() == 0);
}

// Guard: a won Arcanist with no attached class Strategy falls back to identity
// (the raw total), never dereferencing a null strategy.
RV_TEST(scoring_win_without_strategy_falls_back_to_identity) {
    Model m;
    m.setArcanist(std::make_shared<Arcanist>(20, 5, 3)); // stat-only, no strategy
    m.setRuneFragmentTotal(100);
    m.setRunState(RunState::Won);

    RV_CHECK(m.hasScore());
    RV_CHECK(m.finalScore() == 100);
}

} // namespace
