# Requirements Document

## Introduction

RuneVault (RV) is a single-player, terminal-based dungeon-crawler roguelike. The player controls an **Arcanist** who descends through five fixed-layout vault levels, collecting **Rune Fragments**, using **Arcane Scrolls**, defeating **Spectres**, and claiming a guarded **Shard Coffer** on each level. The run is won by reaching and descending the stairway on level 5 and is lost when the Arcanist's Focus Points reach 0. The board is redrawn in full after every command rather than updated incrementally.

These requirements are derived from the approved technical design (`design.md`) and are traceable to the official project specification (`_spec_extracted.txt`), the Piazza clarifications (`piazza.txt`), and the optional bonus features (`bonus_feats.txt`). Each requirement is written to be precise and testable so that the design's correctness properties can be linked back to specific acceptance criteria. Requirements 1–13 describe the core game; Requirement 14 describes clearly-marked optional bonus features that must not weaken or replace core behavior.

Terminology, numeric constants (base stats, damage formula, spawn counts, spawn distribution), and resolved symbol/geometry decisions come directly from the design's Design Decisions (DD-1 through DD-7) and tables. Where the source specification was internally inconsistent, the Piazza-resolved interpretation is authoritative.

## Glossary

- **RuneVault (RV)**: The game system as a whole, comprising the Model, View, and Controller.
- **Arcanist (AC)**: The single player character, always rendered as `@`. Has one of four classes.
- **Class**: The Arcanist archetype (Sage, Hexblade, Warden, Voidwalker) that determines base stats and one class ability.
- **Focus Points (FP)**: The Arcanist's magical reserves, equivalent to hit points. The run ends in loss when FP reaches 0.
- **Power (Pwr)**: A character's offensive strength, used as the attacker term in the damage formula.
- **Ward Rating (WR)**: A character's magical resistance (defense), used as the defender term in the damage formula.
- **Spectre**: A non-playable enemy inhabiting the vault (Wraith, Banshee, Revenant, Shade, Lich, Vault Anchor, Specter Lord).
- **Rune Fragment (RF)**: The scoring currency, rendered as `*`. Grades: Common shard (1), Resonant shard (2), Lich cache (4), Shard Coffer (6).
- **Ward**: A magical lock on the descent to the next level, lifted only while the Arcanist carries that level's Cipher Gem.
- **Cipher Gem**: A unique per-level item (rendered `C`) carried by one non-Vault-Anchor Spectre; when that Spectre is defeated it drops and, once picked up, lifts the Ward and reveals the stairway.
- **Shard Coffer**: The level's Shard (a Rune Fragment worth 6, rendered `$` per DD-7), guarded by a Vault Anchor and collectible only after the Anchor is defeated.
- **Aegis Cloak**: A once-per-run guarded major item (rendered `G`) on a random level 2–4; while carried it halves damage taken (rounded up).
- **Vault Anchor**: A stationary Spectre that always guards a Shard Coffer and is hostile only when the Arcanist is adjacent to that Coffer.
- **Lich**: A Spectre that is non-hostile until attacked; attacking any Lich sets a global, run-scoped hostility flag.
- **Ward Gating**: The rule set that blocks descent and item collection until their unlocking conditions are met.
- **Chamber**: One of the five fixed interior regions (9 columns × 4 rows) of a vault level. Confines Spectre movement.
- **Corridor / Archway**: Transit cells connecting chambers; a corridor (`#`) meets a chamber at an archway (`+`).
- **Cell**: A single board tile: vault wall, floor tile, archway, or corridor segment.
- **1-Block Radius**: The 8 cells immediately adjacent (orthogonally and diagonally) to a character or item.
- **Vault Level**: A predefined configuration of 5 chambers connected by corridors; identical layout on all 5 levels.
- **Playable Region**: The 79 columns × 25 rows area of the board; the bottom 5 of the 30 total rows are the status area.
- **Status Area**: The bottom 5 rows of the display showing class, FP, Pwr, WR, RF total, scroll hints, and messages.
- **RNG**: The provided pseudo-random number utilities (`shuffle.h`/`shuffle.cpp`), seeded once at startup.
- **Spawn**: The placement of an actor, fragment, or scroll on the board at level start.
- **Run**: A single playthrough from class selection until win, loss, restart, or quit.

