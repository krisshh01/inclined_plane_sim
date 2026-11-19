# Plan: Block on an Inclined Plane (Raylib + Raygui, C)

A C99 desktop simulation of a block on an adjustable ramp with real static and kinetic friction,
recreating the oPhysics friction simulation. This document is the project plan: what it must do,
how it is structured, how the physics works, how to build and test it, and what is left.

## 1. Goals

1. Draw the ramp, block, and force vectors (gravity, normal, friction), with an optional
   parallel/perpendicular breakdown of gravity.
2. Use real Newtonian mechanics, with the static-to-kinetic transition handled correctly:
   the block must stick when static friction holds, and accelerate properly when sliding up or down.
3. Provide a Raygui control panel for the parameters, the display options and the time controls.
4. Show live readouts (state, time, displacement, velocity, acceleration, friction) and graphs
   of v, a and friction against time.

## 2. Requirements checklist

**Controls and interface**
- [x] Start, Pause (Resume) and Reset buttons
- [x] Zoom in / zoom out buttons (plus mouse-wheel zoom, drag-to-pan, Reset view)
- [x] Sliders: angle 0-90 deg, mass, gravity, initial velocity, mu_s, mu_k
- [x] Vector-length sliders for the gravity, normal and friction arrows
- [x] Checkboxes: parallel/perpendicular components, x/y components of every vector, show time
- [x] Checkboxes for the v-t, a-t and friction-t graphs
- [x] Step up / step down buttons (0.01, 0.1 or 1.0 s) and a time input box

**Graphics and graphs**
- [x] Live signed values of v, a, t, displacement and friction force
- [x] The three graphs, with a cursor at the current time
- [x] Animation of the block sliding

## 3. Architecture

Five modules plus a thin main loop. One module computes, one controls time, and three draw or take input.
Modules communicate only through `types.h` and each module's own header.

| Module | Files | Responsibility |
|---|---|---|
| 1. Physics engine | `physics.c/h` | Pure mechanics. No Raylib, so it is testable from a console. |
| 2. Simulation control | `sim.c/h` | Clock, Start/Pause/Reset, step and jump, cached history. |
| 3. Scene renderer | `scene.c/h` | Ramp, block, arrows, labels, camera (zoom and pan). |
| 4. Control panel | `ui.c/h` | Every Raygui widget, keyboard shortcuts, wheel and drag input. |
| 5. Graphs and HUD | `graphs.c/h` | The three graphs and the live text readout. |
| Shared | `types.h` | Structs, constants and ownership rules. |
| Shared | `fonts.c/h`, `font_data.h` | Embedded Inter font, one crisp glyph atlas per size. |
| Entry point | `main.c` | Frame loop of about 40 lines. |

**Ownership rules**
- Physics computes `Sample` values from `Params`.
- Sim control owns the `History` and the view time.
- The UI owns `Params` and `ViewOptions` and writes them in place.
- Scene and graphs only read.

**Frame loop**
```
UI_HandleScene -> Sim_Update(frameDt) -> Sim_Current()
  -> Scene_Draw -> HUD_Draw -> Graphs_Draw -> UI_Draw
  -> if UI_Draw reports a parameter change: Sim_SetParams()
```

## 4. Physics model (module 1)

One axis along the slope, **up-slope positive**. This sign convention is used everywhere, including the HUD.

- Gravity along the slope: `Fpar = -m g sin(theta)`
- Normal force: `N = m g cos(theta)` (zero at 90 degrees)
- Maximum static friction: `fs_max = mu_s N`
- Kinetic friction: `fk = mu_k N`, with mu_k clamped so it never exceeds mu_s

**State logic**
1. **Moving** (`|v| > 1e-6`): friction opposes the velocity, so `a = (Fpar - sign(v) fk) / m`.
2. **At rest and `|Fpar| <= fs_max`:** the block **sticks**. `a = 0`, and static friction `= -Fpar`.
   This is the force it has to cancel, not `mu_s N`.
3. **At rest and `|Fpar| > fs_max`:** the block breaks away down the slope with kinetic friction.
4. **Initial velocity:** if `v0 != 0` the block is sliding from the first instant.

**The stop transition.** When a deceleration would flip the sign of `v` inside a step:
- split the step at `t_stop = -v / a`;
- set `v = 0` exactly;
- re-evaluate the remaining time as "at rest", so the block either sticks at the apex (`tan(theta) <= mu_s`)
  or slides back down.

