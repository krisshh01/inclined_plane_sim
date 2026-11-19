/*
 * physics.c - Module 1: block-on-incline physics with static and kinetic friction.
 *
 * MODEL (one axis along the slope, UP the slope is positive)
 * ----------------------------------------------------------
 *   Fpar  = -m g sin(theta)     gravity along the slope (always <= 0)
 *   N     =  m g cos(theta)     normal force (0 at 90 degrees)
 *   fsMax =  mu_s N             largest static friction the surface can supply
 *   fk    =  mu_k N             kinetic friction magnitude
 *
 *   Moving (|v| > eps):  friction = -sign(v) fk,   a = (Fpar + friction) / m
 *   At rest:
 *       |Fpar| <= fsMax  ->  the block STICKS:   a = 0, v = 0, and the static
 *                            friction equals exactly what it has to cancel (-Fpar)
 *       |Fpar| >  fsMax  ->  the block BREAKS AWAY and slides down the slope,
 *                            now with kinetic friction fk
 *
 * The condition |Fpar| <= fsMax simplifies to tan(theta) <= mu_s, which is where
 * the "critical angle" atan(mu_s) comes from (see Physics_CriticalAngle).
 *
 * HOW A STEP WORKS
 * ----------------
 * While nothing changes state, the acceleration is constant, so a step uses the
 * exact formulas  s += v dt + a dt^2 / 2  and  v += a dt  (no Euler error, and
 * therefore no jitter at the stick/slide threshold).
 *
 * The hard part is the instant the block stops. If a deceleration would carry v
 * through zero inside a step, the step is split at  t_stop = -v / a:
 *   - advance exactly t_stop and set v = 0 (never let v flip sign),
 *   - then re-evaluate the rest of the step as "at rest": either the block
 *     sticks where it stopped, or it immediately slides back down.
 *
 * The ramp has finite length: the block stops dead at either end (no bounce).
 *
 * The (a, friction, state) stored in a returned Sample are the INSTANTANEOUS
 * values at that sample's time, evaluated from its position and velocity.
 */
#include "physics.h"
#include <math.h>

/* ----------------------------------------------------------------- constants */
#define V_EPS     1e-6                 /* |v| below this counts as "at rest"       */
#define WALL_EPS  1e-9                 /* tolerance when comparing s to a ramp end */
#define DEG2RAD   0.017453292519943295 /* pi / 180                                 */

/* Quantities that depend only on Params, computed once per call. */
typedef struct {
    double m;        /* mass                                  */
    double Fpar;     /* gravity along the slope (<= 0)        */
    double N;        /* normal force                          */
    double fsMax;    /* maximum static friction               */
    double fk;       /* kinetic friction magnitude            */
    double sMin;     /* lowest allowed displacement (bottom end of the ramp) */
    double sMax;     /* highest allowed displacement (top end of the ramp)   */
} Derived;

/* -1, 0 or +1 */
static double sgn(double x) { return (x > 0.0) - (x < 0.0); }

/* The block centre may travel until its edge touches either end of the ramp. */
double Physics_MinS(void) { return BLOCK_SIZE / 2.0 - START_POS; }
double Physics_MaxS(void) { return RAMP_LENGTH - BLOCK_SIZE / 2.0 - START_POS; }

/* Critical angle for a friction coefficient: tan(theta_c) = mu, so theta_c = atan(mu). */
double Physics_CriticalAngle(double mu)
{
    if (mu < 0.0) mu = 0.0;
    return atan(mu) / DEG2RAD;
}

/* Turn the user-facing parameters into the numbers the maths needs. */
static Derived derive(const Params *p)
{
    Derived d;
    double th  = (double)p->angleDeg * DEG2RAD;
    double mus = p->mu_s, muk = p->mu_k;
    if (muk > mus) muk = mus;                 /* mu_k > mu_s is unphysical: clamp */

    d.m    = (double)p->mass;
    d.Fpar = -d.m * (double)p->g * sin(th);
    /* cos(90 deg) is ~6e-17, not 0, so special-case it: friction must vanish exactly. */
    d.N    = (p->angleDeg >= 90.0f) ? 0.0 : d.m * (double)p->g * cos(th);
    if (d.N < 0.0) d.N = 0.0;
    d.fsMax = mus * d.N;
    d.fk    = muk * d.N;
    d.sMin  = Physics_MinS();
    d.sMax  = Physics_MaxS();
    return d;
}

/*
 * Fill in a, fric and the state flags of *out for the given position s and
 * velocity v. This is the single place that decides "stuck or sliding", so the
 * HUD, graphs and arrows can never disagree with the stepping code.
 */
