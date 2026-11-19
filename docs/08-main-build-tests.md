# 08 · Entry point, build and tests

## `main.c`: the frame loop

`main.c` is deliberately thin (about 65 lines). It wires the modules together and
contains no logic of its own.

### Default values

```c
Params      params = { angle 30°, mass 5 kg, g 9.81, μs 0.40, μk 0.30, v0 0 };
ViewOptions view   = { gravity components ON, x/y OFF, critical line ON, time ON,
                       all three graphs ON, vector scales 1, zoom 1 };
```

To change what the app opens with, edit these two initialisers.

### Window flags

```c
SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
```

- **MSAA 4×** smooths the edges of lines, circles and triangles.
- **HIGHDPI** renders at the screen's real resolution on Retina displays
  (2560 × 1600 for the 1280 × 800 window). Without it, macOS upscales a
  low-resolution image and everything looks pixelated. Code still works in the
  same 1280 × 800 logical units.

Fonts and the scene camera each need one adjustment for high-DPI. See
[Theme and fonts](07-theme-fonts.md) and [Scene](04-scene.md).

### Order matters

- `Fonts_Load` and `UI_Init` need the OpenGL context, so they come after
  `InitWindow`.
- `Sim_Init` must come before any `Sim_SetParams`.
- In the loop, input → clock → **one** `Sim_Current()` → draw scene, HUD, graphs
  → UI last (on top, and it may report a parameter change).

The full loop is explained in [Overall flow](00-overall-flow.md).

---

## Building (`Makefile`)

| Command | Result |
|---|---|
| `make` | builds `./incline_sim` |
| `make test` | builds and runs the two console test programs (no Raylib needed) |
| `make clean` | removes the binaries |

Requirements: **Raylib 5.0+** and `raygui.h` (a copy is included in the project).
The Makefile picks the right libraries for Linux, macOS and Windows (MinGW).

If Raylib isn't in the default include/library paths (for example a Homebrew
install on Apple Silicon), pass them in:

```sh
make INC="$(pkg-config --cflags raylib)" LIB="$(pkg-config --libs-only-L raylib)"
```

---

## Tests (`tests/`)

| File | Kind | What it checks |
|---|---|---|
| `test_physics.c` | console, 57 checks | the mechanics: sticking, the critical-angle threshold, kinetic motion vs closed-form answers, stop-at-apex vs slide-back, 90° and 0°, μk clamp, ramp ends. See [Physics](02-physics.md) |
| `test_sim.c` | console, 23 checks | the clock: jumps, exact replay when going back, `tReached`, clamping, auto-pause at 60 s, stalled frames, param changes. See [Simulation control](03-sim.md) |
| `screenshot.c` | visual smoke test | renders the full UI for 10 scenarios and saves PNGs |

The console tests compile only `physics.c` (and `sim.c`), which shows that those
modules have no dependency on Raylib.

### Screenshot smoke test

Renders scenarios A–J (sliding, thrown up and back down with x/y components,
stick at the apex then jump back in time, static and zoomed, vertical, flat,
pinned against the wall, below the critical angle, stopped, high μ running) and
writes one PNG each. Build it with the same sources as the app:

```sh
gcc -std=c99 -O2 -I. $(pkg-config --cflags raylib) tests/screenshot.c \
    physics.c sim.c scene.c ui.c graphs.c fonts.c theme.c -o shot \
    $(pkg-config --libs raylib) -framework IOKit -framework Cocoa -framework OpenGL
./shot out_dir
```

On a Retina Mac the PNGs are 2560 × 1600.

---

## Tools (`tools/make_font_data.py`)

Regenerates `font_data.h` from the Inter `.otf` files:

```sh
pip install fonttools
python3 tools/make_font_data.py /path/to/inter/fonts
```

It subsets the fonts to the supported characters, removes hinting and layout
features (Raylib uses neither), and tightens the vertical metrics so a requested
size of N px really is an N px em. Keep its `UNICODES` list in sync with
`Fonts_Load()` in `fonts.c`.
