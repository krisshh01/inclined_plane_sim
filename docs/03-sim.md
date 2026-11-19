# 03 · Simulation control (`sim.c`, `sim.h`)

**Role:** owns **time**. It never does physics itself. It asks `physics.c` for
samples and stores them. Everything else reads the current state through
`Sim_Current()`.

## State kept in the module (all `static`)

| Variable | Meaning |
|---|---|
| `P` | the `Params` of the current run |
| `H` | the `History`: samples at `t = 0, dt, 2dt, …` (`dt = 1/240 s`) |
| `mode` | `SIM_STOPPED`, `SIM_RUNNING` or `SIM_PAUSED` |
| `tView` | the time being shown (the cursor) |
| `tReached` | the furthest time shown since the last reset (sets the graphs' x-axis) |

## The central idea: a cursor into a cached history

```
H.data:  [0] [1] [2] [3] ... [count-1]          computed so far
          t=0      i·PHYS_DT

tView ──────────────┐
                    v
         sample i = floor(tView / PHYS_DT)  +  one exact partial step of (tView − i·dt)
```

The physics is deterministic and its steps are exact, so:

- going **back** in time just reads an earlier sample, and gives exactly the
  same values a fresh run to that time would give;
- the frame rate does not affect the result: 30 fps or 144 fps show the same
  block at the same time;
- the animation, HUD and graphs all read from one place, so they always agree.

### Lazy computation

`Hist_Ensure(idx)` computes any missing samples up to `idx`, each one a
`Physics_Step` of `PHYS_DT` from the previous one. Each sample's time is set to
exactly `count · PHYS_DT` (multiplied, not accumulated), so there's no
floating-point drift. The history is allocated once for the full 60 s
(`Sim_Init`).

`IndexOf(t)` adds a tiny epsilon (`1e-9`) before `floor`, so a time like exactly
`1.0` doesn't fall into the previous slot because of rounding.

## Public functions

| Function | What it does |
|---|---|
| `Sim_Init(p)` | allocate the history, sample 0 = `Physics_Initial`, STOPPED at t = 0 |
| `Sim_Shutdown()` | free the history |
| `Sim_SetParams(p)` | new physics: discard the history, restart at t = 0. **A running sim keeps running**; otherwise it goes to STOPPED |
| `Sim_Start()` | → RUNNING. If the clock already hit `T_MAX`, starts a fresh run from 0 |
| `Sim_Pause()` | RUNNING → PAUSED |
| `Sim_Reset()` | t = 0, STOPPED. The history is kept because the params didn't change |
| `Sim_Update(frameDt)` | if RUNNING: `tView += frameDt`, capped at 0.1 s per frame (a stalled frame, such as a window drag, can't jump far). Auto-pauses at `T_MAX` |
| `Sim_StepBy(dt)` | `Sim_JumpTo(tView + dt)` |
| `Sim_JumpTo(t)` | clamp to [0, T_MAX], set the cursor, **pause** (a jump to exactly 0 from STOPPED stays STOPPED) |
| `Sim_Current()` | the `Sample` at `tView` (see above); `cur.t` is set to exactly `tView` |
| `Sim_Time()`, `Sim_ReachedTime()`, `Sim_Mode()` | getters |
| `Sim_History()` | read-only pointer for the graphs |
| `Sim_PlotCount()` | number of samples from 0 to `tReached`, making sure they exist |

## State diagram

```
             Start / Space
  STOPPED ─────────────────────> RUNNING
     ^  ^                         │   ^
     │  │ Reset / R               │   │ Start / Space (Resume)
     │  └─────────────────────────┤   │
     │                     Pause, │   │
     │          Step, Jump,       v   │
     │          or t hits 60 s   PAUSED
     │                            │
     └──── param change ──────────┘   (a param change while RUNNING restarts at 0 and keeps RUNNING)
```

## Why `tReached` exists

If you run to 3 s and then step back to 1 s, `tView = 1` but `tReached = 3`. The
graphs plot `0 … tReached` and draw the cursor at `tView`, so stepping back
moves the cursor along the curve instead of erasing it.

## Tested behaviour (`tests/test_sim.c`)

- starts STOPPED at t = 0 with v = v0
- `JumpTo(1.2345)` matches exact kinematics; jumping pauses
- jumping back gives bit-identical values to a fresh run
- `tReached` and the plot count remember the furthest point
- clamping to [0, T_MAX]; auto-pause at T_MAX; Start after the end restarts
- a stalled frame advances at most 0.1 s
- a param change resets time and history, keeps RUNNING, sends PAUSED to STOPPED
