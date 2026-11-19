# 06 · Graphs and HUD (`graphs.c`, `graphs.h`)

**Role:** the right-hand column of graphs and the status card over the scene.
Both are **read-only consumers**: they only draw what they are given.

## Public functions

| Function | Draws |
|---|---|
| `Graphs_Draw(history, count, tReached, tView, cur, view)` | the graph column |
| `HUD_Draw(cur, mode, view, sceneRect)` | the status card in the scene's top-left corner |

## One generic graph, three definitions

A graph shows one number per `Sample`. An **accessor** function picks it out:

```c
static double AccV(const Sample *s) { return s->v; }
static double AccA(const Sample *s) { return s->a; }
static double AccF(const Sample *s) { return s->fric; }

GraphDef defs[3] = {
    { "v (m/s) vs t",        AccV, C_NORM  },   // blue
    { "a (m/s²) vs t",       AccA, orange  },
    { "Friction f (N) vs t", AccF, C_FRIC  },   // green
};
```

`DrawGraph()` is shared by all three. Only the title, colour and accessor differ.

## Column layout (`Graphs_Draw`)

- The enabled graphs share the column height equally, each at most 300 px, with
  12 px padding and 10 px gaps.
- The x-axis spans `0 … max(tReached, 2 s)`, so a fresh run still shows a
  sensible 2 s axis.
- If every graph is switched off, the column shows "Enable a graph in the panel".

## Inside one graph (`DrawGraph`)

1. **Card and header.** Colour dot, title, and the current value on a pill
   tinted with the curve colour.
2. **Plot area.** Rounded dark rectangle.
3. **Y range.** Min and max over all plotted samples, **always including 0** so
   the zero line is visible. Padded by 8 %. A flat line gets ±1 so it isn't
   squashed.
4. **Grid and ticks.**
   - `NiceStep(range, target)` picks a tick spacing of 1, 2 or 5 × 10ⁿ giving
     about `target` ticks (4 on y, 5 on x).
   - `Decimals(step)` picks how many decimals the labels need so neighbours
     differ.
   - The zero line is drawn brighter; "−0" is never printed.
5. **The curve, in step style.** a(t) and friction(t) are piecewise constant:
   they jump at a stick/slide transition. So every curve is drawn as *hold the
   value, then jump vertically*. That is physically accurate and needs no
   interpolation.
6. **Downsampling.** A long run has thousands of samples, so it takes about one
   per pixel of width (`stride = count / plotWidth`). It always ends exactly on
   the newest sample.
7. **Fill.** Between the curve and the zero line, fading from the curve toward
   zero. It is drawn in **whole pixel columns**, so neighbouring translucent
   pieces never overlap (overlaps used to show up as vertical stripes).
8. **Cursor.** A thin vertical line at `tView` and a dot with a halo on the curve.
   When you step back in time, the cursor moves left along the full curve
   (because the curve runs to `tReached`).

Everything inside the plot is clipped with `BeginScissorMode`.

## HUD card (`HUD_Draw`)

```
┌──────────────────────────────┐
│ ● SLIDING            RUNNING │   state pill (amber SLIDING / green AT REST) + run mode
│ Kinetic friction acts        │   the physical reason
│ ──────────────────────────── │
│ Time                 1.200 s │   (optional: "Time" checkbox)
│ Position s          -1.697 m │
│ Velocity v        -2.828 m/s │   blue, matching the v graph and normal arrow colour
│ Acceleration a   -2.356 m/s² │   amber
│ Friction (kinetic) +12.744 N │   green; the label says static or kinetic
│ + is up the slope            │
└──────────────────────────────┘
```

The reason line comes from the sample flags:

| Flags | Text |
|---|---|
| `atWall` | "Pinned against the end of the ramp" |
| `sliding` | "Kinetic friction acts" |
| otherwise | "Static friction holds the block" |

The card's height adapts to whether the Time row is shown.

## Colour consistency

A physical quantity keeps **one colour everywhere**. Friction is green as an
arrow, a graph and a HUD row. This lets the eye link the scene, the numbers and
the graphs without reading labels.
