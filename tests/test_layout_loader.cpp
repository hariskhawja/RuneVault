// test_layout_loader.cpp
// RuneVault — tests for the LayoutLoader (task 18.2, Requirements 12.1/12.2/12.4).
//
// Assert-based (see tests/test_util.h). The parser primarily exercises the pure
// `loadFromString` overload (no filesystem flakiness); one test covers the
// `loadFromFile` missing-file path. Coverage:
//   * missing file            -> ok==false, non-empty error   (Req 12.4)
//   * well-formed one-grid text -> one grid replicated to 5 levels (Q15 FU-2),
//                                 glyphs → CellTypes, digits recorded + built
//                                 as items (Req 12.1/12.2)
//   * malformed: too many rows / row too wide / bad glyph      (Req 12.4)
//   * digit-mapping coverage: each 0..9 → the intended item     (Req 12.2)

#include "test_util.h"

#include "runevault/cell.h"
#include "runevault/guarded_item.h"
#include "runevault/item.h"
#include "runevault/layout_loader.h"
#include "runevault/level.h"
#include "runevault/scroll_type.h"

#include <string>

using namespace runevault;

namespace {

// A tiny 3-row map grid: a bordered box with a floor interior. `interior` is
// the middle-row interior (between the side walls) — used to inject
// digits/glyphs for a specific test. Per Q15 Follow-up-2 the file is ONE grid
// applied to all five levels, so a single block IS the whole layout.
std::string singleGrid(const std::string &interior) {
    return std::string("|---|\n") + "|" + interior + "|\n" + "|---|";
}

} // namespace

// Requirement 12.4: a missing file yields ok==false with a descriptive error.
RV_TEST(layout_loader_missing_file_reports_error) {
    LoadedLayout loaded =
        LayoutLoader::loadFromFile("does_not_exist_xyz.layout");
    RV_CHECK(loaded.ok == false);
    RV_CHECK(!loaded.error.empty());
}

// Q15 Follow-up-2: one grid is REPLICATED across all five levels.
RV_TEST(layout_loader_replicates_one_grid_across_five_levels) {
    LoadedLayout loaded = LayoutLoader::loadFromString(singleGrid("..."));
    RV_CHECK(loaded.ok == true);
    RV_CHECK(loaded.error.empty());
    // All five levels share the SAME parsed grid (interior "..." = 3 floor
    // tiles on row 1, cols 1..3).
    for (int i = 0; i < kLayoutLevelCount; ++i) {
        RV_CHECK(loaded.boards[i].at(Position{1, 1}).type == CellType::Floor);
        RV_CHECK(loaded.boards[i].at(Position{1, 3}).type == CellType::Floor);
    }
}

// Glyphs map to the right CellTypes (Requirement 12.1 / cell.h table).
RV_TEST(layout_loader_maps_glyphs_to_cell_types) {
    // A grid that exercises walls, archway, corridor, floor.
    std::string text =
        std::string("|-+-|\n") + "|.#.|\n" + "|---|";

    LoadedLayout loaded = LayoutLoader::loadFromString(text);
    RV_CHECK(loaded.ok == true);
    const Board &b = loaded.boards[0];
    RV_CHECK(b.at(Position{0, 0}).type == CellType::Wall);    // '|'
    RV_CHECK(b.at(Position{0, 1}).type == CellType::Wall);    // '-'
    RV_CHECK(b.at(Position{0, 2}).type == CellType::Archway); // '+'
    RV_CHECK(b.at(Position{1, 0}).type == CellType::Wall);    // '|'
    RV_CHECK(b.at(Position{1, 1}).type == CellType::Floor);   // '.'
    RV_CHECK(b.at(Position{1, 2}).type == CellType::Corridor);// '#'
    RV_CHECK(b.at(Position{1, 3}).type == CellType::Floor);   // '.'
}

