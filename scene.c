/*
 * scene.c - Module 3: the scene renderer.
 *
 * COORDINATE SYSTEMS
 * ------------------
 * World units are pixels at zoom 1 (screen convention: x to the right, y DOWN).
 * The low end of the ramp sits at the world origin and the slope rises to the right:
 *     up-slope unit vector      u = ( cos th, -sin th)
 *     outward surface normal    n = (-sin th, -cos th)
 * Forces are worked out in PHYSICAL coordinates (x right, y UP, so gravity is
 * (0, -mg)) and converted with ToWorld() just before drawing.
 *
 * DRAW ORDER (back to front)
 * --------------------------
 *   backdrop -> [camera on] ground -> ramp -> ruler -> angle wedge -> critical
 *   ray -> block -> force vectors -> [camera off] text labels
 *
 * The scene scale ("pixels per metre") is chosen every frame so the WHOLE ramp
 * fits the view at any angle. The user's zoom and pan then work on top of that.
 */
#include "scene.h"
#include "physics.h"
#include "fonts.h"
#include "theme.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

/* ---------------------------------------------------------------- tuning */
#define PX_PER_N    2.0f      /* base arrow scale: pixels per newton (x the slider)   */
#define ARROW_MIN   36.0f     /* the weight arrow is kept between these lengths (px)  */
#define ARROW_MAX   240.0f
#define LABEL_SIZE  15        /* force / angle label text size (px)                   */
#define MAX_LABELS  48
#define WEDGE_R     64.0f     /* radius of the angle wedge at the foot of the ramp    */

/* --------------------------------------------------- scene-only colours */
static const Color SKY_TOP    = {  12,  19,  37, 255 };
static const Color SKY_BOTTOM = {  22,  35,  66, 255 };
static const Color GROUND_TOP = {  20,  30,  53, 255 };
static const Color GROUND_BOT = {   9,  14,  27, 255 };
static const Color RAMP_FILL  = {  36,  52,  87, 255 };
static const Color RAMP_SURF  = {  98, 130, 200, 255 };
static const Color RAMP_END   = {  24,  36,  62, 255 };
static const Color TICK       = { 138, 160, 205, 255 };
static const Color BLOCK_BODY = { 232, 170,  92, 255 };
static const Color BLOCK_TOP  = { 250, 208, 146, 255 };
static const Color BLOCK_EDGE = { 124,  78,  30, 255 };

/* ===================================================================== */
/*  Screen-space text labels                                             */
/* ===================================================================== */
/*
 * Labels are collected while the camera is active, then drawn after it is
 * closed. That keeps text the same size at any zoom, and lets every label be
 * clamped to stay inside the scene area even if its anchor is off-screen.
 */
typedef struct {
    Vector2 pos;           /* top-left corner in screen pixels               */
    Color   col;           /* text colour                                    */
    bool    chip;          /* draw a dark rounded background behind the text */
    char    text[72];
} Label;

static Label     labels[MAX_LABELS];
static int       nLabels;
static Rectangle labelClip;        /* labels are kept inside this rectangle */

/* Project a world position to the screen and queue a printf-style label there. */
static void PushLabel(Camera2D cam, Vector2 worldPos, float dx, float dy,
                      Color c, bool chip, const char *fmt, va_list ap)
{
    if (nLabels >= MAX_LABELS) return;
    Label *l = &labels[nLabels++];
    Vector2 s = GetWorldToScreen2D(worldPos, cam);
    l->pos  = (Vector2){ s.x + dx, s.y + dy };
    l->col  = c;
    l->chip = chip;
    vsnprintf(l->text, sizeof l->text, fmt, ap);
}

/* A label on a dark chip: used for force values, which must stay readable on any background. */
static void AddLabel(Camera2D cam, Vector2 worldPos, float dx, float dy, Color c, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    PushLabel(cam, worldPos, dx, dy, c, true, fmt, ap);
    va_end(ap);
}

/* A plain text label with no background: used for the ruler and the angle. */
static void AddText(Camera2D cam, Vector2 worldPos, float dx, float dy, Color c, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    PushLabel(cam, worldPos, dx, dy, c, false, fmt, ap);
    va_end(ap);
}

