/*
 * sim.h - Module 2: simulation control and history.
 *
 * TIME IS THE MASTER VARIABLE. The "view time" tView is a cursor into a history
 * of fixed-step samples that is computed lazily and cached. Because the physics
 * is deterministic, jumping or stepping backwards gives exactly the values a
 * fresh run to that time would give, and the animation, HUD and graphs always
 * agree because they all read from the same place.
 */
#ifndef SIM_H
#define SIM_H

#include "types.h"

/* Allocate the history for a run with these parameters. Call once at start-up. */
void Sim_Init(const Params *p);
void Sim_Shutdown(void);

/* The parameters changed: throw the history away and restart at t = 0.
 * A running simulation keeps running; otherwise it returns to STOPPED. */
void Sim_SetParams(const Params *p);

void Sim_Start(void);                 /* STOPPED or PAUSED -> RUNNING          */
void Sim_Pause(void);                 /* RUNNING -> PAUSED                     */
void Sim_Reset(void);                 /* back to t = 0, STOPPED                */

/* Call once per frame with the frame time; advances the clock while RUNNING. */
void Sim_Update(double frameDt);

void Sim_StepBy(double dt);           /* move the clock by +/- dt (pauses)     */
void Sim_JumpTo(double t);            /* go to an absolute time (clamped, pauses) */

Sample  Sim_Current(void);            /* the block's state at the view time    */
double  Sim_Time(void);               /* the view time, seconds                */
double  Sim_ReachedTime(void);        /* furthest time reached since the reset */
SimMode Sim_Mode(void);

const History *Sim_History(void);     /* for the graphs                        */
int     Sim_PlotCount(void);          /* number of samples up to Sim_ReachedTime() */

#endif /* SIM_H */