**Exact kinematics.** Acceleration is piecewise constant, so each step uses `s += v dt + a dt^2 / 2`.
There is no Euler error, so a single 1 s step equals 240 steps of 1/240 s, including across a reversal.

**Ramp ends.** The ramp is finite (see section 9). The block stops dead at either end with no bounce,
and is reported as "At Rest, pinned against the ramp end".

**Edge cases:** 90 degrees gives free fall with `a = -g` and zero friction. At 0 degrees the block
stays put with `v0 = 0`, or decelerates at `mu_k g` otherwise.

## 5. Time and history (module 2)

Time is the master variable. The view time `tView` is a cursor into a history of fixed-step samples.

- The history is computed lazily at 1/240 s per sample, up to `T_MAX = 60 s` (about 14,400 samples).
- `Sim_Current()` returns the state at `tView`. Any time between two samples is computed with one exact
  partial step, so stepping and jumping are precise.
- Because the physics is deterministic, stepping back to a time gives exactly what a fresh run to that time gives.
- Modes: `STOPPED`, `RUNNING`, `PAUSED`.
  - Start moves STOPPED or PAUSED to RUNNING.
  - Pause moves RUNNING to PAUSED.
  - Reset returns to STOPPED at t = 0.
  - Stepping or jumping pauses.
  - At `T_MAX` the clock pauses; pressing Start again restarts from 0.
- A single frame can advance the clock by at most 0.1 s, so a stalled frame cannot jump time.
- Changing a physics slider discards the history and restarts at t = 0.
  A running simulation keeps running; otherwise it returns to STOPPED.
- The graphs keep the full curve up to the furthest time reached, so stepping back does not erase it.

## 6. Rendering (module 3)

- Screen layout at 1280 x 800: left panel 300 px, scene in the middle, graph column 400 px on the right.
- **Auto-fit:** at zoom 1 the pixels-per-metre scale is chosen so the whole ramp fits the scene at any angle.
  The ramp and block therefore resize slightly as the angle changes. Zoom (0.3 to 4) and pan work on top of this.
- The scene is drawn inside a `Camera2D`. Labels are queued and drawn in screen space afterwards, so text
  does not scale with zoom and stays inside the scene area.
- **Arrows**
  - Drawn from the block's centre.
  - Gravity points straight down.
  - Normal points perpendicular to the surface.
  - Friction points along the slope.
  - Lengths scale with force times the vector-length slider, and are clamped so the weight arrow stays
    between 36 and 260 px.
  - Each arrow carries a labelled magnitude, and friction is labelled static or kinetic.
- **Components**
  - Parallel/perpendicular: `mg sin(theta)` and `mg cos(theta)` as dashed arrows.
  - x/y: every vector's screen-axis components as dashed arrows with labels.
- Raylib culls clockwise triangles, so every triangle is drawn with both windings.
- Text uses the embedded Inter font (Regular and SemiBold subsets), with supported characters
  ASCII plus the degree sign, superscript 2, middle dot, multiplication sign, theta, mu and minus.

## 7. Control panel (module 4)

| Control | Range / behavior |
|---|---|
| Angle | 0 to 90 degrees |
| Mass | 0.1 to 20 kg |
| Gravity | 0.5 to 25 m/s^2 |
| v0 | -8 to +8 m/s (positive is up the slope) |
| mu_s, mu_k | 0 to 2 each; mu_k is clamped to mu_s |
| Vector length (gravity, normal, friction) | x0.25 to x3; drawing only, never touches the physics |
| Step size | 0.01 / 0.1 / 1.0 s |
| Time box | Click, type a time, press Enter to jump; clamped to [0, 60] s |
| Zoom - / + | x1.2 per click; the mouse wheel is x1.1 per notch |

Keyboard: Space is start/pause, R is reset, Left and Right step time (disabled while typing in the time box).
`RAYGUI_IMPLEMENTATION` is defined in `ui.c` only.

## 8. Graphs and HUD (module 5)

- One generic `DrawGraph` is reused for v(t), a(t) and friction(t).
- Auto-scaled y axis (always including 0, with padding), "nice" tick steps, grid, tick labels,
  a title with units, and a cursor dot at the current time.
- **Step-style curves:** a(t) and friction(t) jump at stick/slide transitions, so each curve holds a value
  and then jumps vertically.
