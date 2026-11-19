/*
 * physics.h - Module 1: pure Newtonian mechanics for a block on an incline.
 *
 * No Raylib or Raygui here, so this module can be compiled and tested from a
 * console (see tests/test_physics.c). Everything is in double precision.
 */
#ifndef PHYSICS_H
#define PHYSICS_H

#include "types.h"

/* A breakdown of the forces on the block, for drawing and for the tests.
 * All values are in newtons. Slope components are positive UP the slope. */
typedef struct {
    double weight;     /* m g, straight down                                       */
    double gPar;       /* m g sin(theta): gravity component DOWN the slope         */
    double gPerp;      /* m g cos(theta): gravity component into the surface       */
    double normal;     /* N, perpendicular to the surface (never negative)         */
    double friction;   /* signed friction force, + is up the slope                 */
    double net;        /* net force along the slope = -gPar + friction             */
} Forces;

/* The state at t = 0: position 0 (the start mark) and velocity v0. */
Sample Physics_Initial(const Params *p);

/* Advance a sample by dt seconds. dt may be any positive value: the stop and
 * break-away logic is exact, so one large step equals many small ones. */
Sample Physics_Step(Sample prev, const Params *p, double dt);

/* The force breakdown for a given sample. */
Forces Physics_Forces(const Sample *s, const Params *p);

/* The critical angle for a friction coefficient: atan(mu), in degrees.
 * With mu = mu_s this is the steepest angle at which a block at rest still sticks.
 * With mu = mu_k it is the angle at which a sliding block moves at constant speed. */
double Physics_CriticalAngle(double mu);

/* Displacement limits of the block centre (it stays on the ramp), measured
 * from the start position. */
double Physics_MinS(void);
double Physics_MaxS(void);

#endif /* PHYSICS_H */
