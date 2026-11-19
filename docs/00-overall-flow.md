# 00 · Overall flow of the project

## What the program is

A C99 desktop app (Raylib for drawing, Raygui for widgets) that simulates a block
on an adjustable ramp with **static and kinetic friction**. The user sets the
angle, mass, gravity, initial velocity and friction coefficients. The program
then animates the block and draws the force vectors, a live readout and three
graphs (velocity, acceleration and friction against time).

## The window

```
+-----------+--------------------------------+-----------+
|           |  HUD card     scene            |           |
|   left    |  ramp, block, force vectors    |  graphs   |
|   panel   +--------------------------------+  column   |
|  (ui.c)   |   transport bar (ui.c)         | (graphs.c)|
+-----------+--------------------------------+-----------+
   320 px              580 px                    380 px
```

The window is 1280 × 800 *logical* pixels. On a Retina screen it is drawn at
2560 × 1600 real pixels (see [main](08-main-build-tests.md) and
[fonts](07-theme-fonts.md)), but all code works in logical pixels.

## The modules and how they depend on each other

```
                    types.h   (shared structs + constants, no code)
                       |
   +--------+----------+-----------+-----------+-----------+
   |        |          |           |           |           |
physics   sim        scene        ui        graphs     theme / fonts
 (1)      (2)         (3)         (4)         (5)       (shared look)
   ^        |          |           |           |
   |        |          +---- uses physics for forces and the critical angle
   +--------+  sim asks physics for every new sample
               ui calls Sim_* for the buttons, physics for the critical angle
```

| Module | Computes or draws | Talks to |
|---|---|---|
| **physics** | Pure mechanics. No Raylib, so it can be tested from a console. | nothing |
| **sim** | Owns *time* and the history of samples. | physics |
| **scene** | Ramp, block, arrows, labels, camera. **Read-only.** | physics (forces, critical angle) |
| **ui** | Every control. **Owns `Params` and `ViewOptions`.** | sim (buttons), physics (critical angle) |
| **graphs** | Graphs and the HUD card. **Read-only.** | nothing (gets data passed in) |
| **theme / fonts** | Colours, rounded shapes, text. | Raylib only |
| **main** | Creates the window and runs the frame loop. | all of the above |

**The ownership rule keeps this simple.** Only `ui.c` writes the parameters. Only
`sim.c` writes the history. Physics is a pure function. Scene and graphs only
read. So there is no shared mutable state to keep in sync.

## Start-up sequence (`main`)

```
1. Params + ViewOptions get their default values (30°, 5 kg, μs 0.40, μk 0.30 ...)
2. SetConfigFlags(MSAA 4x | HIGHDPI)     smooth edges, full Retina resolution
3. InitWindow(1280, 800)                 creates the OpenGL context
4. Fonts_Load()                          prepares the glyph list (atlases load lazily)
5. UI_Init()                             Raygui font + colour theme
6. Sim_Init(&params)                     allocates the 60 s history, sample 0 = initial state
7. frame loop (below) until the window is closed
8. Sim_Shutdown(), Fonts_Unload(), CloseWindow()
```

## One frame

Every frame (60 per second) runs the same fixed sequence:

```
  UI_HandleScene(view)            mouse wheel = zoom, drag = pan (scene area only)
        |
  Sim_Update(frameTime)           if RUNNING: tView += frame time (max 0.1 s)
        |
  cur = Sim_Current()             the block's state at tView  <-- ONE state for everything
        |
  BeginDrawing
  |  Scene_Draw(params, cur, view)        ramp, block, vectors, labels
  |  HUD_Draw(cur, mode, view)            status card over the scene
  |  Graphs_Draw(history, ..., cur)       right-hand column
  |  changed = UI_Draw(params, view)      left panel + transport bar (+ keyboard)
  |  if changed: Sim_SetParams(params)    restart the run with new physics
  EndDrawing
```

