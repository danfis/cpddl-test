#include "test.h"
#include "context.h"
#include <assert.h>
#include <pddl/hm_heur.h>
#include <pddl/hmax.h>

/*
 * Verify that h^2 (dynamic programming) is admissible: its value for every
 * intermediate state along an optimal plan must not exceed the remaining
 * plan cost.  Also verify that h^2 >= h^max (h^2 dominates h^max).
 */
TEST(h2_heur_dyn_prog_admissible, fdr)
{
    if (C.optimal_cost < 0)
        return;
    if (C.plan_file_path[0] == '\x0')
        return;

    pddl_plan_file_strips_t plan;
    int ret = pddlPlanFileStripsInit(&plan, &C.strips, C.plan_file_path, &C.err);
    if (ret < 0)
        return;

    pddl_hmax_t hmax;
    pddlHMaxInitStrips(&hmax, &C.strips);

    int cost = plan.cost;
    for (int i = 0; i < plan.state_size; ++i){
        int h2 = pddlHMHeurDynProg(2, &C.strips, &plan.state[i], &C.err);
        int hm = pddlHMaxStrips(&hmax, &plan.state[i]);

        // h^2 must be admissible.
        assert(h2 <= cost);

        // h^2 dominates h^max.
        assert(h2 >= hm);

        if (i < pddlIArrSize(&plan.op))
            cost -= C.strips.op.op[pddlIArrGet(&plan.op, i)]->cost;
    }

    pddlHMaxFree(&hmax);
    pddlPlanFileStripsFree(&plan);
}

/*
 * Compute h^2 from the initial STRIPS state and print the result.
 * This gives a regression baseline for all tasks regardless of plan files.
 */
TEST(h2_heur_dyn_prog_init_state, strips)
{
    int h2 = pddlHMHeurDynProg(2, &C.strips, &C.strips.init, &C.err);
    fprintf(stdout, "h^2(I) = %d\n", h2);

    if (C.optimal_cost >= 0)
        assert(h2 <= C.optimal_cost);

    pddl_hmax_t hmax;
    pddlHMaxInitStrips(&hmax, &C.strips);
    int hm = pddlHMaxStrips(&hmax, &C.strips.init);
    assert(h2 == PDDL_COST_DEAD_END || h2 >= hm);
    pddlHMaxFree(&hmax);
}
