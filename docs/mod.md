# The Geode mod

`mod/` is a [Geode](https://geode-sdk.org) mod, "GD Pathfinder Tools". It gets
data out of the game for this project. It does not play levels and never
changes one.

**It compiles in CI but has not been run in the game yet.** Button placement,
the popups and the contents of the exported files are all unchecked.

## Install

1. Open the latest successful **Build mod** run under the repository's
   Actions tab and download the `gd-pathfinder-mod` artifact.
2. Unzip it to get the `.geode` file.
3. In the game, open the Geode menu and install the mod from that file. It
   needs Geode 5.11.1 or newer on Geometry Dash 2.2081.

## What it adds

| Where | Button | What it does |
| --- | --- | --- |
| A level's page (online levels and your own) | **PF** | Runs the level check from this repository on the level and shows the verdict. **Export** in that popup saves the level as a `.gmd`. |
| The pause menu while playing | **OBJ** | Saves every loaded object's type and hitbox to a CSV, plus a `.gmd` of the same level. |

Files go to the `exports` folder inside the mod's save folder. The popup shown
after each export has an **Open Folder** button.

File names come from the level name, with anything other than letters, digits,
spaces, `-` and `_` replaced by `_`. Online levels get their ID appended, for
example `My Layout_12345678.gmd`.

## The level check

The mod links the `gdpf` library from this repository and calls the same
`parse_level()` and `analyze()` as `gdpf inspect`. The verdict means the same
thing: `supported`, `unverified` (some object IDs are not in the object table)
or `unsupported`. It says nothing yet about whether a level can be beaten.

## The .gmd export

The file is the game's own serialisation of the level, which is what GDShare
writes for `.gmd`, so GDShare should be able to import it. `gdpf inspect` reads
it by finding the `k4` entry.

## The object dump

One row per object, in the order the game holds them:

| Column | Meaning |
| --- | --- |
| `id` | Object ID, the same number as key 1 in the level string |
| `type` | The game's own object type (table below) |
| `x`, `y` | The object's current position in the game's coordinates |
| `start_x`, `start_y` | The position it was placed at |
| `rotation` | Degrees, clockwise |
| `scale_x`, `scale_y` | Scale as applied to the sprite |
| `flip_x`, `flip_y` | 1 if flipped |
| `rect_x`, `rect_y` | Left and bottom edge of the hitbox |
| `rect_w`, `rect_h` | Width and height of the hitbox |
| `radius` | Above zero for circular hitboxes such as saws |
| `no_touch` | 1 if the object is set to not collide |
| `passable` | 1 if the object is set to passable |
| `trigger` | 1 if the game treats it as a trigger |

What this is for:

- **The object table.** `type`, `rect_w`, `rect_h` and `radius` per `id` are
  exactly what `src/objects.cpp` currently guesses for a few dozen IDs.
- **The vertical offset.** Comparing `y` here with key 3 of the same object in
  the `.gmd` gives the offset between level-string and in-game coordinates.

Not yet known, and to be settled by looking at a real dump:

- Whether the hitbox of a rotated object is reported before or after rotation.
- Whether `rect_w` and `rect_h` already include the object's scale.
- Whether the hitbox is filled in for objects far from the player.

There is no code yet that loads a dump back into the simulator. That comes
once a real dump has answered the questions above.

### Object types

`type` is the game's `GameObjectType`:

| | | | |
| --- | --- | --- | --- |
| 0 Solid | 2 Hazard | 3 Inverse gravity portal | 4 Normal gravity portal |
| 5 Ship portal | 6 Cube portal | 7 Decoration | 8 Yellow pad |
| 9 Pink pad | 10 Gravity pad | 11 Yellow orb | 12 Pink orb |
| 13 Gravity orb | 14 Inverse mirror portal | 15 Normal mirror portal | 16 Ball portal |
| 17 Regular size portal | 18 Mini size portal | 19 UFO portal | 20 Modifier |
| 21 Breakable | 22 Secret coin | 23 Dual portal | 24 Solo portal |
| 25 Slope | 26 Wave portal | 27 Robot portal | 28 Teleport portal |
| 29 Green orb | 30 Collectible | 31 User coin | 32 Drop (black) orb |
| 33 Spider portal | 34 Red pad | 35 Red orb | 36 Custom orb |
| 37 Dash orb | 38 Gravity dash orb | 39 Collision object | 40 Special |
| 41 Swing portal | 42 Gravity toggle portal | 43 Spider orb | 44 Spider pad |
| 45 Enter effect object | 46 Teleport orb | 47 Animated hazard | |

## Building it yourself

CI is the easy way. To build locally you need the Geode SDK and CLI set up as
described in the Geode documentation, then:

```sh
cmake -S mod -B mod/build
cmake --build mod/build --config Release
```

The mod's CMake project pulls in the repository root as a subproject, so the
library is always built from the same commit.

## Code

- `mod/src/main.cpp`: the hooks, buttons and file writing. Needs Geode.
- `mod/src/format.hpp`: file names, the report text and CSV rows. No Geode or
  game headers, so it is tested by `tests/test_modformat.cpp` off the game.
