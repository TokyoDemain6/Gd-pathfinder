# Gd-pathfinder

An offline pathfinder for Geometry Dash layouts: read a level, simulate the
player's physics outside the game, and search for a sequence of inputs that
completes it.

## Status

Early. The level parser works. The physics simulator is a **skeleton that has
not been compared with the game yet**: expect it to drift from the real game
until it has been calibrated against traces recorded in-game. There is no
pathfinder and no mod yet.

| Part | State |
| --- | --- |
| Level parser (decoded strings, encoded strings, `.gmd` exports) | Working, tested on synthetic levels only |
| "Can we handle this level?" check | Working, limited by the object table (below) |
| Simulator: cube, ship, ball, UFO, wave, robot, swing | Written from references, unverified |
| Simulator: spider | Walks and falls; clicking stops the run |
| Solids, spikes, saws, pads, orbs, portals | Simplified, unverified |
| Slopes, dash orbs, teleport portals, dual mode | Not implemented |
| Trace logger mod (records the real game for comparison) | Not started |
| Pathfinder | Not started |

Two limits matter most right now:

- **The object table is tiny.** Only a few dozen object IDs are known (basic
  blocks, three spikes, three saws, pads, orbs, portals, slopes, triggers).
  Any other ID is treated as decoration, so a level using other blocks or
  spikes is reported as `unverified` and its simulation cannot be trusted. The
  real table has to be dumped from the game.
- **Nothing is calibrated.** See [docs/physics.md](docs/physics.md) for what
  each number is based on.

## Scope

Classic (non-platformer) levels with no gameplay-affecting triggers: static
layouts. Levels outside that are detected and rejected rather than simulated
wrongly. Moving objects, toggles and the like are out of scope for now.

The simulator never guesses through something it does not implement. Touching
a slope, using a dash orb, clicking as the spider and so on stop the run with a
named reason.

## Build

Needs CMake 3.20+ and a C++20 compiler. No other dependencies.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Built and tested with GCC 13 and Clang 18 on Linux. Not yet built on Windows.

## Use

```sh
# Is this level inside what the simulator handles?
build/gdpf inspect level.gmd

# Run the simulator over an input script and write a per-tick trace.
build/gdpf sim level.gmd --inputs inputs.txt --trace trace.csv
```

A level file is a level string (decoded or encoded) or a `.gmd` export. An
input script lists button changes by tick, at 240 ticks per second:

```
# tick  1 = press, 0 = release
179 1
180 0
```

`scripts/gdlevel.py` builds and decodes level strings, which is how the test
levels are made.

## Layout

```
include/gdpf/   public headers
src/            parser, object table, level check, simulator
tools/          the gdpf command-line tool
tests/          tests and synthetic fixtures
scripts/        level-string helpers, fixture generator
docs/           level format, physics notes, trace format
```

## Next steps

1. Trace logger: a Geode mod that records the player's state every tick and
   dumps every object's ID and hitbox.
2. Replace the built-in object table with the dump.
3. Calibrate: compare simulator traces with game traces on one-mechanic test
   levels and fix the first tick that differs, mechanic by mechanic.
4. Pathfinder over the simulator, then replay and repair in the game.
5. Slopes, spider, dash orbs, teleport portals, dual mode.

## Sources

The physics is reimplemented from what these projects document about the
game. No code is copied from them.

- [camila314/gdp](https://github.com/camila314/gdp): decompiled player update
  and game loop (no licence stated).
- [matcool/gd-decomps](https://github.com/matcool/gd-decomps): decompiled
  player update for 2.1 (MIT).
- [camila314/pathfinder](https://github.com/camila314/pathfinder): an existing
  simulator-based pathfinder mod; used as a source of measured numbers. Its
  README says the code is not licensed for redistribution.

Geometry Dash is made by RobTop Games. This project is not affiliated with it.
