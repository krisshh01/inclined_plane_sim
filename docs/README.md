# Documentation: Block on an Inclined Plane

These documents explain how the simulator's code works. Start with the overall
flow, then read whichever module you need.

| # | Document | Covers |
|---|---|---|
| 00 | [Overall flow](00-overall-flow.md) | Architecture, start-up, the frame loop, how data moves between modules, the life of a slider change and a button click |
| 01 | [Shared types](01-types.md) | `types.h`: layout constants, `Params`, `Sample`, `History`, `ViewOptions`, `SimMode`, ownership rules |
| 02 | [Physics engine](02-physics.md) | `physics.c/h`: forces, stick / slide logic, exact stepping, the stop transition, ramp ends |
| 03 | [Simulation control](03-sim.md) | `sim.c/h`: the clock, Start / Pause / Reset, stepping and jumping, the cached history |
| 04 | [Scene renderer](04-scene.md) | `scene.c/h`: camera and scale, ramp, ruler, block, force vectors, labels |
| 05 | [Control panel](05-ui.md) | `ui.c/h`: Raygui widgets, custom rounded controls, slider snapping, keyboard and mouse |
| 06 | [Graphs and HUD](06-graphs-hud.md) | `graphs.c/h`: the v-t, a-t and f-t graphs, and the status card |
| 07 | [Theme and fonts](07-theme-fonts.md) | `theme.c/h`, `fonts.c/h`, `font_data.h`: palette, drawing helpers, crisp text on Retina screens |
| 08 | [Entry point, build and tests](08-main-build-tests.md) | `main.c`, the `Makefile`, `tests/`, `tools/` |

## Conventions used in every document

- **Up the slope is positive.** Displacement `s`, velocity `v`, acceleration `a`
  and friction are all signed along the slope. A block sliding down has negative
  `s`, `v` and `a`, and the friction holding it back is positive.
- **`s` is measured from the start mark** (10 m along the 20 m ramp), not from
  the bottom of the ramp.
- Functions are referred to by name (for example `Physics_Step`) rather than by
  line number, so the documents stay correct as the code changes.
