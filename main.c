/*
 * main.c - the frame loop that wires the modules together.
 *
 *   physics.c  (1) Newtonian mechanics: static/kinetic friction, critical angle
 *   sim.c      (2) the clock: Start/Pause/Reset, stepping, cached history
 *   scene.c    (3) ramp, block, force vectors, critical-angle ray, zoom
 *   ui.c       (4) Raygui control panel and transport bar
 *   graphs.c   (5) v-t, a-t and friction-t graphs, and the HUD card
 *   fonts.c / theme.c   shared text rendering and colour palette
 *
 * Each frame: read input, advance the clock, ask the simulation for the block's
 * state at the view time, then draw every module with that one state.
 */
#include "raylib.h"
#include "types.h"
#include "sim.h"
#include "scene.h"
#include "ui.h"
#include "graphs.h"
#include "fonts.h"
#include "theme.h"

int main(void)
{
    /* The values the program starts with. */
    Params params = { .angleDeg = 30.0f, .mass = 5.0f, .g = 9.81f,
                      .mu_s = 0.40f, .mu_k = 0.30f, .v0 = 0.0f };
    ViewOptions view = { .showParallelPerp = true, .showXY = false, .showCritical = true,
                         .showTime = true,
                         .showGraphV = true, .showGraphA = true, .showGraphF = true,
                         .scaleGravity = 1.0f, .scaleNormal = 1.0f, .scaleFriction = 1.0f,
                         .zoom = 1.0f };

    /* MSAA: smooth edges on lines and shapes (ignored if unsupported).
     * HIGHDPI: on Retina / scaled displays, render at the screen's real resolution
     * instead of letting the OS upscale a low-resolution image (which looks pixelated).
     * Coordinates stay in the same 1280 x 800 logical units. */
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_W, SCREEN_H, "Block on an Inclined Plane");
    SetTargetFPS(60);

    Fonts_Load();                               /* needs the window (an OpenGL context) to exist */
    UI_Init();
    Sim_Init(&params);

    /* The area of the window the scene may draw in (everything above the transport bar). */
    const Rectangle sceneRect = { SCENE_X, 0, SCENE_W, SCENE_H };

    while (!WindowShouldClose()) {
        /* ---- update ---- */
        UI_HandleScene(&view, sceneRect);       /* wheel zoom and drag pan */
        Sim_Update(GetFrameTime());             /* advance the clock if running */
        Sample cur = Sim_Current();             /* the block's state at the view time */

        /* ---- draw ---- */
        BeginDrawing();
        ClearBackground(C_BG);
        Scene_Draw(&params, cur, &view, sceneRect);
        HUD_Draw(cur, Sim_Mode(), &view, sceneRect);
        Graphs_Draw(Sim_History(), Sim_PlotCount(), Sim_ReachedTime(), Sim_Time(), cur, &view);
        if (UI_Draw(&params, &view)) Sim_SetParams(&params);   /* a slider moved: restart the run */
        EndDrawing();
    }

    Sim_Shutdown();
    Fonts_Unload();
    CloseWindow();
    return 0;
}