/* Draw every queued label (call after EndMode2D) and empty the queue. */
static void FlushLabels(void)
{
    for (int i = 0; i < nLabels; i++) {
        const Label *l = &labels[i];
        int w = Text_Width(l->text, LABEL_SIZE);
        /* keep the whole label inside the scene area */
        float x = Clamp(l->pos.x, labelClip.x + 10, labelClip.x + labelClip.width  - w - 12);
        float y = Clamp(l->pos.y, labelClip.y +  6, labelClip.y + labelClip.height - 30);
        if (l->chip) {                 /* dark chip with a faint outline in the label's colour */
            const Color chip = { 10, 15, 29, 255 };
            Theme_Card((Rectangle){ x - 7, y - 4, (float)w + 14, LABEL_SIZE + 10 }, 8,
                       chip, Theme_Mix(chip, l->col, 0.35f));
        }
        Text_Draw(l->text, (int)x, (int)y, LABEL_SIZE, l->col);
    }
    nLabels = 0;
}

/* ===================================================================== */
/*  Drawing helpers                                                      */
/* ===================================================================== */

/* Triangular arrow head with its tip at `tip`, pointing along unit vector `dir`. */
static void ArrowHead(Vector2 tip, Vector2 dir, float head, Color c)
{
    Vector2 base = Vector2Subtract(tip, Vector2Scale(dir, head));
    Vector2 perp = { -dir.y, dir.x };
    Vector2 l = Vector2Add(base,      Vector2Scale(perp, head * 0.45f));
    Vector2 r = Vector2Subtract(base, Vector2Scale(perp, head * 0.45f));
    Theme_Tri(tip, l, r, c);
}

/* Solid arrow from `o` along vector `v` (both in world pixels), with a soft glow underneath. */
static void DrawArrow(Vector2 o, Vector2 v, Color c, float thick)
{
    float len = Vector2Length(v);
    if (len < 1.0f) return;                              /* too short to draw */
    Vector2 d    = Vector2Scale(v, 1.0f / len);
    float   head = fminf(15.0f, len * 0.45f);            /* short arrows get small heads */
    Vector2 tip  = Vector2Add(o, v);
    Vector2 neck = Vector2Subtract(tip, Vector2Scale(d, head * 0.8f));
    DrawLineEx(o, neck, thick + 6.0f, Fade(c, 0.18f));   /* glow  */
    DrawLineEx(o, neck, thick, c);                       /* shaft */
    ArrowHead(tip, d, head, c);
}

/* Dashed line from a to b. */
static void DrawDashed(Vector2 a, Vector2 b, Color c, float thick)
{
    float len = Vector2Distance(a, b);
    if (len < 1.0f) return;
    Vector2 d = Vector2Scale(Vector2Subtract(b, a), 1.0f / len);
    for (float t = 0.0f; t < len; t += 11.0f) {          /* 6 px dash, 5 px gap */
        float e = fminf(t + 6.0f, len);
        DrawLineEx(Vector2Add(a, Vector2Scale(d, t)), Vector2Add(a, Vector2Scale(d, e)), thick, c);
    }
}

/* Dashed arrow: used for the components of a vector. */
static void DrawDashedArrow(Vector2 o, Vector2 v, Color c)
{
    float len = Vector2Length(v);
    if (len < 2.0f) return;
    Vector2 d    = Vector2Scale(v, 1.0f / len);
    float   head = fminf(10.0f, len * 0.4f);
    Vector2 tip  = Vector2Add(o, v);
    DrawDashed(o, Vector2Subtract(tip, Vector2Scale(d, head * 0.8f)), c, 2.0f);
    ArrowHead(tip, d, head, c);
}

/* Filled circular wedge from angle 0 to `angRad` (counter-clockwise on screen) at centre c. */
static void DrawWedge(Vector2 c, float radius, float angRad, Color col)
{
    int seg = (int)(angRad * RAD2DEG / 4.0f) + 2;        /* about one slice per 4 degrees */
    for (int i = 0; i < seg; i++) {
        float a0 = angRad * (float)i       / (float)seg;
        float a1 = angRad * (float)(i + 1) / (float)seg;
        Vector2 p0 = { c.x + radius * cosf(a0), c.y - radius * sinf(a0) };
        Vector2 p1 = { c.x + radius * cosf(a1), c.y - radius * sinf(a1) };
        Theme_Tri(c, p0, p1, col);
    }
}

/* Effective pixels-per-newton so the weight arrow stays between ARROW_MIN and
 * ARROW_MAX pixels. Without this, a 0.1 kg block would have an invisible arrow
 * and a 20 kg block one longer than the window. */
