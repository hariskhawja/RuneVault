# Implementation Plan: RuneVault

## Overview

This plan converts the RuneVault design into incremental C++ (C++14/17) coding steps that follow the design's layered build order: displayable board and view first, then generalized actors and interaction, then specialized types (Strategy/Visitor/Decorator/State patterns), and finally the random spawning pipeline. Each step builds on the previous ones and ends by wiring components together so there is no orphaned code.

Property-based tests use `rapidcheck` and are marked optional (`*`). Each property test references a specific correctness property from `design.md` and the requirement acceptance criteria it validates. Testing tasks are always sub-tasks under an implementation parent, never stand-alone.

## Tasks

- [x] 1. Set up project structure and core geometry
  - [x] 1.1 Set up project structure, build system, and test framework
    - Create source/header directory structure and a build file (Makefile/CMake) that compiles `shuffle.cpp` and the RuneVault sources
    - Wire in `rapidcheck` as a test-only dependency and add a runnable test target
    - _Requirements: 12.3_

  - [x] 1.2 Implement Direction, Position, and Geometry helpers
    - Define `Direction` enum, `Position` struct, and the `Geometry` utility (`step`, `neighbours`, `isAdjacent`, `parse`) centralizing the 8-direction / 1-block-radius math
    - _Requirements: 5.1_

  - [ ]* 1.3 Write unit tests for geometry helpers
    - Test `step`, `neighbours`, `isAdjacent`, `parse` for all 8 directions and boundary positions
    - _Requirements: 5.1_

- [x] 2. Implement board, cells, and fixed level structure
  - [x] 2.1 Implement Cell and CellType
    - Define `CellType` (wall, archway, corridor, floor, stairway), the `Cell` struct with chamber membership, and walkability/host queries per the geometry table
    - _Requirements: 2.4, 3.5_

  - [x] 2.2 Implement Board grid with chamber membership and adjacency
    - Implement the `Board` (79×25 playable grid), chamber enumeration, floor-tile enumeration per chamber, and adjacency/geometry queries
    - _Requirements: 2.1, 2.2, 3.1, 3.2, 3.7, 3.8_

  - [x] 2.3 Implement the default fixed layout and Level scaffolding
    - Encode the hardcoded tile-for-tile default layout (5 chambers, corridors, archways, connectivity 1-2, 1-3, 2-4, 3-4, 4-5) and a `Level` that owns the `Board` plus stairway location and revealed state
    - _Requirements: 3.3, 3.4, 3.6_

  - [ ]* 2.4 Write unit tests for board geometry and connectivity
    - Verify chamber sizes, archway placement, full reachability between all chambers, and that the layout fits the playable region
    - _Requirements: 3.2, 3.4, 3.5, 3.8_

