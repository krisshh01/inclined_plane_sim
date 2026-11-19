/*
 * graphs.c - Module 5: graphs and the HUD readout.
 *
 * One generic DrawGraph() serves all three plots; the only differences are the
 * title, the colour and the function that picks a value out of a Sample.
 *
 * a(t) and friction(t) are piecewise constant (they JUMP at the stick/slide
 * transition), so every curve is drawn in "step" style: hold the value, then
 * jump vertically. That is physically accurate and needs no interpolation.
 */
#include "graphs.h"
#include "fonts.h"
#include "theme.h"
#include <math.h>
#include <stdio.h>

/* A graph shows one number per Sample. An "accessor" picks it out. */
typedef double (*Accessor)(const Sample *);
static double AccV(const Sample *s) { return s->v; }
static double AccA(const Sample *s) { return s->a; }
static double AccF(const Sample *s) { return s->fric; }

typedef struct {
    const char *title;      /* includes the unit, e.g. "v (m/s) vs t" */
    Accessor    acc;
    Color       col;
} GraphDef;

/* ---------------------------------------------------------- axis helpers */

/* A "nice" tick spacing (1, 2, 5 times a power of ten) that gives about `target` ticks. */
static double NiceStep(double range, int target)
{
    if (range <= 0.0) return 1.0;
    double raw  = range / target;
    double mag  = pow(10.0, floor(log10(raw)));
    double n    = raw / mag;
    double nice = (n < 1.5) ? 1.0 : (n < 3.5) ? 2.0 : (n < 7.5) ? 5.0 : 10.0;
    return nice * mag;
}

/* How many decimals a tick label needs so neighbouring labels differ. */
static int Decimals(double step)
{
    if (step >= 1.0) return 0;
    int d = (int)ceil(-log10(step) - 1e-9);
    return d < 0 ? 0 : (d > 4 ? 4 : d);
}

/* ------------------------------------------------------------ one graph */
/*
 *   r      the card rectangle
 *   h,count the history and the number of samples to plot
 *   tView  the time cursor
 *   tPlot  the time at the right edge of the plot
 *   cur    the current sample (for the "now" value)
 */