// Digit cells become Floor and are recorded as the correct items when the level
// is built (Requirement 12.2): '0' -> Attune Focus scroll, '6' -> common shard,
// '9' -> Shard Coffer.
RV_TEST(layout_loader_builds_items_from_digits) {
    LoadedLayout loaded = LayoutLoader::loadFromString(singleGrid("069"));
    RV_CHECK(loaded.ok == true);

    // Underlying cells are Floor (digits sit on floor tiles).
    for (int col = 1; col <= 3; ++col) {
        RV_CHECK(loaded.boards[0].at(Position{1, col}).type == CellType::Floor);
    }
    // Three placements recorded on level 0.
    RV_CHECK(loaded.items[0].size() == 3);

    std::unique_ptr<Level> level = LayoutLoader::buildLevel(loaded, 0);
    RV_CHECK(level != nullptr);
    if (!level) {
        return;
    }
    RV_CHECK(level->items().size() == 3);

    // '0' at (1,1) -> Attune Focus scroll.
    Item *i0 = level->itemAt(Position{1, 1});
    RV_CHECK(i0 != nullptr);
    Scroll *scroll = dynamic_cast<Scroll *>(i0);
    RV_CHECK(scroll != nullptr);
    if (scroll) {
        RV_CHECK(scroll->scrollType() == ScrollType::AttuneFocus);
    }

    // '6' at (1,2) -> common shard fragment (value 1).
    Item *i6 = level->itemAt(Position{1, 2});
    RV_CHECK(i6 != nullptr);
    RuneFragment *frag = dynamic_cast<RuneFragment *>(i6);
    RV_CHECK(frag != nullptr);
    if (frag) {
        RV_CHECK(frag->value() == 1);
    }

    // '9' at (1,3) -> Shard Coffer (guarded).
    Item *i9 = level->itemAt(Position{1, 3});
    RV_CHECK(i9 != nullptr);
    RV_CHECK(dynamic_cast<ShardCoffer *>(i9) != nullptr);
}

// Digit-mapping coverage: every digit 0..9 maps to the intended item
// (Requirement 12.2). Uses the exposed makeItemForDigit helper.
RV_TEST(layout_loader_digit_mapping_covers_all_ten) {
    // Scrolls 0..5.
    const ScrollType expected[] = {
        ScrollType::AttuneFocus, ScrollType::SurgePower, ScrollType::FortifyWard,
        ScrollType::DrainFocus,  ScrollType::SapPower,   ScrollType::ErodeWard};
    for (int d = 0; d <= 5; ++d) {
        std::unique_ptr<Item> item =
            LayoutLoader::makeItemForDigit(static_cast<char>('0' + d));
        Scroll *scroll = dynamic_cast<Scroll *>(item.get());
        RV_CHECK(scroll != nullptr);
        if (scroll) {
            RV_CHECK(scroll->scrollType() == expected[d]);
        }
    }

    // Fragments 6/7/8 -> values 1/2/4.
    auto fragValue = [](char d) -> int {
        std::unique_ptr<Item> item = LayoutLoader::makeItemForDigit(d);
        RuneFragment *frag = dynamic_cast<RuneFragment *>(item.get());
        return frag ? frag->value() : -1;
    };
    RV_CHECK(fragValue('6') == 1); // common shard
    RV_CHECK(fragValue('7') == 2); // resonant shard
    RV_CHECK(fragValue('8') == 4); // Lich cache

    // 9 -> Shard Coffer.
    std::unique_ptr<Item> coffer = LayoutLoader::makeItemForDigit('9');
    RV_CHECK(dynamic_cast<ShardCoffer *>(coffer.get()) != nullptr);
}

// Requirement 12.4: a grid taller than 25 rows is malformed.
RV_TEST(layout_loader_too_many_rows_is_malformed) {
    // 26 rows of floor-in-walls exceeds kPlayableRows (25).
    std::string text;
    for (int i = 0; i < 26; ++i) {
        if (i) {
            text += "\n";
        }
        text += "|.|";
    }
    LoadedLayout loaded = LayoutLoader::loadFromString(text);
    RV_CHECK(loaded.ok == false);
    RV_CHECK(!loaded.error.empty());
}

// Requirement 12.4: a row wider than 79 columns is malformed.
RV_TEST(layout_loader_row_too_wide_is_malformed) {
    std::string wideRow(80, '.'); // 80 > kPlayableCols (79)
    LoadedLayout loaded = LayoutLoader::loadFromString(singleGrid(wideRow));
    RV_CHECK(loaded.ok == false);
    RV_CHECK(!loaded.error.empty());
}

// Requirement 12.4: an unrecognized glyph is malformed (strict).
RV_TEST(layout_loader_unknown_glyph_is_malformed) {
    LoadedLayout loaded = LayoutLoader::loadFromString(singleGrid(".X."));
    RV_CHECK(loaded.ok == false);
    RV_CHECK(!loaded.error.empty());
}

// buildLevel rejects out-of-range indices and failed layouts.
RV_TEST(layout_loader_build_level_guards_inputs) {
    LoadedLayout good = LayoutLoader::loadFromString(singleGrid("..."));
    RV_CHECK(good.ok == true);
    RV_CHECK(LayoutLoader::buildLevel(good, -1) == nullptr);
    RV_CHECK(LayoutLoader::buildLevel(good, 5) == nullptr);

    LoadedLayout bad = LayoutLoader::loadFromString("garbage");
    RV_CHECK(bad.ok == false);
    RV_CHECK(LayoutLoader::buildLevel(bad, 0) == nullptr);
}
