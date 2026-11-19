# 02 · Physics engine (`physics.c`, `physics.h`)

**Role:** pure Newtonian mechanics for a block on an incline. No Raylib or Raygui,
so it compiles and runs from a console (`tests/test_physics.c`). Everything is in
double precision.

## Public functions

| Function | Returns |
|---|---|
| `Physics_Initial(p)` | the `Sample` at t = 0: `s = 0`, `v = v0`, plus the state flags |
| `Physics_Step(prev, p, dt)` | `prev` advanced by `dt` seconds. Any `dt` works: one big step equals many small ones |
| `Physics_Forces(sample, p)` | a `Forces` breakdown (weight, mg sinθ, mg cosθ, normal, friction, net) for drawing |
| `Physics_CriticalAngle(mu)` | `atan(mu)` in degrees |
| `Physics_MinS()`, `Physics_MaxS()` | the furthest the block centre can travel down / up from the start mark |

## The model

One axis along the slope, up-slope positive:

```
Fpar  = −m g sin θ      gravity along the slope (always ≤ 0: it pulls down)
N     =  m g cos θ      normal force (forced to exactly 0 at 90°)
fsMax =  μs N           the most static friction the surface can supply
fk    =  μk N           kinetic friction magnitude (μk is clamped to ≤ μs)
```

`derive()` turns `Params` into these numbers once per call (the `Derived`
struct). The 90° case is special-cased because `cos(90°)` in floating point is
about `6e-17`, not 0, and friction must vanish exactly.

## `evaluate()`: the single decision point

This one function decides **stuck or sliding** and fills in `a`, `fric` and the
flags for a given `(s, v)`. Every other part of the program uses its result.

```
Case 1  |v| > 1e-6  (moving)
        fric = −sign(v) · fk                 kinetic friction opposes motion
        a    = (Fpar + fric) / m

Case 2  at rest, |Fpar| ≤ fsMax
        STICKS: a = 0, fric = −Fpar          static friction is exactly what is
                                             needed, NOT μs·N

Case 3  at rest, |Fpar| > fsMax
        if pressed against the ramp end in the direction gravity pulls:
            pinned (atWall), a = 0
        else
            breaks away: fric = −sign(Fpar) · fk, a = (Fpar + fric) / m
            (v is 0 at this exact instant but the block counts as sliding)
```

Case 2 holds when `tan θ ≤ μs`, which is where the **critical angle
θc = atan(μs)** comes from.

> **Why a moving block can keep sliding below θc.** Static friction (Case 2)
> only applies when the block is **at rest**. A moving block feels only kinetic
> friction. If `θk < θ < θc` (where θk = atan(μk)), kinetic friction is weaker
> than gravity's pull, so a block already sliding down keeps accelerating, even
> though one released from rest would stick. This is real physics, not a bug.

## `Physics_Step()`: exact stepping

While nothing changes state, acceleration is constant, so the step uses the
exact kinematic formulas rather than Euler integration:

```
s += v·dt + ½·a·dt²
v += a·dt
```

That means **no numerical drift and no jitter** at the stick/slide threshold.

A step can contain several phases (for example decelerate, stop, slide back),
so it loops over the remaining time `rem` (at most 4 phases):

```
loop while rem > 0:
    if v == 0:
        sticks?           -> stop (nothing moves)
        pinned at an end? -> stop
        else break away with constant a for all of rem
    else (moving):
        a  = (Fpar − sign(v)·fk) / m
        vn = v + a·rem
        if vn and v have opposite signs:          <- the block would stop inside this step
            ts  = −v / a                          <- exact time it reaches v = 0
            advance exactly ts, set v = 0
            rem −= ts, and loop again: re-evaluate "at rest" (stick, or slide back)
        else:
            advance the whole rem
    clamp to the ramp ends (stop dead, no bounce)
```

### The stop transition

This is the hard part. If velocity were simply integrated, `v` would overshoot
zero and flip sign, and the block would wobble or creep. Splitting the step at
`t_stop = −v/a` makes `v` land on **exactly 0**. The rest of the step is then
decided by the at-rest rules. If static friction can hold it, the block stays at
the apex. Otherwise it immediately slides back down with kinetic friction.

### Ramp ends

`Physics_MinS()` = `BLOCK_SIZE/2 − START_POS` and `Physics_MaxS()` =
`RAMP_LENGTH − BLOCK_SIZE/2 − START_POS`. These are the positions where the
block's edge touches an end. Reaching one sets `v = 0`, and `evaluate` reports
`atWall`.

## `Physics_Forces()`

For drawing the arrows and the components:

| Field | Value |
|---|---|
| `weight` | m g |
| `gPar` | m g sin θ (magnitude, down the slope) |
| `gPerp` | m g cos θ (into the surface) |
| `normal` | N |
| `friction` | the sample's signed `fric` |
| `net` | Fpar + fric (equals m·a when not at a wall) |

## Tested behaviour (`tests/test_physics.c`)

- sticks below θc, with static friction = m g sin θ exactly
- sticks just below atan(μs), slides just above
- kinetic acceleration, v(t) and s(t) match closed-form answers
- thrown up the slope: stops at the correct apex and stays there (no drift), or
  reverses with the correct new acceleration
- 90°: a = −g, no friction; 0°: correct flat stopping distance
- μk is clamped to μs
- pinned at the bottom end; never leaves the top end