static void DrawGraph(Rectangle r, const History *h, int count, const GraphDef *g,
                      double tView, double tPlot, const Sample *cur)
{
    /* card, title, and the current value in the curve's own colour */
    Theme_Card(r, 12, C_CARD, C_BORDER);
    DrawRectangle((int)r.x + 12, (int)r.y + 1, (int)r.width - 24, 1, Fade(WHITE, 0.05f));   /* top sheen */
    DrawCircleV((Vector2){ r.x + 19, r.y + 21 }, 4.0f, g->col);                             /* colour key */
    Text_DrawBold(g->title, (int)r.x + 30, (int)r.y + 12, 15, C_TEXT);
    /* the current value on a tinted pill in the curve's own colour */
    const char *val = TextFormat("%+.3f", g->acc(cur));
    int vw = Text_WidthBold(val, 14);
    Theme_Rounded((Rectangle){ r.x + r.width - 14 - vw - 16, r.y + 9, (float)vw + 16, 24 }, 12, Fade(g->col, 0.16f));
    Text_DrawBoldRight(val, (int)(r.x + r.width) - 22, (int)r.y + 13, 14, g->col);

    /* the plot area, inside the card */
    Rectangle pl = { r.x + 52, r.y + 42, r.width - 52 - 16, r.height - 42 - 40 };
    Theme_Rounded((Rectangle){ pl.x - 1, pl.y - 1, pl.width + 2, pl.height + 2 }, 6, C_PLOT_BG);

    /* y range from every plotted sample; always include 0 so the axis is visible */
    double lo = 0.0, hi = 0.0;
    for (int i = 0; i < count; i++) {
        double val = g->acc(&h->data[i]);
        if (val < lo) lo = val;
        if (val > hi) hi = val;
    }
    if (hi - lo < 1e-9) { hi += 1.0; lo -= 1.0; }     /* flat line: give it some height */
    double pad = (hi - lo) * 0.08;
    lo -= pad; hi += pad;

    /* data -> pixel conversion (macros so they can use the locals above) */
    #define PX(t)   ((float)(pl.x + (t) / tPlot * pl.width))
    #define PY(val) ((float)(pl.y + pl.height - ((val) - lo) / (hi - lo) * pl.height))

    /* horizontal grid lines and y tick labels */
    double ys = NiceStep(hi - lo, 4);
    int    yd = Decimals(ys);
    for (double yv = ceil(lo / ys) * ys; yv <= hi + 1e-9; yv += ys) {
        float py   = PY(yv);
        bool  zero = fabs(yv) < ys * 1e-6;
        if (zero) DrawRectangle((int)pl.x, (int)roundf(py), (int)pl.width, 1, Fade(C_TEXT_DIM, 0.55f));   /* the zero line stands out */
        else      DrawRectangle((int)pl.x, (int)roundf(py), (int)pl.width, 1, C_GRID);
        const char *s = TextFormat("%.*f", yd, zero ? 0.0 : yv);               /* avoid printing "-0" */
        Text_Draw(s, (int)pl.x - 8 - Text_Width(s, 12), (int)py - 8, 12, C_TEXT_MUTED);
    }
    /* vertical grid lines and x tick labels */
    double xs = NiceStep(tPlot, 5);
    int    xd = Decimals(xs);
    for (double xv = 0.0; xv <= tPlot + 1e-9; xv += xs) {
        float px = PX(xv);
        DrawRectangle((int)roundf(px), (int)pl.y, 1, (int)pl.height, Fade(C_GRID, 0.6f));
        const char *s = TextFormat("%.*f", xd, xv);
        Text_Draw(s, (int)px - Text_Width(s, 12) / 2, (int)(pl.y + pl.height + 6), 12, C_TEXT_MUTED);
    }
    Text_Draw("t (s)", (int)(pl.x + pl.width / 2) - Text_Width("t (s)", 12) / 2,
              (int)(r.y + r.height - 22), 12, C_TEXT_MUTED);

    /* The curve. With a long run there are thousands of samples, so take about one
     * per pixel of width (the `stride`), which looks identical and draws fast. */
    int stride = count / (int)pl.width;
    if (stride < 1) stride = 1;

    BeginScissorMode((int)pl.x, (int)pl.y - 1, (int)pl.width + 1, (int)pl.height + 2);
    float zy = PY(0.0);                                        /* pixel row of the value 0 */
    Vector2 prev = { PX(h->data[0].t), PY(g->acc(&h->data[0])) };
    for (int i = stride; ; i += stride) {
        bool last = false;
        if (i >= count - 1) { i = count - 1; last = true; }    /* always end exactly on the newest sample */
        Vector2 q = { PX(h->data[i].t), PY(g->acc(&h->data[i])) };

        /* step style: hold the previous value across to the new time, then jump */
        /* soft fill between the curve and the zero line. Whole pixel columns only, so
         * neighbouring translucent pieces never overlap (overlaps showed as stripes).
         * It fades from the curve towards the zero line. */
        int fx0 = (int)roundf(prev.x), fx1 = (int)roundf(q.x);
        int fy0 = (int)roundf(fminf(prev.y, zy)), fy1 = (int)roundf(fmaxf(prev.y, zy));
        if (fx1 > fx0 && fy1 > fy0) {
            Color strong = Fade(g->col, 0.30f), faint = Fade(g->col, 0.02f);
            bool above = prev.y < zy;                          /* positive value: curve on top */
            DrawRectangleGradientV(fx0, fy0, fx1 - fx0, fy1 - fy0, above ? strong : faint, above ? faint : strong);
        }
        DrawLineEx(prev, (Vector2){ q.x, prev.y }, 2.0f, g->col);                /* hold  */
        DrawLineEx((Vector2){ q.x, prev.y }, q, 2.0f, g->col);                   /* jump  */
        prev = q;
        if (last) break;
    }

    /* the time cursor: a thin line and a dot on the curve */
    double tc = tView > tPlot ? tPlot : tView;
    float  cx = PX(tc), cy = PY(g->acc(cur));
    DrawLineEx((Vector2){ cx, pl.y }, (Vector2){ cx, pl.y + pl.height }, 1.0f, Fade(WHITE, 0.28f));
    DrawCircleV((Vector2){ cx, cy }, 10.0f, Fade(g->col, 0.22f));   /* halo */
    DrawCircleV((Vector2){ cx, cy }, 5.5f, C_PLOT_BG);
    DrawCircleV((Vector2){ cx, cy }, 4.0f, g->col);
    EndScissorMode();

    #undef PX
    #undef PY
}

