# Block on an Inclined Plane (Raylib + Raygui, C99)

Interactive friction simulation: a block on an adjustable ramp with real
static/kinetic friction, force vectors, live readouts and graphs.

## Build

Needs **Raylib 5.0+** and **raygui 4.0+** (`raygui.h` must be on the include path).

```
make                       # Linux / macOS / Windows (MinGW, w64devkit)
make INC="-I/path/raylib/src -I/path/raygui/src" LIB="-L/path/raylib/src"   # custom paths
make test                  # console tests for physics + sim control (no Raylib needed)
```

Manual Linux line:
`gcc -std=c99 -O2 main.c physics.c sim.c scene.c ui.c graphs.c fonts.c theme.c -o incline_sim -lraylib -lGL -lm -lpthread -ldl -lrt -lX11`

## Using it

| Control | What it does |
|---|---|
| Start / Pause / Resume (ONE button) | Green "Start" when stopped, amber "Pause" while running, green "Resume" when paused (Space does the same) |
| Reset | Return to t = 0 (R) |
| Critical angle | Card in the left panel shows θc = atan(μs) (and θk = atan(μk)) and updates live with the friction sliders; an amber ray in the scene and an amber notch on the Angle slider mark it. The chip says whether the block slides (θ > θc) or sticks (θ < θc). Toggle the ray with "Critical angle line". |
| Zoom - / Zoom + / Reset view | Scale the scene (mouse wheel also zooms, drag the scene to pan) |
| Step down / Step up | Move time by the chosen step (0.01 / 0.1 / 1.0 s); Left/Right arrows too |
| time (s) box | Click, type a time, press Enter to jump there |
| Angle, Mass, Gravity, v0, mu_s, mu_k | Physics parameters (changing one restarts from t = 0). Drag the slider, or click the value next to it, type a number and press Enter (Esc cancels) |
| Vector length sliders | Scale the gravity / normal / friction arrows (drawing only) |
| Checkboxes | Gravity parallel/perpendicular components, x/y components of every vector, time readout, and the v-t, a-t, friction-t graphs |

**Sign convention everywhere:** positive = up the slope. `s` is measured from the start position,
so a block sliding down has negative `s`, `v`, and `a`, and friction is positive (it points up-slope).

## Physics

With `Fpar = -m g sin(th)`, `N = m g cos(th)`:

- **Moving:** kinetic friction `mu_k N` opposes the velocity.
- **At rest:** if `|Fpar| <= mu_s N` the block **sticks** and static friction equals `-Fpar` (not `mu_s N`);
  otherwise it breaks away and slides down.
- **Stopping:** when a deceleration would flip the sign of `v`, the step is split at `t_stop = -v/a`,
  `v` becomes exactly 0, and the remaining time is re-evaluated (stick at the apex, or slide back).
- Acceleration is piecewise constant, so steps use exact kinematics (no Euler jitter at the threshold).
- At 90 degrees `N = 0` (free fall); `mu_k` is clamped to `<= mu_s`.

## Modules

Detailed code documentation for each module and the overall flow is in [`docs/`](docs/README.md).


| File | Role |
|---|---|
| `types.h` | Shared structs, constants, and the ownership rules |
| `physics.c/h` | Module 1: pure mechanics (no Raylib) |
| `sim.c/h` | Module 2: clock, Start/Pause/Reset, step/jump, cached history |
| `scene.c/h` | Module 3: ramp, block, arrows, labels, camera zoom/pan |
| `ui.c/h` | Module 4: every Raygui widget, keyboard shortcuts, wheel/drag |
| `graphs.c/h` | Module 5: v-t / a-t / f-t graphs and the HUD text |
| `theme.c/h` | Shared dark colour palette and rounded-card / triangle drawing helpers |
| `fonts.c/h`, `font_data.h` | Shared text rendering: embedded Inter font, one crisp atlas per size |
| `main.c` | Thin frame loop |

`tests/` holds the console tests and `screenshot.c`, a visual smoke test that renders the full UI to PNGs.

## Font

All text (Raygui controls, labels, graphs, HUD) uses **Inter**, a typeface designed for screens, embedded in
`font_data.h` as small subsets of Regular and SemiBold (about 11 KB each), so there are no font files to ship.
Inter is licensed under the SIL Open Font License 1.1 (`LICENSE-Inter-OFL.txt`). To change the font or the
characters available, edit `tools/make_font_data.py` (needs `pip install fonttools` and the Inter `.otf` files)
and keep the character list in `fonts.c` in sync. Supported characters: ASCII plus ° ² · × θ μ − ≤ ≥ ≈.

## Assumptions you can change (all in `types.h`)

- Ramp length 20 m, block 1.0 m, start at the 10 m mark; the block stops dead at either end (no bounce)
  and is shown as "At Rest" there.
- At zoom 1 the scene scale is chosen so the whole ramp fits the view at every angle, so the ramp (and block)
  grow or shrink a little as you move the angle slider. Zoom and pan work on top of that; Reset view returns to it.
- The clock runs to 60 s (`T_MAX`) and then pauses; physics steps at 1/240 s.
- Moving a physics slider restarts at t = 0 (a running simulation keeps running).
- Arrow length is `2 px per newton x slider`, limited so the weight arrow stays between 36 and 240 px.