## Requirements

### Requirement 1: Game Setup & Class Selection

**User Story:** As a player, I want to select an Arcanist class or quit at the start of a run, so that I can begin with the archetype whose stats and ability suit my play style.

#### Acceptance Criteria

1. WHEN a new run begins, THE RuneVault_System SHALL prompt the player to select an Arcanist class or quit.
2. WHEN the player enters `s`, THE RuneVault_System SHALL create a Sage Arcanist with 120 FP, 18 Pwr, and 18 WR.
3. WHEN the player enters `h`, THE RuneVault_System SHALL create a Hexblade Arcanist with 100 FP, 24 Pwr, and 14 WR.
4. WHEN the player enters `w`, THE RuneVault_System SHALL create a Warden Arcanist with 150 FP, 16 Pwr, and 22 WR.
5. WHEN the player enters `v`, THE RuneVault_System SHALL create a Voidwalker Arcanist with 110 FP, 22 Pwr, and 12 WR.
6. WHEN the player enters `q` at the class-selection prompt, THE RuneVault_System SHALL quit without starting a run.
7. IF the player enters input other than `s`, `h`, `w`, `v`, or `q` at the class-selection prompt, THEN THE RuneVault_System SHALL re-prompt for class selection without starting a run.
8. WHEN an Arcanist is created, THE RuneVault_System SHALL set the Arcanist's maximum FP equal to the class's starting FP.

### Requirement 2: Board Rendering & Display

**User Story:** As a player, I want the full board and my status redrawn after every command, so that I can always see the current game state in a stable, readable layout.

#### Acceptance Criteria

