#include <assert.h>
#include "test.h"
#include "context.h"

TEST_COND(flow, fdr, LP)
{
    char plan_fn[512];
    sprintf(plan_fn, "%s.plan", TEST_TASK);
    if (!pddlIsFile(plan_fn))
        return;

    pddl_plan_file_fdr_t plan;
    int ret = pddlPlanFileFDRInit(&plan, &C.fdr, plan_fn, &C.err);
    if (ret < 0)
        return;

    pddl_hflow_t flw;
    pddlHFlowInit(&flw, &C.fdr, 0);
    int cost = plan.cost;
    for (int i = 0; i < plan.state_size; ++i){
        int c = pddlHFlow(&flw, plan.state[i], NULL);
        if (c > cost){
            fprintf(stderr, "!!!ERR %d\n", i);
            return;
        }
        assert(c <= cost);
        if (i < borIArrSize(&plan.op))
            cost -= C.fdr.op.op[borIArrGet(&plan.op, i)]->cost;
    }
    pddlHFlowFree(&flw);
    pddlPlanFileFDRFree(&plan);
}

