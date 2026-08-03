// test_model.cpp
// RuneVault — tests for the Model skeleton (task 5.4).
//
// The Model is a state container: these tests exercise the default state, the
// Arcanist/Level ownership hand-off, the Rune Fragment accumulation, the run
// flag round-trips, and the identified-scroll set.

#include "test_util.h"

#include "runevault/model.h"

#include <memory>
#include <utility>

RV_TEST(model_default_state_is_empty_run) {
    using namespace runevault;
    Model m;
    RV_CHECK(m.arcanist() == nullptr);
    RV_CHECK(m.level() == nullptr);
    RV_CHECK(m.levelNumber() == 1);
    RV_CHECK(m.runeFragmentTotal() == 0);
    RV_CHECK(m.lichAggroTriggered() == false);
    RV_CHECK(m.carryingCipherGem() == false);
    RV_CHECK(m.carryingAegisCloak() == false);
    RV_CHECK(m.aegisSpawnedThisRun() == false);
    RV_CHECK(m.aegisLevel() == 0);
    RV_CHECK(m.isScrollIdentified(0) == false);
}

RV_TEST(model_holds_arcanist_and_exposes_its_stats) {
    using namespace runevault;
    Model m;
    auto arc = std::make_shared<Arcanist>(/*fp*/ 50, /*pwr*/ 25, /*wr*/ 15);
    m.setArcanist(arc);
    RV_CHECK(m.arcanist() != nullptr);
    RV_CHECK(m.arcanist()->fp() == 50);
    RV_CHECK(m.arcanist()->maxFp() == 50);
    RV_CHECK(m.arcanist()->pwr() == 25);
    RV_CHECK(m.arcanist()->wr() == 15);
    // Shared ownership: the Model and the local pointer refer to the same object.
    m.arcanist()->reduceFp(10);
    RV_CHECK(arc->fp() == 40);
}

RV_TEST(model_takes_ownership_of_a_level_by_move) {
    using namespace runevault;
    Model m;
    auto level = std::unique_ptr<Level>(new Level(makeDefaultBoard()));
    level->setStairway(Position{4, 5});
    m.setLevel(std::move(level));
    RV_CHECK(level == nullptr); // ownership moved into the Model
    RV_CHECK(m.level() != nullptr);
    RV_CHECK(m.level()->stairway().row == 4);
    RV_CHECK(m.level()->stairway().col == 5);
    // The moved-in board is the default layout (chamber 1 interior is floor).
    RV_CHECK(m.level()->board().at(Position{1, 1}).type == CellType::Floor);
}

RV_TEST(model_level_number_round_trips) {
    using namespace runevault;
    Model m;
    m.setLevelNumber(3);
    RV_CHECK(m.levelNumber() == 3);
}

RV_TEST(model_add_rune_fragments_accumulates) {
    using namespace runevault;
    Model m;
    m.addRuneFragments(5);
    m.addRuneFragments(7);
    RV_CHECK(m.runeFragmentTotal() == 12);
    // Negative amounts are ignored (never decreases the total).
    m.addRuneFragments(-4);
    RV_CHECK(m.runeFragmentTotal() == 12);
    // Direct setter still works.
    m.setRuneFragmentTotal(100);
    RV_CHECK(m.runeFragmentTotal() == 100);
}

RV_TEST(model_run_flags_round_trip) {
    using namespace runevault;
    Model m;
    m.setLichAggroTriggered(true);
    m.setCarryingCipherGem(true);
    m.setCarryingAegisCloak(true);
    m.setAegisSpawnedThisRun(true);
    m.setAegisLevel(2);
    RV_CHECK(m.lichAggroTriggered() == true);
    RV_CHECK(m.carryingCipherGem() == true);
    RV_CHECK(m.carryingAegisCloak() == true);
    RV_CHECK(m.aegisSpawnedThisRun() == true);
    RV_CHECK(m.aegisLevel() == 2);
    // Flags can toggle back.
    m.setCarryingCipherGem(false);
    RV_CHECK(m.carryingCipherGem() == false);
}

RV_TEST(model_identified_scrolls_track_by_id) {
    using namespace runevault;
    Model m;
    RV_CHECK(m.isScrollIdentified(3) == false);
    m.markScrollIdentified(3);
    RV_CHECK(m.isScrollIdentified(3) == true);
    RV_CHECK(m.isScrollIdentified(4) == false);
    // Marking is idempotent.
    m.markScrollIdentified(3);
    RV_CHECK(m.isScrollIdentified(3) == true);
    m.markScrollIdentified(4);
    RV_CHECK(m.isScrollIdentified(4) == true);
}
