# Trace format

A trace records the player's state once per physics tick. The simulator writes
one (`gdpf sim --trace`), and the planned in-game logger will write the same
format, so the two can be compared line by line to find the first tick where
they differ.

Plain CSV with a header row:

```
tick,x,y,y_vel,mode,speed,mini,flipped,on_ground,hold,dead
0,0.000000,15.000000,0.000000,0,1,0,0,1,0,0
1,1.298250,15.000000,0.000000,0,1,0,0,1,0,0
```

| Column | Meaning |
| --- | --- |
| `tick` | Row 0 is the starting state. Row `n` is the state after `n` physics steps (240 per second). |
| `x`, `y` | Centre of the player, in level-string units with the top of the ground at `y = 0`. |
| `y_vel` | Vertical velocity in game units (× 54 = units per second). |
| `mode` | 0 cube, 1 ship, 2 ball, 3 UFO, 4 wave, 5 robot, 6 spider, 7 swing. |
| `speed` | 0 = 0.5x, 1 = 1x, 2 = 2x, 3 = 3x, 4 = 4x. |
| `mini` | 1 if mini. |
| `flipped` | 1 if gravity is upside down. |
| `on_ground` | 1 if the player is resting on a surface. |
| `hold` | 1 if the button was down during the step that produced this row. |
| `dead` | 1 on the row where the player died. No rows follow it. |

## For the in-game logger

- Log after each physics step, not once per rendered frame.
- Convert `y` to level-string units by subtracting the game's ground offset.
  That offset still has to be measured; until then, log the raw value and note
  it at the top of the file as a `#` comment line.
- `y_vel` for the wave is written by the simulator as the effective diagonal
  velocity. The game stores something else there, so skip that column when
  comparing wave sections.

## Input scripts

The matching input format, read by `gdpf sim --inputs`:

```
# <tick> <0|1>: from this tick's step onward the button is up (0) or down (1)
179 1
180 0
```
