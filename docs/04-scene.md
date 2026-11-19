# 04 · Scene renderer (`scene.c`, `scene.h`)

**Role:** draws the ramp, ruler, angle markers, block, force vectors and their
labels inside the scene rectangle. **Read-only:** it never changes any state.

Public function: `Scene_Draw(params, cur, view, sceneRect)`.

## Coordinate systems

There are three, and the code converts between them deliberately:

| System | Units | Axes | Used for |
|---|---|---|---|
| **Physical** | newtons / metres | x right, **y up** | force vectors (gravity is `(0, −mg)`) |
| **World** | pixels at zoom 1 | x right, **y down** | everything drawn under the camera |
| **Screen** | logical pixels | x right, y down | text labels, backdrop |

In world space the **foot of the ramp is the origin**, and the slope rises to the
right:

```
up-slope unit vector     u = ( cos θ, −sin θ)
outward surface normal   n = (−sin θ, −cos θ)
```

`ToWorld(phys, k)` converts a physical force to a world vector: scale by `k`
pixels per newton and flip y.

## `Geo`: per-frame geometry

`MakeGeo()` computes everything the drawing steps share, once per frame:

- `th, cs, sn`: the angle, its cosine and sine
- `u, n`: the unit vectors above
- `ppm`: **pixels per metre**, from `FitPPM()`
- `L`: ramp length in pixels
- `cam`: the `Camera2D`

### Auto-fit scale (`FitPPM`)

The scale is chosen every frame so the **whole ramp fits** at any angle. It may
use 88 % of the scene width and 60 % of its height (height gets less because
arrows and labels stick out above and below), capped at 46 px/m so a flat ramp
isn't absurdly large. That is why the ramp grows or shrinks slightly as the angle
changes. The user's zoom and pan are applied on top.

### Camera

- `target` = centre of the ramp triangle, in world space
- `offset` = centre of the scene rectangle + pan, nudged 24 px down so a
  near-vertical ramp stays clear of the HUD card
- `zoom` = `view.zoom`

The camera is applied with `rlPushMatrix` + `rlMultMatrixf(GetCameraMatrix2D(cam))`
instead of `BeginMode2D`. `BeginMode2D` *replaces* the current transform. On a
Retina (high-DPI) window that transform holds the 2× screen scale, and losing it
draws the world at half size. Multiplying on top keeps it.

## Draw order (back to front)

```
BeginScissorMode(sceneRect)                 nothing escapes the scene area
  DrawBackdrop          sky gradient + faint 40 px grid          (screen space)
  ── camera on ──
  DrawGroundAndRamp     ground gradient, horizon glow, wedge, end face,
                        surface glow, surface line, bright top edge
  DrawRuler             tick every metre, longer tick + "N m" label every 5 m,
                        start marker at 10 m (ticks point INTO the ramp)
  DrawAngleMarkers      blue angle wedge + θ label; dashed amber critical ray
  DrawBlock             shadow, body, lighter top strip, outline -> returns centre
  DrawVectors           components first, then W, N, f arrows, then the centre dot
  ── camera off ──
  FlushLabels           all queued text, in screen space
EndScissorMode
```

## The block

`DrawBlock` places the centre `(START_POS + s) · ppm` along `u` and half a block
along `n` (so it sits on the surface). It builds the four corners from `±u` and
`±n` offsets and draws two triangles per quad. It returns the centre, which is
where all the force arrows start.

## Force vectors

### Arrow length (`KEff`)

The base scale is `PX_PER_N = 2` px per newton × the user's vector-length slider.
It is clamped so the **weight** arrow stays between `ARROW_MIN = 36` and
`ARROW_MAX = 240` px. Without that, a 0.1 kg block would have invisible arrows
and a 20 kg block arrows longer than the window. Normal and friction use their
own slider but the same clamp basis, so arrows stay in proportion to each other.

### What is drawn (`DrawVectors`)

| Vector | Physical direction | Colour |
|---|---|---|
| Weight **W** | `(0, −mg)` | red `C_GRAV` |
| Normal **N** | `N · (−sin θ, cos θ)` | blue `C_NORM` |
| Friction **f** | `fric · (cos θ, sin θ)` (signed) | green `C_FRIC` |
| mg sinθ, mg cosθ (optional) | down the slope / into the surface | purple `C_COMP`, dashed |

`DrawForce` draws one arrow plus its label. If "x / y components" is on, it also
draws dashed component arrows along the screen axes, with faint dashed lines
completing the rectangle. If the vector lies on an axis, its component *is* the
vector, so nothing extra is drawn.

### Drawing helpers

- `DrawArrow`: a soft wide glow line, the shaft, and a triangular head (smaller on short arrows)
- `DrawDashed` / `DrawDashedArrow`: 6 px dash, 5 px gap
- `ArrowHead`: a triangle drawn with `Theme_Tri`, so its winding doesn't matter
- `DrawWedge`: a filled pie slice, one segment per ~4°

## Labels: queued, then drawn in screen space

Text is never drawn under the camera. `AddLabel` (with a dark chip behind it) and
`AddText` (plain) project a world point to the screen with `GetWorldToScreen2D`
and **queue** it. `FlushLabels` draws them all after the camera is off. This
gives two benefits:

1. **Text stays the same size** at any zoom.
2. Each label is **clamped inside the scene rectangle**, so it stays readable
   even if its arrow tip is off-screen.

Chips are dark rounded rectangles with a faint outline tinted by the label's
colour.

## Critical-angle ray

A dashed amber line from the foot of the ramp at angle `θc = atan(μs)`, 80 % of
the ramp length long. If the ramp is **steeper** than the ray, the ray lies
inside the ramp: a block released from rest slides. If the ramp is
**shallower**, the ray points above the surface: the block sticks.