Two design points make this robust:

1. **One state per frame.** `Sim_Current()` is called once and the same `Sample`
   goes to the scene, the HUD and the graphs. The animation, the numbers and the
   graph cursor can never disagree.
2. **The UI is drawn last.** Raygui is an *immediate-mode* GUI: drawing a control
   and reading its input happen in the same call. Drawing it last puts the panels
   on top. A parameter change made this frame is applied with `Sim_SetParams`
   and shows from the next frame.

## How time works

Time is the master variable. The simulation does not "move the block"; it keeps a
cursor `tView` into a cached list of samples taken every 1/240 s:

```
History:  [ t=0 ][ t=1/240 ][ t=2/240 ] ... computed on demand, kept until params change
                         ^
                       tView (between samples -> one exact partial step from the sample before)
```

- **Running** moves `tView` forward by the frame time.
- **Step ±** and the **time box** set `tView` directly (and pause).
- **Going back in time** just reads earlier samples, so it gives exactly the same
  values as a fresh run to that time.
- `tReached` remembers the furthest time shown, so the graphs keep the whole
  curve when you step backwards.

See [Simulation control](03-sim.md) for details.

## Life of a slider change

```
user drags the Angle slider
  -> ui.c  SliderRow: GuiSliderBar updates params.angleDeg,
           then it is snapped to the shown precision (0.1°)
  -> UI_Draw compares Params before/after with memcmp -> returns true
  -> main  Sim_SetParams(&params)
  -> sim   throws the history away, sample 0 = Physics_Initial(new params),
           tView = tReached = 0 (a running sim keeps running)
  -> next frame: every module draws the new run from t = 0
```

The snapping step matters. The simulation must use exactly the number on screen.
Without it, a v0 of −0.04 m/s would show as "−0.0" but start the block moving.

Visual-only settings (`ViewOptions`: checkboxes, vector lengths, zoom) never call
`Sim_SetParams`, so changing them never restarts the run.

## Life of a button click

| Control | Path |
|---|---|
| Start / Pause / Resume, or Space | `ui.c` → `Sim_Start()` / `Sim_Pause()` → mode changes → `Sim_Update` advances or stops the clock |
| Reset, or R | `Sim_Reset()` → `tView = 0`, STOPPED (history kept: same params) |
| Step − / Step +, or ← / → | `Sim_StepBy(±step)` → `Sim_JumpTo` → clamp to [0, 60 s], pause |
| Time box + Enter | `Sim_JumpTo(typed time)` |
| Zoom ± / wheel / drag / Reset view | edits `view.zoom`, `view.panX/Y` → read by `Scene_Draw` next frame |

## Where the physics decision happens

There is exactly **one** place that decides "stuck or sliding": `evaluate()` in
`physics.c`. The stepping code, the HUD state pill, the friction label and the
graphs all use the flags it sets, so they can never contradict each other.

The rule it applies:

```
moving?                      -> kinetic friction μk·N, opposite to v
at rest and |mg sinθ| ≤ μs·N -> STICKS; static friction = exactly mg sinθ
at rest and |mg sinθ| > μs·N -> breaks away and slides down with kinetic friction
```

`|mg sinθ| ≤ μs·N` simplifies to `tan θ ≤ μs`, which gives the
**critical angle θc = atan(μs)** shown in the left panel.

## File map

```
main.c                 frame loop
types.h                shared contract
physics.c / .h         (1) mechanics
sim.c / .h             (2) clock + history
scene.c / .h           (3) scene drawing
ui.c / .h              (4) controls  (the only file with RAYGUI_IMPLEMENTATION)
graphs.c / .h          (5) graphs + HUD
theme.c / .h           palette + rounded shapes
fonts.c / .h           text drawing
font_data.h            embedded Inter font (generated)
raygui.h               third-party, single header
tests/                 console tests + screenshot smoke test
tools/                 font_data.h generator
```