/* ---------------------------------------------------------- the column */
void Graphs_Draw(const History *h, int count, double tReached, double tView,
                 Sample cur, const ViewOptions *v)
{
    /* The graph definitions use theme colours, which are not compile-time
     * constants, so the table is built here rather than as a static array. */
    const GraphDef defs[3] = {
        { "v (m/s) vs t",         AccV, C_NORM },
        { "a (m/s\xC2\xB2) vs t", AccA, { 255, 159, 67, 255 } },
        { "Friction f (N) vs t",  AccF, C_FRIC },
    };
    bool on[3] = { v->showGraphV, v->showGraphA, v->showGraphF };
    int  n = on[0] + on[1] + on[2];

    /* column background with a thin divider on its left edge */
    const int colX = SCREEN_W - RIGHT_W;
    DrawRectangle(colX, 0, RIGHT_W, SCREEN_H, C_PANEL);
    DrawRectangle(colX, 0, 1, SCREEN_H, C_BORDER);

    if (n == 0) {
        Text_Draw("Enable a graph in the panel", colX + 70, SCREEN_H / 2 - 8, 16, C_TEXT_MUTED);
        return;
    }

    /* the enabled graphs share the column height equally (each at most 300 px) */
    const float pad = 12.0f, gap = 10.0f;
    float hgt = (SCREEN_H - 2 * pad - (n - 1) * gap) / n;
    if (hgt > 300.0f) hgt = 300.0f;
    double tPlot = tReached > 2.0 ? tReached : 2.0;       /* x axis shows at least 2 s */

    float y = pad;
    for (int i = 0; i < 3; i++) {
        if (!on[i]) continue;
        DrawGraph((Rectangle){ colX + pad, y, RIGHT_W - 2 * pad, hgt }, h, count, &defs[i], tView, tPlot, &cur);
        y += hgt + gap;
    }
}

/* ------------------------------------------------------------------ HUD */
void HUD_Draw(Sample c, SimMode mode, const ViewOptions *v, Rectangle sceneRect)
{
    const int rows  = 4 + (v->showTime ? 1 : 0);          /* time is optional */
    const int cardW = 250;
    const int cardH = 112 + rows * 26;
    const int x0 = (int)sceneRect.x + 14, y0 = (int)sceneRect.y + 14;
    const int px = x0 + 14, right = x0 + cardW - 14;      /* content area */

    Theme_Card((Rectangle){ (float)x0, (float)y0, (float)cardW, (float)cardH }, 14,
               (Color){ 13, 20, 36, 255 }, C_BORDER);

    /* ---- state pill: green "AT REST" or amber "SLIDING" ---- */
    const Color sc = c.sliding ? C_WARN : C_GOOD;
    const char *state = c.sliding ? "SLIDING" : "AT REST";
    int pillW = Text_WidthBold(state, 14) + 40;
    Theme_Rounded((Rectangle){ (float)px, (float)(y0 + 14), (float)pillW, 26 }, 13, Fade(sc, 0.20f));
    DrawCircleV((Vector2){ (float)(px + 14), (float)(y0 + 27) }, 5.0f, sc);
    Text_DrawBold(state, px + 28, y0 + 19, 14, sc);

    /* ---- run mode on the right: RUNNING / PAUSED / STOPPED ---- */
    const char *ms = mode == SIM_RUNNING ? "RUNNING" : mode == SIM_PAUSED ? "PAUSED" : "STOPPED";
    Color mc = mode == SIM_RUNNING ? C_GOOD : mode == SIM_PAUSED ? C_WARN : C_TEXT_MUTED;
    Text_DrawBoldRight(ms, right, y0 + 21, 12, mc);

    /* ---- why: the physical reason for the current state ---- */
    const char *why = c.atWall ? "Pinned against the end of the ramp"
                    : c.sliding ? "Kinetic friction acts"
                    : "Static friction holds the block";
    Text_Draw(why, px, y0 + 48, 13, C_TEXT_DIM);
    DrawRectangle(px, y0 + 72, cardW - 28, 1, C_BORDER);  /* divider */

    /* ---- the readouts: label on the left, signed value on the right ---- */
    int y = y0 + 82;
    const int pitch = 26;
    if (v->showTime) {
        Text_Draw("Time", px, y + 3, 14, C_TEXT_DIM);
        Text_DrawRight(TextFormat("%.3f s", c.t), right, y, 17, C_TEXT);
        y += pitch;
    }
    Text_Draw("Position s", px, y + 3, 14, C_TEXT_DIM);
    Text_DrawRight(TextFormat("%+.3f m", c.s), right, y, 17, C_TEXT);
    y += pitch;

    Text_Draw("Velocity v", px, y + 3, 14, C_TEXT_DIM);
    Text_DrawRight(TextFormat("%+.3f m/s", c.v), right, y, 17, C_NORM);
    y += pitch;

    Text_Draw("Acceleration a", px, y + 3, 14, C_TEXT_DIM);
    Text_DrawRight(TextFormat("%+.3f m/s\xC2\xB2", c.a), right, y, 17, C_WARN);
    y += pitch;

    /* the label says which kind of friction the value is */
    Text_Draw(c.staticFric ? "Friction (static)" : "Friction (kinetic)", px, y + 3, 14, C_TEXT_DIM);
    Text_DrawRight(TextFormat("%+.3f N", c.fric), right, y, 17, C_FRIC);
    y += pitch;

    Text_Draw("+ is up the slope", px, y + 4, 12, C_TEXT_MUTED);
}