static void evaluate(const Derived *d, double s, double v, Sample *out)
{
    out->atWall = false;

    /* Case 1: moving. Kinetic friction opposes the direction of motion. */
    if (fabs(v) > V_EPS) {
        double dir = sgn(v);
        out->fric       = -dir * d->fk;
        out->a          = (d->Fpar + out->fric) / d->m;
        out->sliding    = true;
        out->staticFric = false;
        return;
    }

    /* Case 2: at rest and static friction is strong enough. The block sticks and
     * friction takes exactly the value needed to cancel gravity (not fsMax). */
    if (fabs(d->Fpar) <= d->fsMax) {
        out->fric       = -d->Fpar;
        out->a          = 0.0;
        out->sliding    = false;
        out->staticFric = true;
        return;
    }

    /* Case 3: at rest but gravity wins. The block wants to slide down the slope
     * (Fpar < 0), unless it is already pressed against the bottom end. */
    double dir = sgn(d->Fpar);
    bool pinned = (dir < 0 && s <= d->sMin + WALL_EPS) ||
                  (dir > 0 && s >= d->sMax - WALL_EPS);
    if (pinned) {
        /* The end of the ramp (plus friction) holds it: report "at rest, at the wall". */
        out->fric       = -dir * d->fsMax;
        out->a          = 0.0;
        out->sliding    = false;
        out->staticFric = true;
        out->atWall     = true;
        return;
    }

    /* Break-away: v is still 0 at this exact instant, but the block is sliding. */
    out->fric       = -dir * d->fk;
    out->a          = (d->Fpar + out->fric) / d->m;
    out->sliding    = true;
    out->staticFric = false;
}

Sample Physics_Initial(const Params *p)
{
    Derived d = derive(p);
    Sample s = {0};                           /* t = 0, s = 0, everything else cleared */
    s.v = ((double)p->v0 > V_EPS || (double)p->v0 < -V_EPS) ? (double)p->v0 : 0.0;
    evaluate(&d, s.s, s.v, &s);
    return s;
}

Sample Physics_Step(Sample prev, const Params *p, double dt)
{
    Derived d = derive(p);
    double s   = prev.s;
    double v   = (fabs(prev.v) <= V_EPS) ? 0.0 : prev.v;
    double rem = dt;                          /* time left to simulate in this step */

    /* A step can contain up to a few phases (e.g. decelerate -> stop -> slide back),
     * so loop until the whole dt is used. 4 iterations is more than ever needed. */
    for (int guard = 0; guard < 4 && rem > 0.0; guard++) {

        if (v == 0.0) {
            /* ---- starting from rest ---- */
            if (fabs(d.Fpar) <= d.fsMax) break;                    /* sticks: nothing moves */
            double dir = sgn(d.Fpar);
            if ((dir < 0 && s <= d.sMin + WALL_EPS) ||
                (dir > 0 && s >= d.sMax - WALL_EPS)) break;        /* pinned at a ramp end  */
            double a = (d.Fpar - dir * d.fk) / d.m;                /* break away            */
            s  += 0.5 * a * rem * rem;
            v   = a * rem;
            rem = 0.0;
        } else {
            /* ---- already moving ---- */
            double dir = sgn(v);
            double a   = (d.Fpar - dir * d.fk) / d.m;              /* kinetic friction      */
            double vn  = v + a * rem;                              /* velocity at step end  */
            if (vn * v < 0.0) {
                /* v would change sign: stop EXACTLY at t_stop and re-evaluate the rest. */
                double ts = -v / a;
                s   += v * ts + 0.5 * a * ts * ts;
                v    = 0.0;
                rem -= ts;
                if (rem < 0.0) rem = 0.0;
            } else {
                s  += v * rem + 0.5 * a * rem * rem;
                v   = vn;
                rem = 0.0;
            }
        }

        /* The ramp is finite: the block stops dead at either end (no bounce). */
        if (s >= d.sMax) { s = d.sMax; v = 0.0; break; }
        if (s <= d.sMin) { s = d.sMin; v = 0.0; break; }
    }

    if (fabs(v) <= V_EPS) v = 0.0;

    Sample n = prev;
    n.t = prev.t + dt;
    n.s = s;
    n.v = v;
    evaluate(&d, s, v, &n);                   /* a, friction and flags at the new instant */
    return n;
}

Forces Physics_Forces(const Sample *s, const Params *p)
{
    Derived d = derive(p);
    Forces f;
    f.weight   = d.m * (double)p->g;
    f.gPar     = -d.Fpar;                     /* magnitude of the down-slope component */
    f.gPerp    = d.N;                         /* equals the normal force on a flat surface */
    f.normal   = d.N;
    f.friction = s->fric;
    f.net      = d.Fpar + s->fric;            /* Newton: m a = net  (when not at a wall) */
    return f;
}
