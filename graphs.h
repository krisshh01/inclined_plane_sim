/*
 * graphs.h - Module 5: the graphs (v-t, a-t, friction-t) and the live HUD readout.
 *
 * Both are read-only consumers of the simulation history: they never change
 * the simulation, they only draw what Module 2 hands them.
 */
#ifndef GRAPHS_H
#define GRAPHS_H

#include "raylib.h"
#include "types.h"

/* Draws the right-hand column of graphs.
 *   h, count  : the history and how many samples of it to plot
 *   tReached  : furthest time reached (sets the x axis)
 *   tView     : the time being shown (where the cursor dot goes)
 *   cur       : the state at tView (for the "now" values) */
void Graphs_Draw(const History *h, int count, double tReached, double tView,
                 Sample cur, const ViewOptions *v);

/* The status card in the top-left corner of the scene: state (At Rest / Sliding),
 * run mode, and the signed values of t, s, v, a and friction. */
void HUD_Draw(Sample cur, SimMode mode, const ViewOptions *v, Rectangle sceneRect);

#endif /* GRAPHS_H */