static float KEff(float scale, double weight)
{
    float k = scale * PX_PER_N;
    if (weight < 1e-6) return k;
    float kmin = ARROW_MIN / (float)weight, kmax = ARROW_MAX / (float)weight;
    if (k < kmin) k = kmin;
    if (k > kmax) k = kmax;
    return k;
}

/* Physical vector (y up, newtons) -> world vector (y down, pixels). */
static Vector2 ToWorld(Vector2 phys, float k) { return (Vector2){ phys.x * k, -phys.y * k }; }

/*
 * One force vector with its label and, optionally, its x / y components.
 *   o        origin (block centre) in world pixels
 *   F        the force in physical coordinates, newtons
 *   k        pixels per newton for this arrow
 *   name     short name for the component labels ("W", "N", "f")
 *   text     the main label, e.g. "N = 42.48 N"
 *   ldx,ldy  label offset from the arrow tip; ldx < 0 means "put it LEFT of the tip"
 */
static void DrawForce(Camera2D cam, Vector2 o, Vector2 F, float k, Color c,
                      const char *name, const char *text, float ldx, float ldy, bool xy)
{
    Vector2 w = ToWorld(F, k);
    if (Vector2Length(w) >= 1.0f) {
        DrawArrow(o, w, c, 3.0f);
        if (ldx < 0) ldx = -(float)(Text_Width(text, LABEL_SIZE) + 16);   /* label left of the tip */
        AddLabel(cam, Vector2Add(o, w), ldx, ldy, c, "%s", text);
    }
    if (!xy) return;

    /* x / y components as dashed arrows along the screen axes */
    Color cc = Fade(c, 0.6f);
    Vector2 wx = { w.x, 0.0f }, wy = { 0.0f, w.y };
    bool hasX = fabsf(wx.x) >= 2.0f, hasY = fabsf(wy.y) >= 2.0f;
    if (hasX != hasY) return;           /* the vector lies on an axis: its component IS the vector */
    if (hasX && hasY) {                 /* complete the rectangle with faint dashed lines */
        DrawDashed(Vector2Add(o, wx), Vector2Add(o, w), Fade(c, 0.35f), 1.5f);
        DrawDashed(Vector2Add(o, wy), Vector2Add(o, w), Fade(c, 0.35f), 1.5f);
    }
    if (hasX) {
        DrawDashedArrow(o, wx, cc);
        AddLabel(cam, Vector2Add(o, wx), 6, (wy.y > 0 ? -26 : 6), cc, "%sx=%+.2f", name, F.x);
    }
    if (hasY) {
        DrawDashedArrow(o, wy, cc);
        AddLabel(cam, Vector2Add(o, wy), (wx.x > 0 ? -92 : 8), -10, cc, "%sy=%+.2f", name, F.y);
    }
}

/* ===================================================================== */
/*  Scene geometry                                                       */
/* ===================================================================== */

/* Everything the drawing steps need to agree on, computed once per frame. */
typedef struct {
    float   th, cs, sn;     /* ramp angle (radians), its cosine and sine          */
    Vector2 u, n;           /* up-slope and outward-normal unit vectors (screen)  */
    float   ppm;            /* world pixels per metre at zoom 1                   */
    float   L;              /* ramp length in world pixels                        */
    Camera2D cam;           /* also used to project labels into screen space      */
} Geo;

/*
 * Pixels per metre at zoom 1, chosen so the WHOLE ramp fits the scene at any
 * angle. Height gets less room than width because the force arrows and labels
 * extend above and below the ramp.
 */
static float FitPPM(float th, Rectangle r)
{
    float w = (float)RAMP_LENGTH * fmaxf(cosf(th), 1e-3f);    /* horizontal extent (m) */
    float h = (float)RAMP_LENGTH * fmaxf(sinf(th), 1e-3f);    /* vertical extent (m)   */
    float s = fminf(r.width * 0.88f / w, r.height * 0.60f / h);
    return fminf(s, 46.0f);                                   /* never absurdly big on a flat ramp */
}

