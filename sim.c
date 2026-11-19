/*
 * sim.c - Module 2: simulation control and history.
 *
 * This module owns TIME. It never does physics itself: it asks physics.c for
 * the next sample and stores the answer. Everything else in the program reads
 * the current state through Sim_Current().
 *
 * Two clocks are kept:
 *   tView     the time being shown right now (the "cursor")
 *   tReached  the furthest time shown since the last reset; the graphs plot the
 *             history up to this, so stepping back in time does not erase the curve.
 */
#include "sim.h"
#include "physics.h"
#include <math.h>
#include <stdlib.h>

#define MAX_FRAME_DT 0.1     /* a stalled frame (window drag, breakpoint) may advance the clock by at most this */

static Params  P;                       /* parameters of the current run             */
static History H;                       /* samples at t = 0, dt, 2 dt, ...           */
static SimMode mode = SIM_STOPPED;
static double  tView = 0.0;
static double  tReached = 0.0;

/* Index of the history sample at or just before time t. The tiny epsilon keeps
 * exact multiples of PHYS_DT (e.g. t = 1.0) from falling into the previous slot
 * because of floating-point rounding. */
static int IndexOf(double t) { return (int)floor(t / PHYS_DT + 1e-9); }

/* Start a new history containing only the initial state. */
static void Hist_Reset(void)
{
    H.data[0] = Physics_Initial(&P);
    H.count = 1;
}

/* Make sure samples 0..idx exist, computing any that are missing. */
static void Hist_Ensure(int idx)
{
    if (idx >= H.capacity) idx = H.capacity - 1;
    while (H.count <= idx) {
        Sample n = Physics_Step(H.data[H.count - 1], &P, PHYS_DT);
        n.t = (double)H.count * PHYS_DT;        /* exact time, no accumulated error */
        H.data[H.count++] = n;
    }
}

/* Move the cursor (clamped to [0, T_MAX]) and remember how far we have been. */
static void SetTime(double t)
{
    if (t < 0.0)   t = 0.0;
    if (t > T_MAX) t = T_MAX;
    tView = t;
    if (tView > tReached) tReached = tView;
}

void Sim_Init(const Params *p)
{
    P = *p;
    H.capacity = (int)(T_MAX / PHYS_DT) + 2;    /* room for the whole 60 s run */
    H.data = (Sample *)malloc((size_t)H.capacity * sizeof(Sample));
    Hist_Reset();
    mode = SIM_STOPPED;
    tView = tReached = 0.0;
}

void Sim_Shutdown(void)
{
    free(H.data);
    H.data = NULL;
    H.count = H.capacity = 0;
}

void Sim_SetParams(const Params *p)
{
    P = *p;
    Hist_Reset();                               /* old samples no longer match the new physics */
    tView = tReached = 0.0;
    if (mode != SIM_RUNNING) mode = SIM_STOPPED;
}

void Sim_Start(void)
{
    /* After the clock has hit T_MAX, Start begins a fresh run instead of doing nothing. */
    if (tView >= T_MAX - 1e-9) tView = tReached = 0.0;
    mode = SIM_RUNNING;
}

void Sim_Pause(void)
{
    if (mode == SIM_RUNNING) mode = SIM_PAUSED;
}

void Sim_Reset(void)
{
    tView = tReached = 0.0;
    mode = SIM_STOPPED;
}

void Sim_Update(double frameDt)
{
    if (mode != SIM_RUNNING) return;
    if (frameDt > MAX_FRAME_DT) frameDt = MAX_FRAME_DT;
    SetTime(tView + frameDt);
    if (tView >= T_MAX) mode = SIM_PAUSED;      /* run finished */
}

void Sim_StepBy(double dt) { Sim_JumpTo(tView + dt); }

void Sim_JumpTo(double t)
{
    SetTime(t);
    /* Stepping or typing a time always pauses (a jump back to exactly 0 from a
     * stopped state leaves the mode as STOPPED). */
    if (mode == SIM_RUNNING || tView > 0.0) mode = SIM_PAUSED;
}

Sample Sim_Current(void)
{
    int i = IndexOf(tView);
    Hist_Ensure(i);
    Sample cur = H.data[i];

    /* tView usually falls between two samples (the frame time is not a multiple
     * of 1/240 s). Physics is exact for any dt, so finish with one partial step. */
    double frac = tView - (double)i * PHYS_DT;
    if (frac > 1e-9) cur = Physics_Step(cur, &P, frac);
    cur.t = tView;
    return cur;
}

double  Sim_Time(void)        { return tView; }
double  Sim_ReachedTime(void) { return tReached; }
SimMode Sim_Mode(void)        { return mode; }

const History *Sim_History(void) { return &H; }

int Sim_PlotCount(void)
{
    int idx = IndexOf(tReached);
    Hist_Ensure(idx);                           /* the graphs need samples up to here */
    return (idx < H.count ? idx : H.count - 1) + 1;
}
