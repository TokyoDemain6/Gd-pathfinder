# Performance checks

Every push runs the **Performance checks** workflow. It measures the
simulator, compares each number with [`baseline.json`](baseline.json), and
fails if a number moved and the baseline was not updated in the same change.

The rule is the same for every check: **a number may only move on purpose.**
If it got worse by accident, fix the code. If the move is intended, or is an
improvement, commit the new baseline alongside the change so the diff shows
exactly what moved.

## What is checked

| Check | What it measures | Fails when |
| --- | --- | --- |
| Simulator behaviour | 316 small levels, one per mechanic and gamemode (each pad, orb and portal, plus free play). Records how each run ends and a hash of the player's state on every tick. | Any run ends differently or takes a different path, even by one bit. |
| Same result everywhere | The same 316 runs on Linux (GCC, Clang, Arm), Windows and macOS. | Any platform disagrees with the baseline. |
| Collision work | Objects visited and hitbox tests per tick on fixed benchmark levels. Counted, so identical on every machine. | Either count changes. |
| CPU instructions | Instructions executed per tick and per level load, counted by valgrind. | A case moves more than 3%. |
| Memory per state | `sizeof(PlayerState)` and `sizeof(Collider)`. | Either changes. |
| Object table | How many object IDs the built-in table knows. | The total changes. |
| Test coverage | Share of library lines the tests run. | It moves more than 0.5 points. |
| Tests pin the physics | Each physics number is changed by 1% in a copy; a number is "pinned" if a test then fails. | A pinned number stops being pinned, or a number is added or removed. |

Wall-clock speed (ticks per second) is printed in every run but never fails
one: shared CI machines vary too much between runs. Instruction counts stand
in for it.

Two things these checks do **not** tell you: whether the simulator matches the
real game (that needs traces recorded in the game), and whether a behaviour
change is right. The behaviour check only says that something changed.

## Running it

```sh
cmake -S . -B build
cmake --build build -j
python3 bench/perf.py                    # every check this machine can run
python3 bench/perf.py --sections work    # one check
```

Sections: `scenarios`, `sizes`, `table`, `work`, `instructions`, `speed`,
`coverage`, `mutation`. `instructions` needs valgrind (Linux), `coverage`
needs `gcovr`, and `mutation` needs g++ or clang++ on a Unix-like system.
`mutation` takes a couple of minutes; the rest take about one.

## Accepting a change

Either of these, then commit `bench/baseline.json` with the change:

- Run `python3 bench/perf.py --update` (add `--sections ...` to limit it).
- Download the `baseline-candidate` artifact from the failed run and use its
  `baseline.json`. This is the better source for instruction counts, which
  belong to the CI machine's compiler (GCC 13 on Ubuntu 24.04).

If the baseline was recorded with a different compiler than the one running
the check, instruction counts are shown but cannot fail the run.

The tolerances live at the top of `baseline.json` under `limits`. Setting
`fail_on_unrecorded_improvement` to `false` lets an improvement pass without
a baseline update; the cost is that a later slowdown back to the old number
passes too.

## Adding to it

- **A benchmark level:** add a case to `all_cases()` in `bench.cpp`.
- **A mechanic:** add it to the lists in `all_scenarios()`.
- **A physics number:** define it in `include/gdpf/constants.hpp` and it is
  picked up by the pinning check. Numbers written inline in `src/sim.cpp` are
  not, apart from the pad and orb tables.

Each of these changes the measurements, so update the baseline with it.

## Files

| File | Purpose |
| --- | --- |
| `bench.cpp` | The benchmark program (`gdpf_bench`, and `gdpf_bench_stats` with work counters compiled in). |
| `perf.py` | Runs the measurements, compares with the baseline, writes the report. |
| `mutation.py` | The pinning check. Works on copies; never edits the repository. |
| `baseline.json` | The recorded numbers. |