static Geo MakeGeo(const Params *p, const ViewOptions *v, Rectangle sceneRect)
{
    Geo g;
    g.th  = p->angleDeg * DEG2RAD;
    g.cs  = cosf(g.th);
    g.sn  = sinf(g.th);
    g.u   = (Vector2){  g.cs, -g.sn };
    g.n   = (Vector2){ -g.sn, -g.cs };
    g.ppm = FitPPM(g.th, sceneRect);
    g.L   = (float)RAMP_LENGTH * g.ppm;

    /* The camera looks at the middle of the ramp triangle. The scene is nudged 24 px
     * down so a near-vertical ramp stays clear of the HUD card in the top-left corner. */
    g.cam.offset   = (Vector2){ sceneRect.x + sceneRect.width  * 0.5f + v->panX,
                                sceneRect.y + sceneRect.height * 0.5f + 24.0f + v->panY };
    g.cam.target   = (Vector2){ g.L * g.cs * 0.5f, -g.L * g.sn * 0.5f };
    g.cam.rotation = 0.0f;
    g.cam.zoom     = v->zoom;
    return g;
}

/* ===================================================================== */
/*  Scene pieces                                                         */
/* ===================================================================== */

/* Sky gradient and a faint technical grid, in screen space (behind the camera). */
static void DrawBackdrop(Rectangle r)
{
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)r.height, SKY_TOP, SKY_BOTTOM);
    const Color grid = { 255, 255, 255, 8 };
    for (float x = r.x; x < r.x + r.width;  x += 40.0f) DrawRectangle((int)x, (int)r.y, 1, (int)r.height, grid);
    for (float y = r.y; y < r.y + r.height; y += 40.0f) DrawRectangle((int)r.x, (int)y, (int)r.width, 1, grid);
}

/* The ground and the ramp itself (world space). */
static void DrawGroundAndRamp(const Geo *g)
{
    Vector2 A = { 0, 0 };                         /* foot of the ramp           */
    Vector2 B = { g->L * g->cs, 0 };              /* bottom of the vertical end */
    Vector2 C = { g->L * g->cs, -g->L * g->sn };  /* top of the ramp            */

    DrawRectangleGradientV(-6000, 0, 12000, 3000, GROUND_TOP, GROUND_BOT);
    DrawRectangleGradientV(-6000, -40, 12000, 40, Fade(RAMP_SURF, 0.0f), Fade(RAMP_SURF, 0.06f));   /* horizon glow */
    DrawLineEx((Vector2){ -6000, 0 }, (Vector2){ 6000, 0 }, 2.0f, C_BORDER);

    Theme_Tri(A, B, C, RAMP_FILL);                /* the wedge                  */
    DrawLineEx(B, C, 2.0f, RAMP_END);             /* vertical end face          */
    DrawLineEx(A, C, 9.0f, Fade(RAMP_SURF, 0.18f));   /* soft glow under the surface */
    DrawLineEx(A, C, 5.0f, RAMP_SURF);            /* the sloping surface        */
    DrawLineEx(Vector2Add(A, Vector2Scale(g->n, 1.5f)), Vector2Add(C, Vector2Scale(g->n, 1.5f)),
               1.5f, Theme_Mix(RAMP_SURF, WHITE, 0.45f));   /* bright top edge */
}

/* Centimetre-style ruler along the surface: a tick every metre, a label every 5 m,
 * and the start marker. Ticks point INTO the ramp so they never cross the block. */
static void DrawRuler(const Geo *g)
{
    for (int i = 0; i <= (int)RAMP_LENGTH; i++) {
        Vector2 q = Vector2Scale(g->u, i * g->ppm);
        float len = (i % 5 == 0) ? 12.0f : 6.0f;
        DrawLineEx(q, Vector2Add(q, Vector2Scale(g->n, -len)), 1.5f, TICK);
        if (i > 0 && i % 5 == 0 && 1)
            AddText(g->cam, Vector2Add(q, Vector2Scale(g->n, -30.0f)), -12, -8, C_TEXT_MUTED, "%d m", i);
    }

    /* the start marker: a line across the surface where the block begins */
    Vector2 q = Vector2Scale(g->u, (float)START_POS * g->ppm);
    DrawLineEx(Vector2Add(q, Vector2Scale(g->n, -10)), Vector2Add(q, Vector2Scale(g->n, 10)), 2.0f, C_TEXT_DIM);
}

/* The current-angle wedge and, when enabled, the critical-angle ray.
 *
 * The critical ray is drawn at theta_c = atan(mu_s) from the foot of the ramp.
 * If the ramp is STEEPER than the ray, the ray lies inside the ramp and the block
 * will slide; if the ramp is SHALLOWER, the ray lies above it and the block sticks. */