- [x] 3. Implement the View renderer
  - [x] 3.1 Implement board frame renderer with DD-7 symbol map
    - Render the full 79×30 frame; map every cell/actor/item to its glyph (`@`, `|`, `-`, `+`, `#`, `.`, `?`, `*`, `C`, `$`, `G`, and Spectre glyphs); render stairway as `.` when hidden and `\` when Cipher Gem carried; apply optional ANSI colour codes
    - _Requirements: 2.3, 2.4, 2.5, 2.6, 2.7, 2.9_

  - [x] 3.2 Implement the status-area renderer
    - Render the bottom 5 status rows: class, FP, Pwr, WR, total Rune Fragments, and identified-scroll hints/messages
    - _Requirements: 2.2, 2.8_

  - [ ]* 3.3 Write unit tests for rendering
    - Assert stairway hidden vs revealed glyph, that scrolls always render as `?`, and that the status area shows the expected stats
    - _Requirements: 2.6, 2.7, 8.9_

- [x] 4. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Implement generalized Arcanist, Spectre, Item, and Model skeleton
  - [x] 5.1 Implement the Arcanist class
    - Implement stats (FP, maxFp, Pwr, WR), `applyDamage`/`reduceFp`, `healUpTo`, `loseUpTo`, and `addPwr`/`addWr` on a non-abstract base Arcanist
    - _Requirements: 1.8_

  - [x] 5.2 Implement the Item base and a generic RuneFragment
    - Define the abstract `Item` (symbol, position, `onWalkOver`) and a generic `RuneFragment` carrying a value
    - _Requirements: 2.4_

  - [x] 5.3 Implement the Spectre base and a generic Spectre
    - Define the abstract `Spectre` (stats, `takeDamage`, `isDead`, `spawnChamber`, `position`/`moveTo`, `isMobile`, `runeDrop`, `isDefaultHostile`) with a generic concrete placeholder
    - _Requirements: 2.5_

  - [x] 5.4 Implement the Model skeleton
    - Aggregate run state: current `Level`, Arcanist pointer, `runeFragmentTotal`, and run flags (`lichAggroTriggered`, `carryingCipherGem`, `carryingAegisCloak`, `identifiedScrolls`, `aegisSpawnedThisRun`, `aegisLevel`)
    - _Requirements: 1.8_

- [x] 6. Implement Arcanist movement and interaction
  - [x] 6.1 Implement 8-direction movement with walkability checks
    - Move the Arcanist one cell for `no/so/ea/we/ne/nw/se/sw`; allow floor/archway/corridor; reject walls and out-of-bounds without consuming the turn
    - _Requirements: 5.1, 5.2, 5.3_

  - [ ]* 6.2 Write property test for directional movement
    - **Property 13: Directional movement onto walkable cells**
    - **Validates: Requirements 5.1, 5.2**

  - [x] 6.3 Implement fragment walk-over collection
    - On entering a fragment tile, add its value to the RF total and remove it from the board
    - _Requirements: 5.4_

  - [ ]* 6.4 Write property test for fragment walk-over
    - **Property 14: Fragment walk-over collection**
    - **Validates: Requirements 5.4**

  - [x] 6.5 Implement guarded-item walk-over and stairway reveal
    - Allow the Arcanist to occupy guarded-item tiles regardless of guard state (DD-4); reveal the stairway when the Cipher Gem is carried
    - _Requirements: 5.5, 2.6, 2.7_

- [x] 7. Implement combat resolution
  - [x] 7.1 Implement the damage formula
    - Implement `Damage = ceil( (100 / (100 + WR_defender)) * Pwr_attacker )` using effective Pwr/WR
    - _Requirements: 6.2, 6.6_

  - [ ]* 7.2 Write property test for damage formula
    - **Property 1: Damage formula non-negativity**
    - **Validates: Requirements 6.2, 6.6**

  - [x] 7.3 Implement Warden/Aegis damage-taken ordering
    - Apply strategy `damageTakenModifier` (Warden −1), then Aegis halve-round-up, then `max(1, …)` in that strict order
    - _Requirements: 6.7, 6.8, 6.9, 9.8_

  - [ ]* 7.4 Write property test for Warden + Aegis ordering
    - **Property 2: Warden + Aegis ordering**
    - **Validates: Requirements 6.7, 6.8, 6.9, 9.8**

  - [x] 7.5 Implement Arcanist attack and the no-op case
    - `a <dir>` always hits the adjacent Spectre with initiative before Spectres act; if no adjacent Spectre, treat as no-op and do not consume the turn
    - _Requirements: 6.1, 6.2, 6.3_

  - [x] 7.6 Implement Spectre attack and loss-on-zero-FP
    - Apply 50% miss via `rng`, compute base damage, route through the damage-taken ordering, reduce FP, and end the run as a loss when FP reaches 0
    - _Requirements: 6.4, 6.5, 6.6, 10.5_

  - [ ]* 7.7 Write property test for loss on zero FP
    - **Property 21: Loss on zero FP**
    - **Validates: Requirements 10.5**

  - [x] 7.8 Implement Spectre defeat drops
    - Add 1 RF for a non-Anchor non-Lich defeat, 4 for a Lich cache, none for a Vault Anchor; drop the Cipher Gem on the carrier's tile when the carrier dies
    - _Requirements: 6.10, 6.11, 6.12_

  - [ ]* 7.9 Write property test for Spectre defeat drops
    - **Property 15: Spectre defeat drops**
    - **Validates: Requirements 6.10, 6.11**

- [x] 8. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 9. Implement Arcanist classes with Factory Method + Strategy
  - [x] 9.1 Implement the class strategy interface, concrete strategies, and factory
    - Implement `ArcanistClassStrategy` (`scoreModifier`, `scrollEffectModifier`, `damageTakenModifier`, `className`, `classKey`), the four concrete strategies plus `ArchmageStrategy`, and `ArcanistFactory::create` with the base-stats table
    - _Requirements: 1.2, 1.3, 1.4, 1.5, 1.8_

  - [x] 9.2 Wire scoring through the score modifier
    - Compute the final score as `scoreModifier(runeFragmentTotal)` on a win (Sage ×1.5); produce no score on loss/restart/quit
    - _Requirements: 13.1, 13.2, 13.3_

  - [ ]* 9.3 Write property test for scoring
    - **Property 12: Scoring**
    - **Validates: Requirements 13.1, 13.2, 13.3**

  - [ ]* 9.4 Write unit tests for class strategies
    - Test Sage scoring, Hexblade doubling, seeded Voidwalker flip, and Warden −1 (min 1)
    - _Requirements: 1.2, 1.3, 1.4, 1.5_

- [x] 10. Implement Spectre subclasses, Visitor, movement, and hostility
  - [x] 10.1 Implement concrete Spectre subclasses and SpectreFactory
    - Implement `Wraith`, `Banshee`, `Revenant`, `Shade`, `Lich`, `VaultAnchor`, `SpecterLord` with the stats/drops table; implement `SpectreFactory::create` and `createRandom` (weights `{4,3,5,2,2,2}`)
    - _Requirements: 2.5, 4.7_

  - [x] 10.2 Implement the CombatVisitor
    - Implement double-dispatch `accept`/`visit` for each Spectre type, trip `lichAggroTriggered` on Lich attack, and unlock guarded items on Vault Anchor death
    - _Requirements: 6.12, 7.7_

  - [x] 10.3 Implement Spectre movement with chamber containment
    - Process Spectres row-by-row, left-to-right, once each; move a non-attacking mobile Spectre to a random unoccupied floor tile in its spawn chamber within its 1-block radius; keep Vault Anchors stationary; never leave the spawn chamber
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.10_

  - [ ]* 10.4 Write property test for chamber containment
    - **Property 4: Chamber containment**
    - **Validates: Requirements 7.2, 7.3, 7.4, 7.10**

  - [x] 10.5 Implement hostility rules
    - Vault Anchor hostile only while the Arcanist is adjacent to its guarded Coffer; Lich hostility driven by the global flag; hostile-in-radius Spectres attack instead of moving
    - _Requirements: 6.4, 7.5, 7.6, 7.7, 7.8, 7.9_

  - [ ]* 10.6 Write property test for Lich aggro monotonicity
    - **Property 7: Lich aggro monotonicity**
    - **Validates: Requirements 7.7, 7.8, 7.9, 11.2**

- [x] 11. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 12. Implement Arcane Scrolls with the Decorator pattern
  - [x] 12.1 Implement ArcanistComponent and the ScrollDecorator chain
    - Implement `ArcanistComponent` (`effPwr`, `effWr`, `base`) adapted onto the base Arcanist, plus `SurgePower`/`FortifyWard`/`SapPower`/`ErodeWard` decorators with floor-at-0 in `effPwr`/`effWr`
    - _Requirements: 8.5, 8.6, 8.7, 8.8_

  - [x] 12.2 Implement scroll types and useScroll
    - Implement `u <dir>` (1-block-radius search, no-op if none), the 6 scroll types, permanent Attune/Drain Focus (cap at max / floor at 0), and temporary effects via decorator wrapping
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.9_

  - [ ]* 12.3 Write property test for permanent scroll bounds
    - **Property 16: Permanent scroll bounds**
    - **Validates: Requirements 8.3, 8.4**

  - [x] 12.4 Implement Hexblade doubling and Voidwalker flip
    - Route scroll magnitude/sign through `scrollEffectModifier`: Hexblade doubles magnitude; Voidwalker has a 50% chance to flip a negative effect to its positive counterpart
    - _Requirements: 8.11, 8.12_

  - [ ]* 12.5 Write property test for Hexblade scroll magnitude
    - **Property 17: Hexblade scroll magnitude**
    - **Validates: Requirements 8.11**

  - [x] 12.6 Implement scroll identification tracking
    - Record a scroll type as remembered on first use for the rest of the run while keeping the board symbol always `?` (DD-3)
    - _Requirements: 8.9, 8.10_

  - [ ]* 12.7 Write property test for scroll identification integrity
    - **Property 8: Scroll identification integrity**
    - **Validates: Requirements 8.9, 8.10**

- [x] 13. Implement fragment grades and the guarded-item State pattern
  - [x] 13.1 Implement Rune Fragment grades and values
    - Value common shard 1, resonant shard 2, Lich cache 4, Shard Coffer 6
    - _Requirements: 9.1_

  - [x] 13.2 Implement GuardedItem, State machine, and concrete items
    - Implement `GuardState` (`LockedState`/`UnlockedState`), `GuardedItem::tryCollect`/`onAnchorDeath`, and `ShardCoffer`, `AegisCloak`, `CipherGem`; walking onto tiles allowed, collection gated by state
    - _Requirements: 9.3, 9.4, 9.5, 9.7_

  - [x] 13.3 Wire Vault Anchor death to unlock guarded items
    - On Anchor death (via `CombatVisitor::visit(VaultAnchor&)`) transition its guarded item(s) to Unlocked; Cipher Gem walk-over lifts the Ward and reveals the stairway; Aegis carried halves damage
    - _Requirements: 9.2, 9.3, 9.8, 10.1, 10.2_

  - [ ]* 13.4 Write property test for ward gating
    - **Property 10: Ward gating**
    - **Validates: Requirements 2.6, 2.7, 5.5, 6.12, 7.5, 7.6, 9.2, 9.3, 9.4, 9.5, 9.7, 10.1, 10.2**

- [x] 14. Checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 15. Implement the random spawning pipeline
  - [x] 15.1 Implement the Spawner pipeline order, occupancy, and guarded-placement helper
    - Execute the spec-ordered pipeline (Arcanist, stairway, scrolls, fragments, spectres) on floor tiles only, one object per cell, with a shared `placeGuarded` helper putting a Vault Anchor within a 1-block radius of its guarded item
    - _Requirements: 4.1, 4.6, 4.8, 4.9, 4.10_

  - [ ]* 15.2 Write property test for occupancy invariant
    - **Property 3: Occupancy invariant**
    - **Validates: Requirements 4.6, 4.9, 4.10**

  - [x] 15.3 Implement spawn counts and distributions
    - Place exactly 10 scrolls (each 1/6), 1 Shard Coffer + 9 fragment piles (common 5/7, resonant 2/7), and 20 Spectres using the distribution weights
    - _Requirements: 4.4, 4.5, 4.7_

  - [ ]* 15.4 Write property test for spawn counts per level
    - **Property 5: Spawn counts per level**
    - **Validates: Requirements 4.4, 4.5, 4.7**

  - [x] 15.5 Implement Arcanist/stairway chamber separation
    - Place the Arcanist and stairway on floor tiles in different chambers, choosing chamber and tile uniformly at random
    - _Requirements: 4.2, 4.3_

  - [ ]* 15.6 Write property test for Arcanist spawn separation
    - **Property 6: Arcanist spawn separation**
    - **Validates: Requirements 4.2, 4.3**

  - [x] 15.7 Implement Cipher Gem carrier selection
    - After spectres are placed, choose exactly one non-Vault-Anchor Spectre uniformly at random to carry the Cipher Gem
    - _Requirements: 4.12_

  - [ ]* 15.8 Write property test for Cipher Gem carrier selection
    - **Property 19: Cipher Gem carrier selection**
    - **Validates: Requirements 4.12**

  - [x] 15.9 Implement Aegis Cloak single guarded placement
    - Select one level in 2–4 at run start and place the Aegis Cloak once using the guarded-placement helper
    - _Requirements: 4.13, 9.6_

  - [ ]* 15.10 Write property test for Aegis Cloak single guarded placement
    - **Property 20: Aegis Cloak single guarded placement**
    - **Validates: Requirements 4.13, 9.6, 10.8**

  - [ ]* 15.11 Write property test for generation determinism
    - **Property 18: Generation determinism**
    - **Validates: Requirements 4.11, 12.3**

- [x] 16. Implement level transition and run lifecycle
  - [x] 16.1 Implement descent, decorator teardown, and carry-state reset
    - On descent with the Cipher Gem, tear down the scroll decorator chain (reset `activeArcanist_` to base), reset carried Cipher Gem state, retain the Aegis Cloak, preserve permanent FP, and build the next level
    - _Requirements: 10.3, 10.6, 10.7, 10.8_

  - [ ]* 16.2 Write property test for decorator teardown
    - **Property 9: Decorator teardown**
    - **Validates: Requirements 8.5, 8.6, 8.7, 8.8, 10.6, 10.7**

  - [x] 16.3 Implement win and lose conditions
    - Block descent without the Cipher Gem (stairway walk-over has no effect); win on level-5 descent; loss when FP reaches 0
    - _Requirements: 10.1, 10.2, 10.4, 10.5_

- [x] 17. Implement the command interpreter (Controller)
  - [x] 17.1 Implement the Game main loop and session controls
    - Implement class-selection prompt (`s/h/w/v/q`, re-prompt on invalid), restart (`r`), quit (`q`), and the win/loss play-again prompt
    - _Requirements: 1.1, 1.6, 1.7, 11.3, 11.5_

  - [x] 17.2 Implement command dispatch with full redraw
    - Dispatch move/use/attack commands to Model actions, advance Spectres, and redraw the entire board after every command; ignore invalid commands without consuming a turn
    - _Requirements: 11.1, 11.4, 2.3_

  - [x] 17.3 Implement restart reset semantics
    - On `r`, reset stats, active scroll effects, RF total, and the global `lichAggroTriggered` flag, and require a new class selection
    - _Requirements: 11.2_

  - [ ]* 17.4 Write property test for turn consumption
    - **Property 11: Turn consumption**
    - **Validates: Requirements 5.3, 6.3, 8.2, 11.4**

- [x] 18. Implement command-line options
  - [x] 18.1 Implement argument parsing
    - Parse an optional layout-file argument and an optional seed argument, seeding `rng` once at startup (or from system time when absent)
    - _Requirements: 12.3_

  - [x] 18.2 Implement the LayoutLoader with digit mapping and error handling
    - Load all five levels from a layout file (excluding status rows), map digits 0–9 to scrolls/fragments, and report missing/malformed files at startup
    - _Requirements: 12.1, 12.2, 12.4_

  - [x] 18.3 Implement seed error handling
    - On a non-numeric seed argument, report the error and seed from system time instead
    - _Requirements: 12.5_

- [x] 19. Integration and final wiring
  - [x] 19.1 Wire all components into a runnable game
    - Connect Controller, Model, View, Spawner, and CLI parsing in `main`; ensure a full run (class selection → play → win/loss → play again) executes end to end
    - _Requirements: 11.1, 11.5_

  - [ ]* 19.2 Write seeded integration tests
    - Run seeded end-to-end scenarios asserting spawn counts, Arcanist/stairway separation, a scripted win score, and the Lich-aggro persistence scenario
    - _Requirements: 4.11, 13.1, 7.8_

- [x] 20. Final checkpoint - Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional test tasks and can be skipped for a faster MVP.
- The task order mirrors the design's Build Order & Layering, so partial submissions remain compilable and demoable at each layer.
- Each property test references a numbered correctness property from `design.md` and the requirement clauses it validates for traceability.
- Checkpoints ensure incremental validation before moving to the next layer.

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1.1"] },
    { "id": 1, "tasks": ["1.2"] },
    { "id": 2, "tasks": ["1.3", "2.1"] },
    { "id": 3, "tasks": ["2.2"] },
    { "id": 4, "tasks": ["2.3", "2.4"] },
    { "id": 5, "tasks": ["3.1", "5.1", "5.2", "5.3"] },
    { "id": 6, "tasks": ["3.2", "3.3", "5.4"] },
    { "id": 7, "tasks": ["6.1", "7.1"] },
    { "id": 8, "tasks": ["6.2", "6.3", "7.2", "7.3"] },
    { "id": 9, "tasks": ["6.4", "6.5", "7.4", "7.5"] },
    { "id": 10, "tasks": ["7.6", "9.1", "10.1"] },
    { "id": 11, "tasks": ["7.7", "7.8", "9.2", "10.2", "10.3"] },
    { "id": 12, "tasks": ["7.9", "9.3", "9.4", "10.4", "10.5"] },
    { "id": 13, "tasks": ["10.6", "12.1", "13.1"] },
    { "id": 14, "tasks": ["12.2", "13.2"] },
    { "id": 15, "tasks": ["12.3", "12.4", "12.6", "13.3"] },
    { "id": 16, "tasks": ["12.5", "12.7", "13.4"] },
    { "id": 17, "tasks": ["15.1"] },
    { "id": 18, "tasks": ["15.2", "15.3", "15.5", "15.7", "15.9"] },
    { "id": 19, "tasks": ["15.4", "15.6", "15.8", "15.10", "15.11"] },
    { "id": 20, "tasks": ["16.1", "16.3"] },
    { "id": 21, "tasks": ["16.2", "17.1"] },
    { "id": 22, "tasks": ["17.2", "17.3", "18.1"] },
    { "id": 23, "tasks": ["17.4", "18.2", "18.3"] },
    { "id": 24, "tasks": ["19.1"] },
    { "id": 25, "tasks": ["19.2"] }
  ]
}
```
