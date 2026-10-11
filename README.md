# Gd-pathfinder

An offline pathfinder for Geometry Dash layouts: read a level, simulate the
player's physics outside the game, and search for a sequence of inputs that
completes it.

## Status

Early. The level parser works. The physics simulator is a **skeleton that has
not been compared with the game yet**: expect it to drift from the real game
until it has been calibrated against traces recorded in-game. There is no
pathfinder yet. A small Geode mod exports levels and object hitboxes from the
game; it compiles in CI but has not been run in the game.

| Part | State |
| --- | --- |
| Level parser (decoded strings, encoded strings, `.gmd` exports) | Working, tested on synthetic levels only |
| "Can we handle this level?" check | Working, limited by the object table (below) |
| Simulator: cube, ship, ball, UFO, wave, robot, swing | Written from references, unverified |
| Simulator: spider | Walks and falls; clicking stops the run |
| Solids, spikes, saws, pads, orbs, portals | Simplified, unverified |
| Slopes, dash orbs, teleport portals, dual mode | Not implemented |
| Geode mod: level check, `.gmd` export, object hitbox dump | Compiles in CI, not yet run in the game |
| Loading a hitbox dump into the simulator | Not started |
| Trace logger (records the real game for comparison) | Not started |
| Pathfinder | Not started |

Two limits matter most right now:

- **The object table is tiny.** Only a few dozen object IDs are known (basic
  blocks, three spikes, three saws, pads, orbs, portals, slopes, triggers).
  Any other ID is treated as decoration, so a level using other blocks or
  spikes is reported as `unverified` and its simulation cannot be trusted. The
  real table has to come from the game: the mod's object dump is the first
  half of that.
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

CI builds and tests the library on Linux, Windows and macOS, and builds the
mod for every platform Geode supports.

## Performance checks

Every push also runs the checks in [`bench/`](bench/README.md). They compare
the simulator's speed, memory use and behaviour with numbers recorded in
`bench/baseline.json`, and fail if any of them moved without the baseline
being updated in the same change. That includes the simulator giving a
different result on another platform or compiler.

```sh
python3 bench/perf.py            # run the checks
python3 bench/perf.py --update   # accept the new numbers
```

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

## The mod

`mod/` adds a **PF** button to level pages (level check and `.gmd` export) and
an **OBJ** button to the pause menu (object hitbox dump). Download the built
`.geode` file from the latest **Build mod** run under the Actions tab. See
[docs/mod.md](docs/mod.md) for installing it and for the dump format.

## Layout

```
include/gdpf/   public headers
src/            parser, object table, level check, simulator
tools/          the gdpf command-line tool
mod/            the Geode mod
tests/          tests and synthetic fixtures
bench/          performance checks and their recorded baseline
scripts/        level-string helpers, fixture generator
docs/           level format, physics notes, trace format, the mod
```

## Next steps

1. Run the mod in the game and check its exports against a real level.
2. Replace the built-in object table with a hitbox dump.
3. Trace logger: record the player's state every tick from the mod.
4. Calibrate: compare simulator traces with game traces on one-mechanic test
   levels and fix the first tick that differs, mechanic by mechanic.
5. Pathfinder over the simulator, then replay and repair in the game.
6. Slopes, spider, dash orbs, teleport portals, dual mode.

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

The mod's `.gmd` export uses the same game call as
[HJfod/GMD-API](https://github.com/HJfod/GMD-API), which defines the format.

Geometry Dash is made by RobTop Games. This project is not affiliated with it.