static void DrawAngleMarkers(const Params *p, const ViewOptions *v, const Geo *g)
{
    const Vector2 A = { 0, 0 };

    if (p->angleDeg > 0.5f) {
        DrawWedge(A, WEDGE_R, g->th, Fade(C_ACCENT, 0.22f));
        DrawRingLines(A, WEDGE_R, WEDGE_R, -p->angleDeg, 0, 40, C_ACCENT);
        AddText(g->cam, (Vector2){ WEDGE_R * 0.35f, 6.0f }, 0, 0, C_ACCENT_HI, "%.1f\xC2\xB0", p->angleDeg);   /* under the ground line, clear of the vectors */
    }

    if (v->showCritical) {
        double thcDeg = Physics_CriticalAngle(p->mu_s);
        float  thc    = (float)thcDeg * DEG2RAD;
        float  len    = 0.8f * g->L;                                /* ray length: 80 % of the ramp, so its label is far from the block */
        Vector2 tip   = { len * cosf(thc), -len * sinf(thc) };
        DrawDashed(A, tip, C_CRIT, 2.5f);
        DrawCircleV(tip, 4.0f, C_CRIT);
        char t[48];
        snprintf(t, sizeof t, "\xCE\xB8""c = %.1f\xC2\xB0 critical", thcDeg);
        AddLabel(g->cam, tip, -(float)(Text_Width(t, LABEL_SIZE) + 16), 8, C_CRIT, "%s", t);   /* left of the tip */
    }
}

/* The block: soft shadow, body, a lighter top face and a dark outline.
 * Returns the block centre in world pixels (the origin of the force arrows). */
static Vector2 DrawBlock(Sample cur, const Geo *g)
{
    float h = (float)BLOCK_SIZE * g->ppm * 0.5f;                  /* half the block side, px */
    Vector2 c = Vector2Add(Vector2Scale(g->u, ((float)START_POS + (float)cur.s) * g->ppm),
                           Vector2Scale(g->n, h));                /* centre sits h above the surface */

    /* corners: along-slope (+/-u) and across-slope (+/-n) offsets from the centre */
    Vector2 c0 = Vector2Add(c, Vector2Add(Vector2Scale(g->u, -h), Vector2Scale(g->n, -h)));  /* bottom-left  */
    Vector2 c1 = Vector2Add(c, Vector2Add(Vector2Scale(g->u,  h), Vector2Scale(g->n, -h)));  /* bottom-right */
    Vector2 c2 = Vector2Add(c, Vector2Add(Vector2Scale(g->u,  h), Vector2Scale(g->n,  h)));  /* top-right    */
    Vector2 c3 = Vector2Add(c, Vector2Add(Vector2Scale(g->u, -h), Vector2Scale(g->n,  h)));  /* top-left     */

    /* shadow: the same quad nudged down-right and darkened */
    const Vector2 so = { 4.0f, 5.0f };
    const Color   sc = { 0, 0, 0, 90 };
    Theme_Tri(Vector2Add(c0, so), Vector2Add(c1, so), Vector2Add(c2, so), sc);
    Theme_Tri(Vector2Add(c0, so), Vector2Add(c2, so), Vector2Add(c3, so), sc);

    /* body */
    Theme_Tri(c0, c1, c2, BLOCK_BODY);
    Theme_Tri(c0, c2, c3, BLOCK_BODY);

    /* lighter strip along the top edge, for a hint of depth */
    Vector2 in = Vector2Scale(g->n, -0.24f * 2.0f * h);
    Theme_Tri(c3, c2, Vector2Add(c2, in), BLOCK_TOP);
    Theme_Tri(c3, Vector2Add(c2, in), Vector2Add(c3, in), BLOCK_TOP);

    /* outline */
    DrawLineEx(c0, c1, 2.0f, BLOCK_EDGE);
    DrawLineEx(c1, c2, 2.0f, BLOCK_EDGE);
    DrawLineEx(c2, c3, 2.0f, BLOCK_EDGE);
    DrawLineEx(c3, c0, 2.0f, BLOCK_EDGE);
    return c;
}

/* Gravity, normal and friction arrows from the block centre, plus the optional
 * parallel / perpendicular components of gravity. */
