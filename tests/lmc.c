#include "test.h"
#include "context.h"
#include <assert.h>

TEST(lmc, fdr)
{
    char plan_fn[512];
    sprintf(plan_fn, "%s.plan", TEST_TASK);
    if (!pddlIsFile(plan_fn))
        return;

    pddl_plan_file_fdr_t plan;
    int ret = pddlPlanFileFDRInit(&plan, &C.fdr, plan_fn, &C.err);
    if (ret < 0)
        return;

    pddl_lm_cut_t lmc;
    pddlLMCutInit(&lmc, &C.fdr, 0, 0);
    int cost = plan.cost;
    for (int i = 0; i < plan.state_size; ++i){
        int c = pddlLMCut(&lmc, plan.state[i], &C.fdr.var, NULL, NULL);
        if (c > cost){
            fprintf(stderr, "!!!ERR %d\n", i);
            return;
        }
        assert(c <= cost);
        if (i < pddlIArrSize(&plan.op))
            cost -= C.fdr.op.op[pddlIArrGet(&plan.op, i)]->cost;
    }
    pddlLMCutFree(&lmc);
    pddlPlanFileFDRFree(&plan);
}
