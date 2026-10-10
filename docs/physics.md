# Physics notes

What the simulator assumes, where each assumption came from, and what still
has to be checked against the game. Target version: Geometry Dash 2.2081.

**Nothing here has been verified in-game.** "Reference" below means a number
or behaviour documented by one of the projects under [Sources](#sources).

## Units and coordinates

- Positions are in level-string units. A grid block is 30. The top of the
  ground is `y = 0`, so a cube standing on the ground has its centre at
  `y = 15`. The game itself draws the ground higher; the offset between the two
  is one of the first things to measure.
- `y_vel` is in the game's own velocity units. One unit is 54 position units
  per second.
- One tick is 1/240 s. The game runs physics in fixed steps of `dt = 0.25`
  (in 60ths of a second) and uses `dt * 0.9 = 0.225` for vertical motion.

## One tick

1. Apply the input. A press sets `jump_held` and `fresh_click`; a release
   clears both.
2. Update `y_vel` for the current gamemode.
3. Move: `x += 0.25 * x_velocity * speed_factor`, `y += y_vel * 0.225`.
4. Collide, in this order: pads, orbs and portals; then solids; then hazards;
   then the floor and ceiling of the gamemode.
5. The level is finished once `x` passes the last object by 340.

Every write to `y_vel` rounds it to three decimals, as the game does. The one
exception is the ball's jump, which the game writes unrounded.

## Constants

| Constant | Value | Basis |
| --- | --- | --- |
| Steps per second | 240 | Decompiled game loop |
| Vertical step | 0.25 × 0.9 | Decompiled player update (2.1), consistent with reference numbers |
| Horizontal speed (units/s) | 251.16, 311.58, 387.42, 468.00, 576.00 | Reference, reproduced to 1e-6 |
| Cube/robot gravity per speed | 0.940199, 0.958199, 0.957199, 0.961199, 0.961199 | Inferred from reference thresholds |
| Cube jump per speed | 10.620032, 11.180032, 11.420032, 11.230032, 11.230032 | Reference |
| Gravity for ball, spider, flying modes | 0.9582 | Decompiled velocity update |
| Terminal velocity | 15 | Decompiled velocity update |
| Ball/spider gravity scale, robot gravity scale | 0.6, 0.9 | Decompiled velocity update |
| Flying velocity limits | +8 / −6.4 (mini +9.4118 / −7.5294) | Decompiled velocity update |
| UFO click | 7 (mini 8 × 0.85) | Decompiled velocity update |
| Swing gravity scale | 0.4 (mini 0.6), limits ±8 | Decompiled velocity update |
| Robot | jump × 0.5, hold cancels gravity until a counter reaches 1.5 | Decompiled velocity update |
| Gravity flip | keeps half the velocity | Reference |
| Player hitbox | 30 (mini × 0.6), wave 10, inner 9 | Reference |
| Landing clip | 10 (ship and UFO 7) | Reference |
| Corridor height | ship, UFO, wave 300; ball 240 | Reference |
| Corridor height | spider 270, swing 300 | **Guess** |
| Level end | last object + 340 | **Guess** |

### Numbers the tests reproduce

These fall out of the rules above without being hard-coded, and match the
references:

- Cube at 1x: launches at 11.18 and loses 0.216 per tick; a jump is about 2.2
  blocks high and 4.5 blocks long.
- Ship: thrust +0.108 per tick while slow, +0.086 once rising fast.
- UFO: 6.871 after a click (mini 6.648).
- Ball: 3.354 after a click.

## The "is falling" test

The game decides whether the player is falling by comparing `y_vel` with twice
the gravity constant rather than with zero, and it does not mirror the
comparison when gravity is flipped. The simulator copies this exactly because
it has visible effects: an upside-down cube keeps `on_ground` for about nine
ticks after leaving a ledge (so it can still jump), and the ship and UFO switch
between two acceleration values at that threshold.

## Simplified on purpose

Each of these is a known gap, not an oversight.

- **Solids** are axis-aligned boxes. Landing needs the player to be falling
  and within the clip distance of the top. Touching a solid with the inner
  hitbox kills. Ship, UFO, swing, ball and spider stop against the underside.
  The cube's horizontal snapping on staircases is missing.
- **Pads and orbs** set the velocity to a fraction of a base value, per
  gamemode (`kPadFactor`, `kOrbFactor` in `src/sim.cpp`). Fractions for robot,
  spider and swing are unknown, as are a few others; those stop the run. Mini
  uses a flat 0.8, which the references show is not always right.
- **Orbs** need a press that is still held and has not already been used by a
  jump or another orb. It can be made before reaching the orb. The reference
  simulator also accepts an orb touched on the previous tick.
- **Changing gamemode** halves the velocity. The real factor depends on both
  gamemodes.
- **Pads and portals ignore their rotation and flip**, and multi-activate is
  ignored.
- **Mirror portals** are ignored, which is correct for physics.
- **Start positions** are parsed but not used; runs start at the beginning.

## Not implemented

Slopes, the spider's teleport, dash orbs, spider and teleport orbs, spider
pads, teleport portals, dual mode, and solids rotated off the 90-degree grid.
Reaching any of them stops the run with a reason.

## Open questions for calibration

1. The vertical offset between level-string `y` and the in-game `y`.
2. Whether the level's first tick matches: initial position, and whether the
   player counts as on the ground before the first step.
3. Whether a pad or orb applies gravity on the tick it fires. The reference
   numbers suggest some do and some do not.
4. Exact overlap rules: strict or inclusive edges, and collision order.
5. The value the ship uses while boosted past its limits and not holding
   (marked in `update_flying`). Nothing sets that boosted state yet.
6. Where the level actually ends.

## Sources

- [camila314/gdp](https://github.com/camila314/gdp): decompiled
  `PlayerObject::updateJump` and `GJBaseGameLayer::update` for 2.2.
- [matcool/gd-decomps](https://github.com/matcool/gd-decomps): decompiled
  `PlayerObject::update` for 2.1.
- [camila314/pathfinder](https://github.com/camila314/pathfinder): its
  simulator's speeds, accelerations, pad and orb strengths, hitbox sizes and
  object IDs.
