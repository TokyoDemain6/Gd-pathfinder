# Future features

A plan, not a promise. **Nothing in this document is built.** Order and scope
will change as the project learns what works in the game.

## Who this is for

Players and creators using the mod in the game. Features are judged by what
they do for that person. Tools that mostly help develop the project itself are
listed under [stretch goals](#stretch-goals).

Most levels people will want to use this on have **one intended path**. Every
feature is designed for that case first. Levels with real branches are a
stretch goal.

## What has to exist first

Almost everything below needs these three, in this order:

1. **A calibrated simulator.** The real object table from the game, and
   physics checked against traces recorded in the game.
2. **The pathfinder.** A search over the simulator that produces a macro.
3. **Replay and repair.** Play the macro in the game and fix it where the
   simulator was wrong.

Each feature below says which of these it needs.

## Planned features

### 1. Search settings

Settings shown before a search starts, with sensible defaults so nobody has to
touch them.

#### Search budget

Limits on how hard the pathfinder tries.

- Limits on time, on positions explored, and on how far back it may undo.
- Presets: **Quick**, **Normal**, **Exhaustive**.
- A progress display with a cancel button.
- When a limit is hit, it reports the furthest point reached and where it got
  stuck, instead of only saying it failed.

Needs: pathfinder.

#### Safety margin

On a single path there are still thousands of input timings that survive. A
search naturally returns one that scrapes past hazards with one-tick timing.
This setting chooses a better one from the same path. It does not pick between
routes.

What it changes:

- Presses land in the middle of their timing window, not at its edge.
- The path keeps its distance from spikes and block edges where there is room.
- Ship, UFO, swing and wave sections fly straighter, with fewer corrections.

A slider from **Tight** (first path found, fastest search) to **Safe** (widest
margins). The default sits in between.

Why it matters beyond looks:

- A macro with margin still works when the simulator is slightly wrong, so it
  needs less repair in the game.
- It is what makes the [play guide](#4-play-guide) usable: a human cannot hit
  one-tick windows, so the guide has to be built from a forgiving path.

How it could work: search with hazards enlarged by the margin and fall back to
a smaller margin only where no path exists; then, for each press, measure how
many ticks earlier or later still survive and move the press to the centre.

Needs: pathfinder.

#### Click style

What kind of inputs the path is allowed to use.

- **Fewest clicks**, or a cap on clicks per second.
- A minimum gap between a release and the next press.
- **Input rate**: only allow inputs every 4th tick, to find paths that are
  physically possible for someone playing at 60 fps.

Needs: pathfinder.

### 2. Level scan

A look at the level before searching, shown to the player.

- A timeline of the level: which gamemodes appear, in order, with speed, size
  and gravity changes, split into sections at each portal.
- Sections the simulator cannot handle are marked before any time is spent
  searching, with the reason.
- A confidence label per section, for example "ship: calibrated" or
  "spider: not implemented", so it is clear where a path can be trusted.
- The pathfinder uses the same sections to report progress ("solving ship
  section, 3 of 7").

This extends the level check in `gdpf inspect`, which the in-game mod also
exposes as a button.

Needs: nothing new for the timeline. Confidence labels need calibration.

### 3. Results

What the player sees once a level is solved, besides the macro.

- A summary: number of clicks, the tightest timing window, the hardest section.
- A graph of timing windows along the level, with frame-perfect inputs marked.
- Macro export in the formats common bots read.

Needs: pathfinder.

### 4. Play guide

Shows a solved path to someone playing the level themselves, in the style of a
classic rhythm game.

```
   level                                   note lane
 ┌──────────────────────────────────┐      ┌─────┐
 │            ·  ·                  │      │  █  │  hold: a long bar, release at its end
 │        ·  x     ·                │      │  █  │
 │  ───x·            ·  x ·  ·      │      │     │
 │                          line of │      │  ■  │  tap: a short note
 │                          motion  │      ├─────┤  hit line: press when a note arrives
 └──────────────────────────────────┘      └─────┘
```

Two parts, each of which can be turned off:

- **Note lane.** A bar at the side of the screen with notes sliding toward a
  hit line. A tap is a short note, a hold is a long bar that ends where the
  button should be released. Notes are placed by time, so speed portals do not
  distort the lane.
- **Line of motion.** The path the player should follow, drawn in the level,
  with a mark at each press and release.

Options:

- Which side the lane is on, its size and opacity.
- Scroll speed, meaning how far ahead notes appear.
- Colour notes by gamemode.
- Show each note's timing window as its thickness, so wide windows look easy
  and tight ones look tight.
- Hit feedback (early, late, on time) comparing the player's input with the
  window.
- An optional tick sound on each note.
- Two lanes for two-player dual sections.

Things to solve:

- **Staying in step after an imperfect input.** A solved path assumes every
  earlier input was exact. A human will be a little early or late, which
  shifts everything after it. Two answers, probably both: build the guide from
  a high [safety margin](#safety-margin) path so small errors do not matter,
  and re-solve the next few seconds from where the player actually is.
- **Practice mode.** The guide has to pick up from a checkpoint or start
  position, not only from the beginning.
- **Fair play.** This is an assist. It should probably be limited to practice
  mode or to runs that do not count, and that needs checking against Geode's
  rules for mods before it is published.

Needs: pathfinder, safety margin. The re-solve needs a search fast enough to
run during play, which nothing else in this plan requires.

### 5. Creator tools

For people building levels.

- **Still possible?** A check in the editor that says whether the level can
  still be beaten after an edit, and highlights the section that broke.
- **Impossibility report.** Shows that no input sequence passes a section, for
  example "this jump cannot be made at 3x speed".
- **Survivable-space overlay.** Draws every position the player could be in
  and still survive. Unintended skips and secret ways show up as extra
  corridors.
- **Nerf advisor.** The smallest object move that turns a one-tick window into
  a comfortable one.
- **Auto-level check.** Does the level complete with no input at all?
- **Start position placer.** Places start positions at section boundaries with
  the right gamemode, speed, size and gravity.

Needs: calibrated simulator and pathfinder. The overlay and the impossibility
report need the search to cover every reachable position, not just find one
path.

### 6. Practice coach

Compares a player's recorded attempt with the solved path's timing windows and
says where they are off, for example "3 ticks late on average at 47%".

Needs: pathfinder, and a way to record the player's inputs.

## Stretch goals

Kept in mind, not scheduled.

| Idea | What it is | Why it waits |
| --- | --- | --- |
| Route choice | On levels that branch, pick the shortest, safest or all-coins route | Most target levels have one path |
| Batch scan | Run the level scan over a folder of exports and rank which missing mechanic would unlock the most levels | Developer-facing |
| Physics change detector | Every calibrated trace becomes a test, so a game update that changes physics is caught | Developer-facing |
| Music-synced route | Prefer clicks that land on the song's beats, as a guess at the intended route | Needs audio analysis |
| Metronome challenge | Can the level be beaten by tapping a fixed rhythm? | Novelty |
| Swag route and lazy route | The path passing closest to hazards, and the one with the fewest inputs | Novelty; lazy route overlaps with click style |
| Level evolver | Mutate a layout to be as hard as possible while still provably beatable | Large, and needs a very fast search |
| Song to layout | Give it a click pattern and it builds a layout that demands those clicks | Large; the reverse of everything else here |
| Gameplay fingerprint | Turn a level's click pattern into a barcode to find similar layouts or spot copies | Needs many solved levels |
| Browser version | Run the level scan and results in a web page | The library has no dependencies, so this is feasible, but it is a second product |
| Search replay | Animate the search exploring and backtracking | Presentation only |

## Suggested order

1. Pathfinder in the mod with the search budget, progress display and macro
   export.
2. Level scan timeline.
3. Safety margin and click style.
4. Results summary and timing-window graph.
5. Play guide: note lane and line of motion first, re-solve later.
6. Creator tools, starting with "still possible?".
7. Practice coach.

## Open questions

- Which macro formats to export first.
- Whether the play guide may be offered outside practice mode at all.
- How fast the search must be for the play guide's re-solve, and whether that
  is realistic on a phone.
- How the timing-window graph should treat holds, where both the press and the
  release have a window.
