/*
 * types.h - the shared contract between all modules.
 *
 * Every module includes this file and nothing else is shared between them, so
 * the data layout below is the "interface" of the whole program.
 *
 * Who owns what (the rule that keeps the modules independent):
 *   Params       written by ui.c            read by everyone else
 *   ViewOptions  written by ui.c            read by scene.c and graphs.c
 *   History      owned by sim.c             read-only for graphs.c
 *   Sample       produced by physics.c      stored and served by sim.c
 *
 * Sign convention used EVERYWHERE (physics, HUD, graphs, labels):
 *   Anything measured ALONG THE SLOPE is positive UP the slope.
 *   So a block sliding down has negative s, v and a, and the friction force that
 *   resists it is positive. The displacement s is measured from the start position.
 */
#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

/* ============================ Window layout (pixels) ======================
 *
 *   +-----------+--------------------------------+-----------+
 *   |           |             scene              |           |
 *   |   left    |  ramp, block, vectors, HUD     |  graphs   |
 *   |   panel   +--------------------------------+  column   |
 *   |           |         transport bar          |           |
 *   +-----------+--------------------------------+-----------+
 *
 * The scene rectangle (where the ramp is drawn) excludes the transport bar, so
 * mouse wheel zoom and drag-to-pan never trigger from the bar.
 */
#define SCREEN_W   1280
#define SCREEN_H   800
#define LEFT_W     320                        /* left panel: parameters, display options  */
#define RIGHT_W    380                        /* right column: the three graphs           */
#define BAR_H      110                        /* transport bar under the scene            */
#define SCENE_X    LEFT_W
#define SCENE_W    (SCREEN_W - LEFT_W - RIGHT_W)
#define SCENE_H    (SCREEN_H - BAR_H)

/* ======================= World / simulation constants ====================== */
#define RAMP_LENGTH   20.0     /* metres, measured along the slope                    */
#define BLOCK_SIZE    1.0      /* metres, side of the square block                    */
#define START_POS     10.0     /* metres along the slope: where the block starts      */
#define PHYS_DT       (1.0/240.0)   /* fixed physics step, in seconds                 */
#define T_MAX         60.0     /* the simulation clock stops here (seconds)           */

/* ================================== Data =================================== */

/* The physical parameters the user can change with the sliders. */
typedef struct {
    float angleDeg;   /* incline angle, 0..90 degrees                         */
    float mass;       /* kg                                                   */
    float g;          /* gravitational acceleration, m/s^2                    */
    float mu_s;       /* static friction coefficient                          */
    float mu_k;       /* kinetic friction coefficient (kept <= mu_s)          */
    float v0;         /* initial velocity along the slope, + is up the slope  */
} Params;

/* The complete state of the block at one instant. */
typedef struct {
    double t;          /* time since the start of the run, s                          */
    double s;          /* displacement from the start position, m (+ up the slope)    */
    double v;          /* velocity, m/s (+ up the slope)                              */
    double a;          /* acceleration, m/s^2 (+ up the slope)                        */
    double fric;       /* friction force, N (+ up the slope)                          */
    bool   sliding;    /* true: moving (or about to move); false: at rest             */
    bool   staticFric; /* true: fric is STATIC friction, false: it is KINETIC         */
    bool   atWall;     /* true: the block is pinned against an end of the ramp        */
} Sample;

/* Every fixed-step sample of the current run, from t = 0. Owned by sim.c. */
typedef struct {
    Sample *data;
    int     count;      /* samples computed so far */
    int     capacity;   /* allocated samples       */
} History;

/* Purely visual switches and scales. Changing these never affects the physics. */
typedef struct {
    bool  showParallelPerp;   /* draw mg sin(theta) and mg cos(theta)            */
    bool  showXY;             /* draw the x / y components of every vector       */
    bool  showCritical;       /* draw the critical-angle ray in the scene        */
    bool  showTime;           /* show t in the HUD                               */
    bool  showGraphV, showGraphA, showGraphF;   /* which graphs are visible      */
    float scaleGravity, scaleNormal, scaleFriction;  /* arrow length multipliers */
    float zoom;               /* camera zoom, 1 = the "fit whole ramp" view      */
    float panX, panY;         /* camera pan in screen pixels                     */
} ViewOptions;

/* What the clock is doing. */
typedef enum { SIM_STOPPED, SIM_RUNNING, SIM_PAUSED } SimMode;

#endif /* TYPES_H */
