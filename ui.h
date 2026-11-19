/*
 * ui.h - Module 4: the control panel (every Raygui widget lives in ui.c).
 *
 * Layout:
 *   - the LEFT PANEL holds the physical parameters, the critical-angle card,
 *     the vector-length sliders and the display options;
 *   - the TRANSPORT BAR under the scene holds Start/Pause/Resume, Reset, step
 *     and time controls, and zoom.
 */
#ifndef UI_H
#define UI_H

#include "raylib.h"
#include "types.h"

/* Call once after Fonts_Load(): sets the Raygui font and the colour theme. */
void UI_Init(void);

/* Draws the left panel and the transport bar, handles the keyboard shortcuts
 * (Space, R, Left/Right) and runs the button actions through the Sim_* functions.
 * Edits Params and ViewOptions in place.
 * Returns true if any PHYSICAL parameter changed this frame; the caller then
 * calls Sim_SetParams() so the history is recomputed. */
bool UI_Draw(Params *p, ViewOptions *v);

/* Mouse-wheel zoom and drag-to-pan inside the scene area (not the transport bar). */
void UI_HandleScene(ViewOptions *v, Rectangle sceneRect);

#endif /* UI_H */
