/* Console tests for Module 2 (sim control + history). No Raylib needed. */
#include <stdio.h>
#include <math.h>
#include "../physics.h"
#include "../sim.h"

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEAR(a, b, tol) (fabs((a) - (b)) <= (tol))

int main(void)
{
    printf("Sim tests\n");
    Params p = { 40, 2, 9.81f, 0.5f, 0.3f, 3.0f };       /* up, reverses, slides down */
    Sim_Init(&p);

    /* initial state */
    CHECK(Sim_Mode() == SIM_STOPPED && Sim_Time() == 0.0, "starts stopped at t=0");
    CHECK(NEAR(Sim_Current().v, 3.0, 1e-12), "initial v = v0");
    CHECK(Sim_PlotCount() == 1, "one sample at start");

    /* an arbitrary (non-multiple-of-dt) time equals ONE exact physics step */
    Sim_JumpTo(1.2345);
    Sample a = Sim_Current();
    Sample ref = Physics_Step(Physics_Initial(&p), &p, 1.2345);
    CHECK(NEAR(a.s, ref.s, 1e-9) && NEAR(a.v, ref.v, 1e-9), "JumpTo(1.2345) matches exact kinematics");
    CHECK(NEAR(a.t, 1.2345, 1e-12), "reported time is the requested time");
    CHECK(Sim_Mode() == SIM_PAUSED, "jumping pauses");

    /* step down from 3 s to 1 s == fresh jump to 1 s (and == earlier history) */
    Sim_JumpTo(3.0);
    Sample at3 = Sim_Current();
    Sim_StepBy(-2.0);
    Sample back = Sim_Current();
    Sim_Reset();
    Sim_JumpTo(1.0);
    Sample fresh = Sim_Current();
    CHECK(NEAR(back.s, fresh.s, 1e-12) && NEAR(back.v, fresh.v, 1e-12) &&
          NEAR(back.a, fresh.a, 1e-12) && NEAR(back.fric, fresh.fric, 1e-12),
          "step down 3s->1s identical to a fresh run to 1s");
    CHECK(fresh.v < 0, "sanity: block is sliding down at 1 s");
    CHECK(at3.atWall && !at3.sliding && NEAR(at3.s, Physics_MinS(), 1e-12),
          "sanity: block has reached the bottom end and rests there at 3 s");

    /* the full curve stays available after stepping back */
    Sim_JumpTo(3.0);
    Sim_StepBy(-2.0);
    CHECK(NEAR(Sim_ReachedTime(), 3.0, 1e-12), "reached time remembers the furthest point");
    CHECK(Sim_PlotCount() == (int)floor(3.0 / PHYS_DT + 1e-9) + 1, "plot count covers 0..3 s");

    /* clamping */
    Sim_JumpTo(1e9);
    CHECK(NEAR(Sim_Time(), T_MAX, 1e-12), "huge time clamps to T_MAX");
    Sim_JumpTo(-5.0);
    CHECK(Sim_Time() == 0.0, "negative time clamps to 0");
    Sim_Reset();
    Sim_StepBy(-0.1);
    CHECK(Sim_Time() == 0.0 && Sim_Mode() == SIM_STOPPED, "stepping down at t=0 stays stopped");

    /* running: advances, and auto-pauses at T_MAX */
    Sim_Reset();
    Sim_Start();
    Sim_Update(1.0 / 60.0);
    CHECK(Sim_Mode() == SIM_RUNNING && NEAR(Sim_Time(), 1.0 / 60.0, 1e-12), "running advances the clock");
    Sim_Pause();
    Sim_Update(1.0);
    CHECK(NEAR(Sim_Time(), 1.0 / 60.0, 1e-12), "paused does not advance");
    Sim_Start();
    for (int i = 0; i < 700; i++) Sim_Update(0.1);
    CHECK(Sim_Mode() == SIM_PAUSED && NEAR(Sim_Time(), T_MAX, 1e-12), "auto-pauses at T_MAX");
    Sim_Start();
    CHECK(Sim_Mode() == SIM_RUNNING && Sim_Time() == 0.0, "Start after the end restarts from 0");

    /* huge frame stall is capped */
    Sim_Reset();
    Sim_Start();
    Sim_Update(5.0);
    CHECK(NEAR(Sim_Time(), 0.1, 1e-12), "a stalled frame advances at most 0.1 s");

    /* parameter change: history discarded, t = 0, running state kept */
    Sim_JumpTo(2.0);
    Sim_Start();
    Params q = p; q.angleDeg = 10; q.v0 = 0;                /* now it just sticks */
    Sim_SetParams(&q);
    CHECK(Sim_Time() == 0.0 && Sim_ReachedTime() == 0.0 && Sim_PlotCount() == 1, "param change resets time and history");
    CHECK(Sim_Mode() == SIM_RUNNING, "running stays running after a slider change");
    Sim_Pause();
    Sim_SetParams(&q);
    CHECK(Sim_Mode() == SIM_STOPPED, "paused -> stopped after a slider change");
    Sim_JumpTo(5.0);
    Sample s5 = Sim_Current();
    CHECK(!s5.sliding && NEAR(s5.s, 0.0, 1e-12), "new params are in effect (block sticks)");

    Sim_Shutdown();
    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