static void DrawVectors(const Params *p, Sample cur, const ViewOptions *v, const Geo *g, Vector2 c)
{
    const Forces f = Physics_Forces(&cur, p);

    /* physical unit vectors (y UP): up the slope and the outward normal */
    const Vector2 upP  = { g->cs, g->sn };
    const Vector2 nrmP = { -g->sn, g->cs };

    float kg = KEff(v->scaleGravity,  f.weight);
    float kn = KEff(v->scaleNormal,   f.weight);
    float kf = KEff(v->scaleFriction, f.weight);

    Vector2 Fg = { 0.0f, -(float)f.weight };                  /* gravity: straight down */
    Vector2 Fn = Vector2Scale(nrmP, (float)f.normal);         /* normal: out of the surface */
    Vector2 Ff = Vector2Scale(upP,  (float)f.friction);       /* friction: along the slope (signed) */

    /* gravity components, drawn first so the main arrows sit on top */
    if (v->showParallelPerp) {
        Vector2 Fpar  = Vector2Scale(upP,  -(float)f.gPar);   /* down the slope   */
        Vector2 Fperp = Vector2Scale(nrmP, -(float)f.gPerp);  /* into the surface */
        Vector2 wPar = ToWorld(Fpar, kg), wPerp = ToWorld(Fperp, kg), wG = ToWorld(Fg, kg);
        DrawDashed(Vector2Add(c, wPar),  Vector2Add(c, wG), Fade(C_COMP, 0.4f), 1.5f);   /* complete the rectangle */
        DrawDashed(Vector2Add(c, wPerp), Vector2Add(c, wG), Fade(C_COMP, 0.4f), 1.5f);
        DrawDashedArrow(c, wPar,  C_COMP);
        DrawDashedArrow(c, wPerp, C_COMP);
        if (Vector2Length(wPar)  >= 2.0f) AddLabel(g->cam, Vector2Add(c, wPar),  8, 6, C_COMP, "mg sin\xCE\xB8 = %.2f N", f.gPar);
        if (Vector2Length(wPerp) >= 2.0f) AddLabel(g->cam, Vector2Add(c, wPerp), 8, 6, C_COMP, "mg cos\xCE\xB8 = %.2f N", f.gPerp);
    }

    char tg[48], tn[48], tf[64];
    snprintf(tg, sizeof tg, "W = %.2f N", f.weight);
    snprintf(tn, sizeof tn, "N = %.2f N", f.normal);
    snprintf(tf, sizeof tf, "f = %+.2f N (%s)", f.friction, cur.staticFric ? "static" : "kinetic");

    DrawForce(g->cam, c, Fg, kg, C_GRAV, "W", tg, -1, 0,   v->showXY);   /* -1: label to the left */
    DrawForce(g->cam, c, Fn, kn, C_NORM, "N", tn, 10, -26, v->showXY);
    DrawForce(g->cam, c, Ff, kf, C_FRIC, "f", tf, 10, 10,  v->showXY);

    DrawCircleV(c, 4.0f, C_TEXT);                              /* the point the forces act on */
}

/* ===================================================================== */
/*  Public entry point                                                   */
/* ===================================================================== */
void Scene_Draw(const Params *p, Sample cur, const ViewOptions *v, Rectangle sceneRect)
{
    Geo g = MakeGeo(p, v, sceneRect);

    nLabels   = 0;
    labelClip = sceneRect;

    /* Everything is clipped to the scene rectangle, including anything that
     * zooms or pans far off-screen. */
    BeginScissorMode((int)sceneRect.x, (int)sceneRect.y, (int)sceneRect.width, (int)sceneRect.height);

    DrawBackdrop(sceneRect);

    /* Apply the camera ON TOP of the current transform instead of BeginMode2D(),
     * which replaces it: on a high-DPI (Retina) window that transform holds the
     * screen scale, and losing it draws the world at half size. */
    rlDrawRenderBatchActive();
    rlPushMatrix();
    rlMultMatrixf(MatrixToFloat(GetCameraMatrix2D(g.cam)));
        DrawGroundAndRamp(&g);
        DrawRuler(&g);
        DrawAngleMarkers(p, v, &g);
        Vector2 c = DrawBlock(cur, &g);
        DrawVectors(p, cur, v, &g, c);
    rlDrawRenderBatchActive();
    rlPopMatrix();

    FlushLabels();                  /* text is drawn in screen space, after the camera closes */
    EndScissorMode();
}
