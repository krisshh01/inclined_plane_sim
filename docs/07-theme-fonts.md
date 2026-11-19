# 07 · Theme and fonts (`theme.c/h`, `fonts.c/h`, `font_data.h`)

These two small shared modules give the whole app one visual identity. Change
the palette in `theme.c` or the font in `fonts.c`, and every module follows.

---

## Theme (`theme.c`, `theme.h`)

### Palette

A dark blue-grey interface. Surfaces get **lighter as they come forward**:

| Constant | Use |
|---|---|
| `C_BG` → `C_PANEL` → `C_CARD` → `C_CARD_HI` | window → panels → cards → raised controls |
| `C_BORDER` | outlines |
| `C_PLOT_BG`, `C_GRID` | inside graphs |
| `C_CHIP` | translucent label background |
| `C_TEXT`, `C_TEXT_DIM`, `C_TEXT_MUTED` | primary, secondary, hint text |
| `C_ACCENT`, `C_ACCENT_HI` | blue for sliders, selections, hover |
| `C_GOOD` / `C_WARN` | green (Start, sticks, at rest) / amber (Pause, slides) |

**Physics colours.** The same colour means the same quantity everywhere:

| Constant | Quantity |
|---|---|
| `C_GRAV` (red) | weight |
| `C_NORM` (blue) | normal force, and velocity |
| `C_FRIC` (green) | friction |
| `C_COMP` (purple) | gravity components |
| `C_CRIT` (amber) | critical angle |

`scene.c` and `ui.c` add a few local colours (sky, ramp, block, input field) that
only they use.

### Helpers

| Function | Purpose |
|---|---|
| `Theme_Mix(a, b, t)` | linear blend between two colours (used for hover and pressed shades) |
| `Theme_Rounded(rect, radiusPx, colour)` | filled rounded rectangle with the radius in **pixels**. Raylib's own parameter is relative "roundness" (0–1 of half the short side), which is awkward to use |
| `Theme_Card(rect, radius, fill, border)` | 1 px border + fill, drawn as two rounded rectangles. This works on every Raylib 5.x/6.x, whose rounded-outline function changed signature. The fill must be **opaque** |
| `Theme_Tri(a, b, c, colour)` | a triangle that shows whichever way its corners are ordered. Raylib culls clockwise triangles, a common cause of invisible shapes, so it draws both windings |

---

## Fonts (`fonts.c`, `fonts.h`, `font_data.h`)

### The font

All text uses **Inter** (Regular + SemiBold), embedded in `font_data.h` as byte
arrays (~11 KB each). There are no font files to ship and nothing to find at run
time. Licence: SIL OFL 1.1 (`LICENSE-Inter-OFL.txt`).

Supported characters: printable ASCII plus `° ² · × θ μ − ≤ ≥ ≈`. Any other
character renders as a box. To add one, update **both** the `UNICODES` list in
`tools/make_font_data.py` (and regenerate `font_data.h`) **and** the `extra[]`
list in `Fonts_Load()`.

### API

| Function | Purpose |
|---|---|
| `Fonts_Load()` / `Fonts_Unload()` | build the glyph list after `InitWindow` / free atlases before `CloseWindow` |
| `Fonts_Get(size)`, `Fonts_GetBold(size)` | the `Font` at a size (used for `GuiSetFont`) |
| `Text_Draw`, `Text_DrawRight`, `Text_Width` | Regular weight; `Right` means the text *ends* at x |
| `Text_DrawBold`, `Text_DrawBoldRight`, `Text_WidthBold` | SemiBold weight |

Coordinates are the text's top-left corner, in logical pixels.

### One crisp atlas per size, loaded lazily

Scaling a single glyph atlas up and down blurs text. Instead, `Get(size, bold)`
rasterises a **separate atlas for each (size, weight)** the first time it is
asked for, and caches it (up to 32). The app uses only a handful of sizes, so
this is cheap.

### High-DPI (Retina) screens

On a Retina display one logical pixel is 2×2 real pixels. `Get()` reads
`GetWindowScaleDPI()` and rasterises the atlas at `size × scale` real pixels,
then draws it at `size` logical pixels. Each glyph pixel maps 1:1 to a screen
pixel, so text is sharp rather than upscaled. On a normal screen the scale is 1
and nothing changes.

### Small details

- `BoldSpacing(size)` adds 4 % of the size between bold letters. Raylib rounds
  bold advances to whole pixels, which packs bold words too tightly.
- Raylib's "glyph taller than the font size" warnings are muted while loading;
  they are harmless.
- Bilinear filtering gives smooth sub-pixel positioning.
- If loading fails, Raylib's built-in font is used as a fallback.
