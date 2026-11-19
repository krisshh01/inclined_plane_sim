/*
 * Visual smoke test: renders the full UI for a few scenarios and saves PNGs.
 * Works with any Raylib build (including the headless PLATFORM_MEMORY one).
 * Usage: screenshot <output-dir>
 */
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include "../types.h"
#include "../sim.h"
#include "../scene.h"
#include "../ui.h"
#include "../graphs.h"
#include "../fonts.h"
#include "../theme.h"

static Params params;
static ViewOptions view;
static const Rectangle sceneRect = { SCENE_X, 0, SCENE_W, SCENE_H };

static void Frame(void)
{
    Sample cur = Sim_Current();
    BeginDrawing();
    ClearBackground(C_BG);
    Scene_Draw(&params, cur, &view, sceneRect);
    HUD_Draw(cur, Sim_Mode(), &view, sceneRect);
    Graphs_Draw(Sim_History(), Sim_PlotCount(), Sim_ReachedTime(), Sim_Time(), cur, &view);
    if (UI_Draw(&params, &view)) Sim_SetParams(&params);
    EndDrawing();
}

static void Shot(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    Image img = LoadImageFromScreen();
#ifdef HEADLESS_FIXUP
    /* The headless software renderer returns bottom-up BGRA pixels; a normal
       window does not need this. */
    ImageFlipVertical(&img);
    unsigned char *px = (unsigned char *)img.data;
    for (int i = 0; i < img.width * img.height; i++) { unsigned char t = px[4*i]; px[4*i] = px[4*i+2]; px[4*i+2] = t; }
#endif
    ExportImage(img, path);
    UnloadImage(img);
    printf("wrote %s (%dx%d)\n", path, img.width, img.height);
}

/* Run the simulation clock for `seconds` of 60 fps frames. */
static void RunFor(double seconds)
{
    Sim_Start();
    int n = (int)(seconds * 60.0 + 0.5);
    for (int i = 0; i < n; i++) { Sim_Update(1.0 / 60.0); Frame(); }
    Sim_Pause();
    Frame();
}

static void Defaults(void)
{
    params = (Params){ .angleDeg = 30, .mass = 5, .g = 9.81f, .mu_s = 0.4f, .mu_k = 0.3f, .v0 = 0 };
    view = (ViewOptions){ .showParallelPerp = true, .showXY = false, .showCritical = true, .showTime = true,
                          .showGraphV = true, .showGraphA = true, .showGraphF = true,
                          .scaleGravity = 1, .scaleNormal = 1, .scaleFriction = 1, .zoom = 1 };
    Sim_SetParams(&params);
    Sim_Reset();
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    SetConfigFlags(FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_W, SCREEN_H, "shot");
    Fonts_Load();
    UI_Init();
    params =(Params){ .angleDeg = 30, .mass = 5, .g = 9.81f, .mu_s = 0.4f, .mu_k = 0.3f, .v0 = 0 };
    Sim_Init(&params);          /* must come before any Sim_SetParams */
    Defaults();

    /* A: default, sliding down, mid-run */
    Frame();
    RunFor(1.2);
    Shot(dir, "A_sliding.png");

    /* B: thrown up the slope, reverses (mu_s < tan th), x/y components on */
    Defaults();
    params.angleDeg = 35; params.mu_s = 0.5f; params.mu_k = 0.3f; params.v0 = 4.0f;
    view.showXY = true; view.showParallelPerp = false;
    Sim_SetParams(&params);
    RunFor(2.4);
    Shot(dir, "B_up_then_down_xy.png");

    /* C: thrown up, sticks at the apex (tan th < mu_s) */
    Defaults();
    params.angleDeg = 20; params.mu_s = 0.6f; params.mu_k = 0.4f; params.v0 = 4.0f;
    Sim_SetParams(&params);
    RunFor(3.0);
    Sim_JumpTo(1.0);          /* step back in time: graphs keep the full curve */
    Frame();
    Shot(dir, "C_sticks_at_apex_jump_back.png");

    /* D: static at rest, only the v graph, zoomed in */
    Defaults();
    params.angleDeg = 25; params.mu_s = 0.6f; params.mu_k = 0.4f;
    view.showGraphA = false; view.showGraphF = false; view.zoom = 1.8f;
    Sim_SetParams(&params);
    RunFor(1.0);
    Shot(dir, "D_static_zoomed.png");

    /* E: vertical ramp (90 deg, free fall): the ramp must still fit the view */
    Defaults();
    params.angleDeg = 90;
    Sim_SetParams(&params);
    RunFor(1.0);
    Shot(dir, "E_vertical.png");

    /* F: flat ramp (0 deg), thrown along it: decelerates with kinetic friction, then rests */
    Defaults();
    params.angleDeg = 0; params.mu_s = 0.5f; params.mu_k = 0.3f; params.v0 = 5.0f;
    Sim_SetParams(&params);
    RunFor(1.0);
    Shot(dir, "F_flat.png");

    /* G: slides all the way down and is pinned against the bottom end */
    Defaults();
    params.angleDeg = 55; params.mu_s = 0.2f; params.mu_k = 0.1f;
    view.showXY = true;
    Sim_SetParams(&params);
    RunFor(3.0);
    Shot(dir, "G_wall.png");

    /* H: below the critical angle (sticks); critical ray hidden-vs-shown is covered by A */
    Defaults();
    params.angleDeg = 18; params.mu_s = 0.5f; params.mu_k = 0.3f;
    Sim_SetParams(&params);
    RunFor(1.0);
    Shot(dir, "H_below_critical.png");

    /* I: fresh STOPPED state, green Start button */
    Defaults();
    Frame();
    Shot(dir, "I_stopped.png");

    /* J: running (amber Pause button), friction slider raised so theta_c moves */
    Defaults();
    params.mu_s = 0.9f; params.mu_k = 0.6f; params.angleDeg = 45;
    Sim_SetParams(&params);
    Sim_Start();
    for (int i = 0; i < 30; i++) { Sim_Update(1.0 / 60.0); Frame(); }
    Shot(dir, "J_running_high_mu.png");

    Sim_Shutdown();
    Fonts_Unload();
    CloseWindow();
    return 0;
}
