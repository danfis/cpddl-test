#include "test.h"
#include "context.h"
#include <assert.h>

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

    if (C.optimal_cost >= 0)
        assert(C.optimal_cost == plan_cost);
}

static int fdrHasTNFOps(const pddl_fdr_t *fdr)
{
    for (int oi = 0; oi < fdr->op.op_size; ++oi){
        const pddl_fdr_op_t *op = fdr->op.op[oi];
        for (int i = 0; i < op->eff.fact_size; ++i){
            if (!pddlFDRPartStateIsSet(&op->pre, op->eff.fact[i].var))
                return 0;
        }
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            const pddl_fdr_op_cond_eff_t *ce = op->cond_eff + cei;
            for (int i = 0; i < ce->eff.fact_size; ++i){
                if (!pddlFDRPartStateIsSet(&ce->pre, ce->eff.fact[i].var))
                    return 0;
            }
        }
    }

    return 1;
}

static void run(pddl_symbolic_task_config_t *symb_cfg)
{
    symb_cfg->constr_max_time = 1000.;
    symb_cfg->goal_constr_max_time = 1000.;
    symb_cfg->bw.step_time_limit = 30.;

    pddl_symbolic_task_t *task = pddlSymbolicTaskNew(&C.fdr, symb_cfg, &C.err);
    if (task == NULL){
        pddlErrPrint(&C.err, 0, stdout);
        return;
    }

    PDDL_IARR(plan);
    int res = pddlSymbolicTaskSearch(task, &plan, &C.err);
    assert(res == PDDL_SYMBOLIC_PLAN_FOUND || res == PDDL_SYMBOLIC_PLAN_NOT_EXIST);
    if (res == PDDL_SYMBOLIC_PLAN_FOUND){
        checkPlan(&C.strips, &plan);
        checkPlanStates(&C.fdr, task);

        int plan_cost = 0;
        int op_id;
        PDDL_IARR_FOR_EACH(&plan, op_id){
            const pddl_fdr_op_t *op = C.fdr.op.op[op_id];
            plan_cost += op->cost;
        }
        printf("Plan Cost: %d\n", plan_cost);
    }

    pddlIArrFree(&plan);
    pddlSymbolicTaskDel(task);
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
    run(&symb_cfg);
}

TEST_COND(symbolic_fw_pot, symbolic, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_task_config_t symb_cfg = PDDL_SYMBOLIC_TASK_CONFIG_INIT;
    symb_cfg.fw.enabled = 1;
    symb_cfg.bw.enabled = 0;

    symb_cfg.fw.pot_heur_config.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    symb_cfg.fw.pot_heur_config.opt_all_syntactic_states.add_state_constr.init_state = pddl_true;
    if (fdrHasTNFOps(&C.fdr)){
        symb_cfg.fw.use_pot_heur = pddl_true;
        symb_cfg.fw.use_pot_heur_inconsistent = pddl_false;
    }else{
        symb_cfg.fw.use_pot_heur = pddl_false;
        symb_cfg.fw.use_pot_heur_inconsistent = pddl_true;
    }
    run(&symb_cfg);
}

TEST(symbolic_fwbw, symbolic)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_task_config_t symb_cfg = PDDL_SYMBOLIC_TASK_CONFIG_INIT;
    symb_cfg.fw.enabled = 1;
    symb_cfg.bw.enabled = 1;
    run(&symb_cfg);
}

TEST_COND(symbolic_fwbw_pot, symbolic, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_task_config_t symb_cfg = PDDL_SYMBOLIC_TASK_CONFIG_INIT;
    symb_cfg.fw.enabled = 1;
    symb_cfg.bw.enabled = 1;

    symb_cfg.fw.pot_heur_config.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    symb_cfg.fw.pot_heur_config.opt_all_syntactic_states.add_state_constr.init_state = pddl_true;
    if (fdrHasTNFOps(&C.fdr)){
        symb_cfg.fw.use_pot_heur = pddl_true;
        symb_cfg.fw.use_pot_heur_inconsistent = pddl_false;
    }else{
        symb_cfg.fw.use_pot_heur = pddl_false;
        symb_cfg.fw.use_pot_heur_inconsistent = pddl_true;
    }
    run(&symb_cfg);
}

TEST_COND(symbolic_fwbw_pot_pot, symbolic, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddlErrLogEnable(&C.err, stderr);
    pddl_bool_t is_tnf = fdrHasTNFOps(&C.fdr);

    pddl_symbolic_task_config_t symb_cfg = PDDL_SYMBOLIC_TASK_CONFIG_INIT;
    symb_cfg.fw.enabled = 1;
    symb_cfg.bw.enabled = 1;
    symb_cfg.log_every_step = 0;

    symb_cfg.fw.pot_heur_config.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    symb_cfg.fw.pot_heur_config.opt_all_syntactic_states.add_state_constr.init_state = pddl_true;
    if (is_tnf){
        symb_cfg.fw.use_pot_heur = pddl_true;
        symb_cfg.fw.use_pot_heur_inconsistent = pddl_false;
    }else{
        symb_cfg.fw.use_pot_heur = pddl_false;
        symb_cfg.fw.use_pot_heur_inconsistent = pddl_true;
    }

    symb_cfg.bw.pot_heur_config.type = PDDL_HPOT_OPT_STATE_TYPE;
    if (is_tnf){
        symb_cfg.bw.use_pot_heur = pddl_true;
        symb_cfg.bw.use_pot_heur_inconsistent = pddl_false;
        symb_cfg.bw.use_goal_splitting = pddl_true;
    }else{
        symb_cfg.bw.use_pot_heur = pddl_false;
        symb_cfg.bw.use_pot_heur_inconsistent = pddl_true;
        symb_cfg.bw.use_goal_splitting = pddl_true;
    }

    run(&symb_cfg);
}

TEST(symbolic_bw, symbolic)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_task_config_t symb_cfg = PDDL_SYMBOLIC_TASK_CONFIG_INIT;
    symb_cfg.fw.enabled = 0;
    symb_cfg.bw.enabled = 1;
    run(&symb_cfg);
}

TEST_COND(symbolic_bw_pot, symbolic, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_bool_t is_tnf = fdrHasTNFOps(&C.fdr);

    pddl_symbolic_task_config_t symb_cfg = PDDL_SYMBOLIC_TASK_CONFIG_INIT;
    symb_cfg.fw.enabled = 0;
    symb_cfg.bw.enabled = 1;

    symb_cfg.bw.pot_heur_config.type = PDDL_HPOT_OPT_STATE_TYPE;
    if (is_tnf){
        symb_cfg.bw.use_pot_heur = pddl_true;
        symb_cfg.bw.use_pot_heur_inconsistent = pddl_false;
        symb_cfg.bw.use_goal_splitting = pddl_true;
    }else{
        symb_cfg.bw.use_pot_heur = pddl_false;
        symb_cfg.bw.use_pot_heur_inconsistent = pddl_true;
        symb_cfg.bw.use_goal_splitting = pddl_false;
    }

    run(&symb_cfg);
}
