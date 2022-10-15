#include <assert.h>
#include "test.h"
#include "context.h"

static void checkPlanStates(const pddl_fdr_t *fdr, pddl_symbolic_task_t *ss)
{
    pddl_plan_file_fdr_t planf;
    char plan_fn[256];
    sprintf(plan_fn, "%s.plan", TEST_TASK);
    if (!pddlIsFile(plan_fn))
        return;

    int res = pddlPlanFileFDRInit(&planf, fdr, plan_fn, NULL);
    if (res != 0)
        return;

    for (int si = 0; si < planf.state_size - 1; ++si){
        int op_id = pddlIArrGet(&planf.op, si);
        assert(pddlSymbolicTaskCheckApplyFw(ss, planf.state[si],
                                                planf.state[si + 1], op_id));
    }

    for (int si = planf.state_size - 1; si > 0; --si){
        int op_id = pddlIArrGet(&planf.op, si - 1);
        assert(pddlSymbolicTaskCheckApplyBw(ss, planf.state[si],
                                                planf.state[si - 1], op_id));
    }

    assert(pddlSymbolicTaskCheckPlan(ss, &planf.op, planf.state_size - 1));

    pddlPlanFileFDRFree(&planf);
}

static void checkPlan(const pddl_strips_t *strips, const pddl_iarr_t *plan)
{
    PDDL_ISET(state);
    pddlISetUnion(&state, &strips->init);
    int plan_cost = 0;
    int op_id;
    PDDL_IARR_FOR_EACH(plan, op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        plan_cost += op->cost;
        assert(pddlISetIsSubset(&op->pre, &state));
        if (!pddlISetIsSubset(&op->pre, &state)){
            fprintf(stderr, "Failed on operator %d\n", op_id);
            return;
        }
        PDDL_ISET(state2);
        pddlISetMinus2(&state2, &state, &op->del_eff);
        pddlISetUnion(&state2, &op->add_eff);
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            const pddl_strips_op_cond_eff_t *ce = &op->cond_eff[cei];
            if (pddlISetIsSubset(&ce->pre, &state)){
                pddlISetMinus(&state2, &ce->del_eff);
                pddlISetUnion(&state2, &ce->add_eff);
            }
        }
        pddlISetEmpty(&state);
        pddlISetUnion(&state, &state2);
        pddlISetFree(&state2);
    }
    assert(pddlISetIsSubset(&strips->goal, &state));
    pddlISetFree(&state);

    if (C.optimal_cost < PDDL_COST_MAX)
        assert(C.optimal_cost == plan_cost);
}


TEST_COND(symbolic, fdr, CUDD)
{
}

TEST(symbolic_fw, symbolic)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_task_config_t symb_cfg = PDDL_SYMBOLIC_TASK_CONFIG_INIT;
    symb_cfg.fw.enabled = 1;
    symb_cfg.bw.enabled = 0;

    pddl_symbolic_task_t *task = pddlSymbolicTaskNew(&C.fdr, &symb_cfg, &C.err);

    PDDL_IARR(plan);
    int res = pddlSymbolicTaskSearch(task, &plan, &C.err);
    assert(res == PDDL_SYMBOLIC_PLAN_FOUND || res == PDDL_SYMBOLIC_PLAN_NOT_EXIST);
    if (res == PDDL_SYMBOLIC_PLAN_FOUND)
        checkPlan(&C.strips, &plan);
    pddlIArrFree(&plan);

    pddlSymbolicTaskDel(task);
}

TEST(symbolic_fwbw, symbolic)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_task_config_t symb_cfg = PDDL_SYMBOLIC_TASK_CONFIG_INIT;
    symb_cfg.fw.enabled = 1;
    symb_cfg.bw.enabled = 1;

    pddl_symbolic_task_t *task = pddlSymbolicTaskNew(&C.fdr, &symb_cfg, &C.err);

    PDDL_IARR(plan);
    int res = pddlSymbolicTaskSearch(task, &plan, &C.err);
    assert(res == PDDL_SYMBOLIC_PLAN_FOUND || res == PDDL_SYMBOLIC_PLAN_NOT_EXIST);
    if (res == PDDL_SYMBOLIC_PLAN_FOUND){
        checkPlan(&C.strips, &plan);
        checkPlanStates(&C.fdr, task);
    }
    pddlIArrFree(&plan);

    pddlSymbolicTaskDel(task);
}

TEST(symbolic_bw, symbolic)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_task_config_t symb_cfg = PDDL_SYMBOLIC_TASK_CONFIG_INIT;
    symb_cfg.fw.enabled = 0;
    symb_cfg.bw.enabled = 1;

    pddl_symbolic_task_t *task = pddlSymbolicTaskNew(&C.fdr, &symb_cfg, &C.err);

    PDDL_IARR(plan);
    int res = pddlSymbolicTaskSearch(task, &plan, &C.err);
    assert(res == PDDL_SYMBOLIC_PLAN_FOUND || res == PDDL_SYMBOLIC_PLAN_NOT_EXIST);
    if (res == PDDL_SYMBOLIC_PLAN_FOUND)
        checkPlan(&C.strips, &plan);
    pddlIArrFree(&plan);

    pddlSymbolicTaskDel(task);
}
