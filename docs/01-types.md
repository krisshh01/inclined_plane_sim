# 01 · Shared types (`types.h`)

`types.h` is the **contract** between the modules. It has no code, only constants
and structs. The modules share nothing else, so reading this file tells you what
data flows through the program.

## Layout constants (logical pixels)

| Constant | Value | Meaning |
|---|---|---|
| `SCREEN_W`, `SCREEN_H` | 1280, 800 | window size |
| `LEFT_W` | 320 | left control panel |
| `RIGHT_W` | 380 | graph column |
| `BAR_H` | 110 | transport bar under the scene |
| `SCENE_X`, `SCENE_W`, `SCENE_H` | derived | the scene rectangle; it **excludes** the transport bar, so wheel-zoom and drag-pan never fire from the bar |

## World and simulation constants

| Constant | Value | Meaning |
|---|---|---|
| `RAMP_LENGTH` | 20 m | measured along the slope |
| `BLOCK_SIZE` | 1 m | side of the square block |
| `START_POS` | 10 m | where the block starts (the `s = 0` mark) |
| `PHYS_DT` | 1/240 s | spacing of the cached history samples |
| `T_MAX` | 60 s | the clock stops here |

## `Params`: what the user sets (physics inputs)

```c
typedef struct {
    float angleDeg;   /* 0..90 */
    float mass;       /* kg */
    float g;          /* m/s² */
    float mu_s;       /* static coefficient */
    float mu_k;       /* kinetic coefficient (kept <= mu_s) */
    float v0;         /* initial velocity, + up the slope */
} Params;
```

Written **only** by `ui.c`. Any change restarts the run (`Sim_SetParams`).

## `Sample`: the complete state at one instant

```c
typedef struct {
    double t, s, v, a;   /* time, displacement, velocity, acceleration */
    double fric;         /* signed friction force (N) */
    bool   sliding;      /* moving, or about to break away */
    bool   staticFric;   /* fric is static (true) or kinetic (false) */
    bool   atWall;       /* pinned against an end of the ramp */
} Sample;
```

Produced by `physics.c`, stored by `sim.c`, read by everyone else. All values use
double precision so long runs don't drift.

The flags are what the UI shows:

| `sliding` | `staticFric` | `atWall` | HUD shows |
|---|---|---|---|
| true | false | false | **SLIDING**: "Kinetic friction acts" |
| false | true | false | **AT REST**: "Static friction holds the block" |
| false | true | true | **AT REST**: "Pinned against the end of the ramp" |

## `History`: every sample of the current run

```c
typedef struct { Sample *data; int count; int capacity; } History;
```

Owned by `sim.c`. `data[i]` is the state at `t = i · PHYS_DT`. Capacity covers
the whole 60 s run (14 402 samples). Samples are computed lazily, only as far as
they are needed.

## `ViewOptions`: purely visual settings

Checkboxes (`showParallelPerp`, `showXY`, `showCritical`, `showTime`,
`showGraphV/A/F`), arrow scale multipliers (`scaleGravity/Normal/Friction`) and
the camera (`zoom`, `panX`, `panY`).

Written by `ui.c`, read by `scene.c` and `graphs.c`. **Changing these never
touches the physics**, so they never restart the run.

## `SimMode`

`SIM_STOPPED` (at t = 0, never started) → `SIM_RUNNING` → `SIM_PAUSED`.

## Ownership rules (from the header comment)

| Data | Written by | Read by |
|---|---|---|
| `Params` | ui.c | everyone |
| `ViewOptions` | ui.c | scene.c, graphs.c |
| `History` | sim.c | graphs.c (read-only pointer) |
| `Sample` | physics.c | stored and served by sim.c |

## Sign convention

**Up the slope is positive, everywhere.** A block sliding down has negative `s`,
`v` and `a`, and the friction resisting it is positive.
