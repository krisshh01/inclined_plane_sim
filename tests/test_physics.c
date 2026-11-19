/* Console tests for Module 1 (no Raylib needed). Build: make test */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "../physics.h"

static int fails = 0, checks = 0;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEAR(a, b, tol) (fabs((a) - (b)) <= (tol))

static Params mk(float ang, float m, float mus, float muk, float v0)
{
    Params p = { ang, m, 9.81f, mus, muk, v0 };
    return p;
}

/* Run to time T with the fixed step; return final sample. */
static Sample run(const Params *p, double T)
{
    Sample s = Physics_Initial(p);
    long n = (long)llround(T / PHYS_DT);
    for (long i = 0; i < n; i++) s = Physics_Step(s, p, PHYS_DT);
    return s;
}

int main(void)
{
    const double G = (double)9.81f;   /* Params stores g as a float */
    printf("Physics tests\n");

    /* 1. Sticks when tan(th) < mu_s */
    {
        Params p = mk(25, 5, 0.6f, 0.4f, 0);               /* tan25 = 0.466 < 0.6 */
        Sample s = run(&p, 5.0);
        CHECK(!s.sliding && s.staticFric, "should stick (state)");
        CHECK(NEAR(s.s, 0, 1e-12) && NEAR(s.v, 0, 1e-12), "should not move");
        CHECK(NEAR(s.fric, 5 * G * sin(25 * M_PI / 180), 1e-6), "static friction = m g sin(th), got %f", s.fric);
        CHECK(NEAR(s.a, 0, 1e-12), "a = 0 when stuck");
    }

    /* 2. Boundary at atan(mu_s): just below sticks, just above slides */
    {
        double th = atan(0.5) * 180 / M_PI;
        Params lo = mk((float)(th - 0.05), 3, 0.5f, 0.3f, 0);
        Params hi = mk((float)(th + 0.05), 3, 0.5f, 0.3f, 0);
        CHECK(!run(&lo, 1.0).sliding, "just below atan(mu_s) must stick");
        CHECK(run(&hi, 1.0).sliding,  "just above atan(mu_s) must slide");
    }

    /* 3. Slides down: a = g (sin - muk cos) (negative = down) */
    {
        Params p = mk(35, 4, 0.4f, 0.3f, 0);
        double th = 35 * M_PI / 180;
        double aExp = -G * (sin(th) - 0.3 * cos(th));
        Sample s = Physics_Initial(&p);
        CHECK(s.sliding && !s.staticFric, "sliding at t=0 (kinetic)");
        CHECK(NEAR(s.a, aExp, 1e-6), "a = %f expected %f", s.a, aExp);
        Sample s1 = run(&p, 1.0);
        CHECK(NEAR(s1.v, aExp * 1.0, 1e-6), "v(1s) = %f expected %f", s1.v, aExp);
        CHECK(NEAR(s1.s, 0.5 * aExp, 1e-6), "s(1s) = %f expected %f", s1.s, 0.5 * aExp);
        CHECK(NEAR(s1.fric, 0.3 * 4 * G * cos(th), 1e-6), "kinetic friction magnitude/sign");
    }

    /* 4. Up the slope, then sticks at apex when tan(th) <= mu_s */
    {
        Params p = mk(20, 2, 0.6f, 0.4f, 4.0f);            /* tan20 = 0.364 < 0.6 */
        double th = 20 * M_PI / 180;
        double aUp = -G * (sin(th) + 0.4 * cos(th));
        Sample s0 = Physics_Initial(&p);
        CHECK(NEAR(s0.a, aUp, 1e-6), "decel up the slope = %f expected %f", s0.a, aUp);
        double tStop = -4.0 / aUp;
        Sample s = run(&p, tStop + 0.5);
        CHECK(NEAR(s.v, 0, 1e-12), "v == 0 after stopping, got %g", s.v);
        CHECK(!s.sliding && s.staticFric, "sticks at apex");
        CHECK(NEAR(s.s, 4.0 * 4.0 / (2 * -aUp), 2e-3), "apex distance %f", s.s);
        Sample s2 = run(&p, tStop + 3.0);
        CHECK(NEAR(s2.s, s.s, 1e-12), "stays at apex (no drift)");
    }

    /* 5. Up the slope, then reverses when tan(th) > mu_s */
    {
        Params p = mk(40, 2, 0.5f, 0.3f, 3.0f);            /* tan40 = 0.839 > 0.5 */
        double th = 40 * M_PI / 180;
        double aUp = -G * (sin(th) + 0.3 * cos(th));
        double aDn = -G * (sin(th) - 0.3 * cos(th));
        double tStop = -3.0 / aUp;
        Sample before = run(&p, tStop - 0.02);
        CHECK(before.v > 0, "still moving up just before apex");
        Sample after = run(&p, tStop + 0.5);
        CHECK(after.v < 0, "reversed and sliding down, v=%f", after.v);
        CHECK(NEAR(after.v, aDn * (0.5), 5e-3), "v after reversal = %f expected %f", after.v, aDn * 0.5);
        CHECK(after.sliding && !after.staticFric, "kinetic friction after reversal");
        CHECK(after.fric > 0, "kinetic friction points up-slope while sliding down");
    }

    /* 6. Moving down but kinetic friction exceeds gravity pull: decelerates, sticks */
    {
        Params p = mk(15, 2, 0.8f, 0.7f, -2.0f);           /* tan15=0.268 < 0.7 */
        Sample s = run(&p, 3.0);
        CHECK(NEAR(s.v, 0, 1e-12), "stops");
        CHECK(!s.sliding && s.staticFric, "sticks after stopping");
    }

    /* 7. 90 degrees: free fall, a = -g, friction 0 */
    {
        Params p = mk(90, 3, 0.9f, 0.8f, 0);
        Sample s = Physics_Initial(&p);
        CHECK(NEAR(s.a, -G, 1e-6), "a = -g at 90 deg, got %f", s.a);
        CHECK(NEAR(s.fric, 0, 1e-12), "no friction at 90 deg");
    }

    /* 8. 0 degrees: at rest stays; with v0 decelerates at mu_k g */
    {
        Params p0 = mk(0, 3, 0.5f, 0.3f, 0);
        CHECK(!Physics_Initial(&p0).sliding, "flat, v0=0: at rest");
        Params p = mk(0, 3, 0.5f, 0.3f, 2.0f);
        Sample s = Physics_Initial(&p);
        CHECK(NEAR(s.a, -0.3 * G, 1e-6), "flat deceleration %f", s.a);
        Sample e = run(&p, 5.0);
        CHECK(NEAR(e.v, 0, 1e-12) && !e.sliding, "flat block stops and rests");
        CHECK(NEAR(e.s, 4.0 / (2 * 0.3 * G), 2e-3), "flat stopping distance %f", e.s);
    }

    /* 9. mu_k > mu_s is clamped (same result as mu_k = mu_s) */
    {
        Params a = mk(35, 2, 0.3f, 0.9f, 0), b = mk(35, 2, 0.3f, 0.3f, 0);
        CHECK(NEAR(run(&a, 1).s, run(&b, 1).s, 1e-12), "mu_k clamped to mu_s");
    }

    /* 10. Ramp ends: no escape, bottom end reported pinned */
    {
        Params p = mk(60, 2, 0.1f, 0.05f, 0);
        Sample s = run(&p, 10.0);
        CHECK(NEAR(s.s, Physics_MinS(), 1e-12), "pinned at bottom, s=%f", s.s);
        CHECK(s.atWall && !s.sliding && NEAR(s.v, 0, 1e-12), "at rest against wall");
        Params q = mk(5, 2, 0.05f, 0.02f, 8.0f);           /* shoots up the flat-ish ramp */
        Sample t = run(&q, 10.0);
        CHECK(t.s <= Physics_MaxS() + 1e-12, "never leaves the top end, s=%f", t.s);
    }

    /* 11. Step-size independence (exact kinematics): 1 big step == many small */
    {
        Params p = mk(35, 4, 0.4f, 0.3f, 0);
        Sample a = Physics_Initial(&p), b = a;
        for (int i = 0; i < 240; i++) a = Physics_Step(a, &p, PHYS_DT);
        b = Physics_Step(b, &p, 1.0);
        CHECK(NEAR(a.s, b.s, 1e-9) && NEAR(a.v, b.v, 1e-9), "1 x 1s step == 240 x 1/240s steps");
        Params q = mk(40, 2, 0.5f, 0.3f, 3.0f);           /* includes the reversal */
        Sample c = Physics_Initial(&q), d = c;
        for (int i = 0; i < 480; i++) c = Physics_Step(c, &q, PHYS_DT);
        d = Physics_Step(d, &q, 2.0);
        CHECK(NEAR(c.s, d.s, 1e-9) && NEAR(c.v, d.v, 1e-9), "same across a reversal");
    }

    /* 12. Newton: m a = sum of forces along slope, in every state */
    {
        Params ps[3] = { mk(35, 4, 0.4f, 0.3f, 0), mk(20, 2, 0.6f, 0.4f, 4.0f), mk(25, 5, 0.6f, 0.4f, 0) };
        for (int k = 0; k < 3; k++) {
            Sample s = Physics_Initial(&ps[k]);
            for (int i = 0; i < 2400; i++) {
                s = Physics_Step(s, &ps[k], PHYS_DT);
                if (s.atWall) continue;   /* the wall adds a force the model doesn't list */
                Forces f = Physics_Forces(&s, &ps[k]);
                if (!NEAR(f.net, ps[k].mass * s.a, 1e-9)) { CHECK(0, "m a != net force (case %d, i=%d)", k, i); break; }
            }
            CHECK(1, "ok");
        }
    }

    /* 13. Critical angle = atan(mu_s): just below it the block sticks, just above it slides */
    {
        CHECK(NEAR(Physics_CriticalAngle(1.0), 45.0, 1e-9), "atan(1) = 45 deg");
        CHECK(NEAR(Physics_CriticalAngle(0.0), 0.0, 1e-12), "mu 0 -> 0 deg");
        CHECK(Physics_CriticalAngle(1e6) < 90.0, "huge mu stays below 90 deg");
        double mus[8] = { 0.1, 0.25, 0.4, 0.6, 0.8, 1.0, 1.5, 2.0 };
        for (int i = 0; i < 8; i++) {
            double thc = Physics_CriticalAngle(mus[i]);
            Params lo = mk((float)(thc - 0.05), 3, (float)mus[i], (float)mus[i] * 0.7f, 0);
            Params hi = mk((float)(thc + 0.05), 3, (float)mus[i], (float)mus[i] * 0.7f, 0);
            Sample a = run(&lo, 1.0), b = run(&hi, 1.0);
            CHECK(!a.sliding && NEAR(a.s, 0.0, 1e-12), "mu_s=%.2f: sticks below theta_c", mus[i]);
            CHECK(b.sliding && b.s < 0.0, "mu_s=%.2f: slides above theta_c", mus[i]);
        }
    }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