1. THE RuneVault_System SHALL render a board that is 79 columns wide and 30 rows tall.
2. THE RuneVault_System SHALL reserve the bottom 5 rows of the board as the status area, leaving a 79-column by 25-row playable region.
3. WHEN any command is processed, THE RuneVault_System SHALL redraw the entire board.
4. WHILE rendering, THE RuneVault_System SHALL display the Arcanist as `@`, vault walls as `|` and `-`, archways as `+`, corridors as `#`, floor tiles as `.`, Rune Fragments as `*`, Arcane Scrolls as `?`, the Cipher Gem as `C`, the Shard Coffer as `$`, and the Aegis Cloak as `G`.
5. WHILE rendering, THE RuneVault_System SHALL display Spectres as `W` (Wraith), `B` (Banshee), `R` (Revenant), `S` (Shade), `L` (Lich), `A` (Vault Anchor), and `X` (Specter Lord).
6. WHILE the Cipher Gem for the current level has not been picked up, THE RuneVault_System SHALL render the stairway tile as a floor tile `.`.
7. WHILE the Cipher Gem for the current level is carried, THE RuneVault_System SHALL render the stairway tile as `\`.
8. WHILE rendering the status area, THE RuneVault_System SHALL display the Arcanist's class, FP, Pwr, WR, and total Rune Fragments collected.
9. WHERE ANSI colour output is enabled, THE RuneVault_System SHALL apply colour codes to distinguish the Arcanist, stairway, Spectres, Rune Fragments, and Scrolls.

### Requirement 3: Vault Level Structure

**User Story:** As a player, I want each vault level to use the same fixed chamber-and-corridor layout, so that I can learn the map and navigate reliably across all five levels.

#### Acceptance Criteria

1. THE RuneVault_System SHALL construct each vault level from exactly 5 chambers.
2. THE RuneVault_System SHALL give each chamber a fixed interior size of 9 columns by 4 rows.
3. THE RuneVault_System SHALL connect chamber 1 to chamber 2, chamber 1 to chamber 3, chamber 2 to chamber 4, chamber 3 to chamber 4, and chamber 4 to chamber 5.
4. THE RuneVault_System SHALL ensure every chamber is reachable from every other chamber.
5. WHERE a corridor meets a chamber, THE RuneVault_System SHALL place an archway `+` at the junction on the chamber wall.
6. THE RuneVault_System SHALL use the same chamber-and-corridor layout on all five levels.
7. THE RuneVault_System SHALL draw the level layout tile-for-tile at its fixed dimensions without scaling it to fill the playable region.
8. THE RuneVault_System SHALL fit the entire level layout within the 79-column by 25-row playable region.

### Requirement 4: Level Generation & Spawning

**User Story:** As a player, I want each level populated with actors and items according to fixed, reproducible rules, so that every run is fair, varied, and repeatable from a seed.

#### Acceptance Criteria

1. WHEN a level is generated, THE Spawner SHALL place objects in the order: Arcanist location, stairway location, scrolls, rune fragments, spectres.
2. WHEN placing the Arcanist, THE Spawner SHALL choose a floor tile in a chamber that is not the stairway's chamber, selecting the chamber uniformly at random and the floor tile within that chamber uniformly at random.
3. WHEN placing the stairway, THE Spawner SHALL choose a floor tile in a chamber different from the Arcanist's chamber.
4. WHEN placing scrolls, THE Spawner SHALL place exactly 10 scrolls per level, selecting each scroll's type from the 6 scroll types with equal probability of 1/6.
5. WHEN placing rune fragments, THE Spawner SHALL place exactly 1 Shard Coffer guarded by a Vault Anchor, plus exactly 9 additional fragment piles that are each a common shard with probability 5/7 or a resonant shard with probability 2/7.
6. WHEN placing the Vault Anchor for a Shard Coffer, THE Spawner SHALL position the Vault Anchor within a 1-block radius of that Shard Coffer.
7. WHEN placing spectres, THE Spawner SHALL place exactly 20 spectres per level using the distribution Wraith 2/9, Banshee 3/18, Shade 5/18, Revenant 1/9, Specter Lord 1/9, and Lich 1/9.
8. WHEN selecting a chamber for any spawned scroll, fragment, or spectre, THE Spawner SHALL make every chamber equally likely.
9. THE Spawner SHALL place spectres and items only on floor tiles and never on archways, corridors, or the stairway.
10. THE Spawner SHALL ensure no two placed objects occupy the same cell, except that rune fragments may share the Arcanist's cell transiently during walk-over collection.
11. WHEN the RNG is seeded with a given seed value, THE Spawner SHALL produce an identical sequence of placements across runs.
12. WHEN a level's spectres have been placed, THE Spawner SHALL choose exactly one non-Vault-Anchor spectre uniformly at random to carry the Cipher Gem for that level.
13. WHEN a run begins, THE Spawner SHALL select one level from levels 2 through 4 uniformly at random on which to place the Aegis Cloak using the guarded-placement rules of a Shard Coffer.

### Requirement 5: Arcanist Movement

**User Story:** As a player, I want to move the Arcanist in any of eight directions across walkable cells, so that I can explore the vault, collect fragments, and reach objectives.

#### Acceptance Criteria

1. WHEN the player enters `no`, `so`, `ea`, `we`, `ne`, `nw`, `se`, or `sw`, THE RuneVault_System SHALL move the Arcanist one cell in the specified direction.
2. WHERE the target cell is a floor tile, archway, or corridor, THE RuneVault_System SHALL allow the Arcanist to enter that cell.
3. IF the target cell is a vault wall or outside the playable region, THEN THE RuneVault_System SHALL reject the move and SHALL NOT consume the turn.
4. WHEN the Arcanist enters a cell containing a rune fragment, THE RuneVault_System SHALL add that fragment's value to the total Rune Fragments and remove the fragment from the board.
5. WHEN the Arcanist enters a cell containing a guarded item, THE RuneVault_System SHALL allow the Arcanist to occupy that cell regardless of whether the guarding Vault Anchor is defeated.

### Requirement 6: Combat Resolution

**User Story:** As a player, I want deterministic, rule-based combat with clear initiative and damage math, so that I can make tactical decisions about when to attack.

#### Acceptance Criteria

1. WHEN the player enters `a <direction>` and a Spectre is immediately adjacent in that direction, THE RuneVault_System SHALL resolve an Arcanist attack against that Spectre before any Spectre acts.
2. WHEN the Arcanist attacks a Spectre, THE RuneVault_System SHALL always hit and SHALL reduce the Spectre's FP by `ceil( (100 / (100 + WR_defender)) * Pwr_attacker )`.
3. IF the player enters `a <direction>` and no Spectre is immediately adjacent in that direction, THEN THE RuneVault_System SHALL treat the command as a no-op and SHALL NOT consume the turn.
4. WHEN a hostile Spectre is within a 1-block radius of the Arcanist during the Spectre action phase, THE Spectre SHALL attack instead of moving.
5. WHEN a Spectre attacks, THE RuneVault_System SHALL apply a 50% chance that the attack misses and deals no damage.
6. WHEN a Spectre attack hits, THE RuneVault_System SHALL compute base damage as `ceil( (100 / (100 + WR_Arcanist)) * Pwr_Spectre )`.
7. WHILE the Arcanist is a Warden and no Aegis Cloak is carried, THE RuneVault_System SHALL reduce incoming Spectre damage to `max(1, base_damage - 1)`.
8. WHILE the Arcanist carries the Aegis Cloak and is not a Warden, THE RuneVault_System SHALL reduce incoming Spectre damage to `max(1, ceil(base_damage / 2))`.
9. WHILE the Arcanist is a Warden and carries the Aegis Cloak, THE RuneVault_System SHALL compute incoming Spectre damage as `max(1, ceil((base_damage - 1) / 2))`.
10. WHEN a non-Vault-Anchor, non-Lich Spectre is defeated, THE RuneVault_System SHALL add 1 Rune Fragment to the total.
11. WHEN a Lich is defeated, THE RuneVault_System SHALL add a Lich cache worth 4 Rune Fragments to the total.
12. WHEN a Spectre carrying the Cipher Gem is defeated, THE RuneVault_System SHALL drop the Cipher Gem on that Spectre's tile.

### Requirement 7: Spectre Behavior

**User Story:** As a player, I want Spectres to behave by consistent movement and hostility rules, so that I can anticipate and outmaneuver them.

#### Acceptance Criteria

1. WHEN the Arcanist completes an action, THE RuneVault_System SHALL process Spectre actions row by row, left to right, moving each Spectre at most once.
2. WHEN a mobile Spectre moves and is not attacking, THE RuneVault_System SHALL move it to a uniformly-random unoccupied floor tile within its spawn chamber selected from its 1-block radius.
3. THE RuneVault_System SHALL never allow a Spectre to leave the chamber in which it was spawned.
4. THE RuneVault_System SHALL keep every Vault Anchor stationary.
5. WHILE the Arcanist is adjacent to a Vault Anchor's guarded Shard Coffer, THE RuneVault_System SHALL treat that Vault Anchor as hostile.
6. WHILE the Arcanist is not adjacent to a Vault Anchor's guarded Shard Coffer, THE RuneVault_System SHALL treat that Vault Anchor as non-hostile.
7. WHEN any Lich is attacked, THE RuneVault_System SHALL set the global run-scoped Lich hostility flag to true.
8. WHILE the global Lich hostility flag is true, THE RuneVault_System SHALL treat every already-spawned and future Lich as hostile.
9. WHILE the global Lich hostility flag is false, THE RuneVault_System SHALL treat every Lich as non-hostile.
10. THE RuneVault_System SHALL never cause a Spectre to pursue the Arcanist outside its spawn chamber.

### Requirement 8: Arcane Scrolls

**User Story:** As a player, I want to use scrolls of six distinct types to buff or affect the Arcanist, so that I can adapt to combat and survival needs.

#### Acceptance Criteria

1. WHEN the player enters `u <direction>` and a scroll is within a 1-block radius in that direction, THE RuneVault_System SHALL apply that scroll's effect and remove the scroll from the board.
2. IF the player enters `u <direction>` and no scroll is within a 1-block radius in that direction, THEN THE RuneVault_System SHALL treat the command as a no-op and SHALL NOT consume the turn.
3. WHEN an Attune Focus scroll is used, THE RuneVault_System SHALL restore up to 10 FP without exceeding the class maximum FP, as a permanent change.
4. WHEN a Drain Focus scroll is used, THE RuneVault_System SHALL reduce FP by up to 10 without falling below 0, as a permanent change.
5. WHEN a Surge Power scroll is used, THE RuneVault_System SHALL increase the Arcanist's effective Pwr by 5 for the current level only.
6. WHEN a Fortify Ward scroll is used, THE RuneVault_System SHALL increase the Arcanist's effective WR by 5 for the current level only.
7. WHEN a Sap Power scroll is used, THE RuneVault_System SHALL decrease the Arcanist's effective Pwr by 5 for the current level only, without falling below 0.
8. WHEN an Erode Ward scroll is used, THE RuneVault_System SHALL decrease the Arcanist's effective WR by 5 for the current level only, without falling below 0.
9. THE RuneVault_System SHALL render every scroll as `?` regardless of whether its type has been used.
10. WHEN a scroll type is used for the first time in a run, THE RuneVault_System SHALL record that type as remembered for the remainder of the run.
11. WHILE the Arcanist is a Hexblade, THE RuneVault_System SHALL double the magnitude of every scroll effect.
12. WHILE the Arcanist is a Voidwalker and a negative scroll is used, THE RuneVault_System SHALL apply a 50% chance to produce the positive counterpart of that effect instead.

### Requirement 9: Items & Major Items

**User Story:** As a player, I want to collect rune fragments and unlock guarded major items, so that I can grow my score and gain advantages toward completing the run.

#### Acceptance Criteria

1. THE RuneVault_System SHALL value a common shard at 1, a resonant shard at 2, a Lich cache at 4, and a Shard Coffer at 6 Rune Fragments.
2. WHEN the Cipher Gem's carrier Spectre is defeated, THE RuneVault_System SHALL place the Cipher Gem on that Spectre's tile.
3. WHEN the Arcanist walks over a dropped Cipher Gem, THE RuneVault_System SHALL mark the Cipher Gem as carried, lift the Ward on the descent, and reveal the stairway.
4. WHILE a Shard Coffer's guarding Vault Anchor is alive, THE RuneVault_System SHALL prevent collection of that Shard Coffer.
5. WHEN a Shard Coffer's guarding Vault Anchor is defeated, THE RuneVault_System SHALL allow the Arcanist to collect that Shard Coffer.
6. THE RuneVault_System SHALL place the Aegis Cloak exactly once per run, on one randomly selected level from levels 2 through 4, guarded by a Vault Anchor.
7. WHILE a guarding Vault Anchor is alive, THE RuneVault_System SHALL prevent collection of the Aegis Cloak it guards.
8. WHILE the Arcanist carries the Aegis Cloak, THE RuneVault_System SHALL halve all damage taken, rounded up, subject to the combat damage-ordering rules.

### Requirement 10: Level Transition, Win & Lose

**User Story:** As a player, I want clear rules for descending between levels and for winning or losing, so that I understand how to progress and what ends the run.

#### Acceptance Criteria

1. WHILE the Arcanist does not carry the current level's Cipher Gem, THE RuneVault_System SHALL prevent descent to the next level.
2. WHILE the Arcanist does not carry the current level's Cipher Gem, THE RuneVault_System SHALL treat walking onto the stairway tile as having no effect.
3. WHEN the Arcanist descends the stairway while carrying the Cipher Gem on a level from 1 through 4, THE RuneVault_System SHALL advance to the next level.
4. WHEN the Arcanist descends the stairway while carrying the Cipher Gem on level 5, THE RuneVault_System SHALL end the run as a win.
5. IF the Arcanist's FP reaches 0, THEN THE RuneVault_System SHALL end the run as a loss.
6. WHEN the Arcanist descends to the next level, THE RuneVault_System SHALL remove all current-level-only scroll effects so that effective Pwr and WR return to their base values.
7. WHEN the Arcanist descends to the next level, THE RuneVault_System SHALL preserve permanent FP changes made during prior levels.
8. WHEN the Arcanist descends to the next level, THE RuneVault_System SHALL reset the carried Cipher Gem state and retain the Aegis Cloak if it was collected.

### Requirement 11: Command Interpreter

**User Story:** As a player, I want a consistent command interpreter for moving, using scrolls, attacking, restarting, and quitting, so that I can control the game predictably.

#### Acceptance Criteria

1. WHEN the player enters a movement, use, or attack command during active play, THE RuneVault_System SHALL dispatch it to the corresponding game action and redraw the board.
2. WHEN the player enters `r`, THE RuneVault_System SHALL reset all Arcanist stats, active scroll effects, total Rune Fragments, and the global Lich hostility flag, and SHALL require a new class selection.
3. WHEN the player enters `q`, THE RuneVault_System SHALL quit the game.
4. IF the player enters an input that is not a valid command during active play, THEN THE RuneVault_System SHALL ignore it and SHALL NOT consume the turn.
5. WHEN a run ends by win or loss, THE RuneVault_System SHALL offer the player the option to play again.

### Requirement 12: Command-Line Options

**User Story:** As a player or tester, I want optional command-line arguments for a fixed layout file and an RNG seed, so that I can define custom levels and reproduce runs.

#### Acceptance Criteria

1. WHERE a level-layout file argument is provided, THE RuneVault_System SHALL load the exact layout of all five levels from that file, excluding the 5 status rows.
2. WHERE a level-layout file is loaded, THE RuneVault_System SHALL map digits 0 through 9 to items as: 0 = Attune Focus, 1 = Surge Power, 2 = Fortify Ward, 3 = Drain Focus, 4 = Sap Power, 5 = Erode Ward, 6 = common shard, 7 = resonant shard, 8 = Lich cache, 9 = Shard Coffer.
3. WHERE an RNG seed argument is provided, THE RuneVault_System SHALL seed the random number generator with that value once at startup so that runs are reproducible.
4. IF a provided level-layout file is missing or malformed, THEN THE RuneVault_System SHALL report the error at startup.
5. IF a provided RNG seed argument is non-numeric, THEN THE RuneVault_System SHALL report the error and seed the generator from the system time instead.

### Requirement 13: Scoring

**User Story:** As a player, I want a score computed on a win based on the fragments I collected, so that I can measure and compare my performance.

#### Acceptance Criteria

1. WHEN a run ends in a win, THE RuneVault_System SHALL compute a score equal to the total Rune Fragments collected.
2. WHILE the Arcanist is a Sage, THE RuneVault_System SHALL apply a 50% bonus to the final score on a win.
3. IF a run ends by loss, restart, or quit, THEN THE RuneVault_System SHALL NOT produce a score.

### Requirement 14: Optional Bonus Features

**User Story:** As a player seeking extra challenge, I want optional advanced mechanics that build on the core game without weakening it, so that the game offers greater depth when enabled.

> **Note:** The acceptance criteria in this requirement are **OPTIONAL / bonus** features. They must extend the core game and must never replace, disable, or make easier any behavior required by Requirements 1–13.

#### Acceptance Criteria

1. WHERE the Prestige upgrade feature is enabled and the run reaches level 3, THE RuneVault_System SHALL upgrade the Arcanist's class ability by swapping its class strategy (for example, Sage to Archmage) at runtime without recompilation.
2. WHERE the Prestige upgrade feature is enabled, THE RuneVault_System SHALL preserve all core combat, scoring, and scroll behavior for the upgraded Arcanist except for the upgraded ability.
3. WHERE the Spectre pursuit feature is enabled and the Arcanist enters a pursuing Spectre's line of sight, THE RuneVault_System SHALL switch that Spectre to a pursuit movement behavior that paths toward the Arcanist across corridors.
4. WHERE the Spectre pursuit feature is enabled, THE RuneVault_System SHALL continue to support base-game wandering behavior for non-pursuing Spectres at runtime without recompilation.