- Downsampled to about one point per pixel.
- Enabled graphs share the right column evenly (each at most 300 px tall).
- HUD: state (At Rest / Sliding) with the reason (static friction holds, kinetic friction, or pinned at the ramp end),
  the mode, and t, s, v, a and friction as signed values with units.

## 9. Constants (`types.h`)

| Constant | Value | Meaning |
|---|---|---|
| `RAMP_LENGTH` | 20 m | Ramp length along the slope |
| `BLOCK_SIZE` | 1.0 m | Side of the square block |
| `START_POS` | 10 m | Block start, measured along the slope (midpoint) |
| `PHYS_DT` | 1/240 s | Fixed physics step |
| `T_MAX` | 60 s | Clock limit |

Default parameters at launch: angle 30 degrees, mass 5 kg, g = 9.81, mu_s = 0.40, mu_k = 0.30, v0 = 0.

## 10. Build and run

Requires Raylib 5.0+ and raygui 4.0+ (`raygui.h` placed in the project folder).

| Platform | Steps |
|---|---|
| Linux | `make`, then `./incline_sim` (needs Raylib's dev package) |
| macOS | `brew install raylib`; `make` (Intel) or `make INC="-I/opt/homebrew/include" LIB="-L/opt/homebrew/lib"` (Apple Silicon) |
| Windows | Install Raylib's installer (MinGW `w64devkit`), then in its terminal `make INC="-IC:/raylib/raylib/src" LIB="-LC:/raylib/raylib/src"`; or MSYS2 with `pacman -S make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-raylib` and plain `make` |

`make test` runs the console tests and needs no Raylib. Visual Studio / MSVC is not set up.

## 11. Testing

- **Physics (38 checks):** sticks when tan(theta) < mu_s; the boundary at atan(mu_s); sliding acceleration
  `g(sin - mu_k cos)`; up-slope deceleration; sticking at the apex; reversal; stopping while sliding down;
  90 and 0 degree cases; mu_k clamping; ramp ends; one big step equals many small steps (also across a reversal);
  `m a` equals the net force in every state.
- **Simulation control (23 checks):** initial state; exact partial steps; stepping back equals a fresh run;
  clamping; running and auto-pause; stalled-frame cap; parameter changes reset the history.
- **Visual smoke test:** `tests/screenshot.c` renders full UI frames to PNG (sliding, up then down with x/y components,
  stick at the apex then jump back in time, static and zoomed, vertical, flat, and pinned at the wall).
  Used with a headless Raylib build, it has also been run under AddressSanitizer and UBSan with no findings.
- **Not automated:** real mouse and keyboard interaction (button clicks, typing in the time box, wheel zoom, drag-to-pan).
  These need a hands-on pass on the target machine.

## 12. Assumptions and known limitations

- Moving a physics slider restarts from t = 0 rather than editing the run in place.
- The block does not bounce at the ramp ends.
- The clock stops at 60 s.
- In x/y component mode, with a steep ramp and the block pinned at the bottom, a couple of force labels can overlap.
- Raylib is run in its standard (non-high-DPI) mode, so text can look soft on Retina and other scaled displays.
- Mac and Windows builds have not been tested on real machines; Linux was checked with a headless Raylib build.
- Digits in the HUD use proportional widths, so the right edge of a number can shift slightly as values change.

## 13. Possible next steps

- High-DPI rendering for sharp text on Retina and scaled Windows displays.
- Bounce or restitution at the ramp ends, or a wall that stops the block.
- An applied force or a pulley and hanging mass.
- Export the history to CSV.
- Optionally, keep the sliders live without resetting the run.
- Save and load parameter presets.


## Update: single transport button, critical angle, new look

- **One Start/Pause/Resume button** driven by `Sim_Mode()` (label and colour follow the state); Reset is separate.
- **Critical angle** `Physics_CriticalAngle(mu) = atan(mu)` in degrees. Shown in a left-panel card (live with the friction sliders), as a dashed amber ray in the scene, and as a notch on the Angle slider. Test: just below θc the block sticks, just above it slides, for eight values of μs.
- **Visual redesign**: shared palette and card helpers in `theme.c`; all Raygui controls themed in `ui.c`; graphs and HUD restyled; 4x MSAA requested at start-up.
- **Code comments**: every module carries a header explaining its role, coordinate conventions and drawing order.
