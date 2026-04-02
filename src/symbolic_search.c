#include "test.h"
#include "context.h"
#include "pddl/symbolic_search.h" // TODO
#include <assert.h>

static void checkPlan(const pddl_fdr_t *fdr,
                      const pddl_iarr_t *plan,
                      pddl_bool_t check_optimality)
{
    int val = validatePlanSeq(fdr, plan);
    assert(val == 0);

    if (check_optimality){
        int plan_cost = 0;
        PDDL_IARR_FOR_EACH(plan, op_id){
            const pddl_fdr_op_t *op = fdr->op.op[op_id];
            plan_cost += op->cost;
        }
        printf("Plan cost: %d\n", plan_cost);
        if (C.optimal_cost >= 0)
            assert(C.optimal_cost == plan_cost);
    }
}

static void run(pddl_symbolic_search_config_t *symb_cfg,
                pddl_bool_t check_optimality)
{
    symb_cfg->fdr = &C.fdr;
    symb_cfg->constr_max_time = 30.;
    symb_cfg->goal_constr_max_time = 30.;
    symb_cfg->bw.bw.step_time_limit = 30.;

    pddlErrLogEnable(&C.err, stderr);
    pddl_symbolic_search_t *s = pddlSymbolicSearchNew(symb_cfg, &C.err);
    if (s == NULL){
        pddlErrPrint(&C.err, 0, stdout);
        return;
    }

    pddl_symbolic_search_status_t st;
    st = pddlSymbolicSearchInitStep(s, &C.err);
    while (st == PDDL_SYMBOLIC_SEARCH_STATUS_CONT){
        st = pddlSymbolicSearchStep(s, &C.err);
        switch (st){
            case PDDL_SYMBOLIC_SEARCH_STATUS_CONT:
                pddlSymbolicSearchProgressLog(s, &C.err);
                break;
            case PDDL_SYMBOLIC_SEARCH_STATUS_FOUND_PLAN:
                assert(!check_optimality);
            case PDDL_SYMBOLIC_SEARCH_STATUS_FOUND_OPTIMAL_PLAN:
                pddlSymbolicSearchProgressLog(s, &C.err);
                PDDL_LOG(&C.err, "Plan found.");

                PDDL_IARR(plan);
                pddlSymbolicSearchExtractPlan(s, &plan);
                checkPlan(&C.fdr, &plan, check_optimality);
                pddlIArrFree(&plan);
                break;

            case PDDL_SYMBOLIC_SEARCH_STATUS_UNSOLVABLE:
                pddlSymbolicSearchProgressLog(s, &C.err);
                PDDL_LOG(&C.err, "Task unsolvable.");
                printf("Unsolvable.\n");
                assert(C.optimal_cost < 0);
                break;

            case PDDL_SYMBOLIC_SEARCH_STATUS_EXPLORED:
                pddlSymbolicSearchProgressLog(s, &C.err);
                PDDL_LOG(&C.err, "State space fully explored.");
                // We terminate with the first plan, so this should never
                // happen.
                assert(0);
                break;

            case PDDL_SYMBOLIC_SEARCH_STATUS_ERR:
                pddlErrPrint(&C.err, 0, stdout);
                break;
        }
    }
    pddlSymbolicSearchStatsLog(s, &C.err);
    pddlSymbolicSearchDel(s);
}

TEST_COND(symbolic_search, fdr_essential, CUDD)
{
}

TEST(symbolic_search_fw_blind, symbolic_search)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_ASTAR;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_NONE;
    symb_cfg.fw.pot_heur = pddl_false;
    symb_cfg.bw.pot_heur = pddl_false;
    run(&symb_cfg, pddl_true);
}

TEST(symbolic_search_bw_blind, symbolic_search)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_ASTAR;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_NONE;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.fw.pot_heur = pddl_false;
    symb_cfg.bw.pot_heur = pddl_false;
    run(&symb_cfg, pddl_true);
}

TEST(symbolic_search_fwbw_blind, symbolic_search)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_ASTAR;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.fw.pot_heur = pddl_false;
    symb_cfg.bw.pot_heur = pddl_false;

    run(&symb_cfg, pddl_true);
}

TEST_COND(symbolic_search_fw_astar_AI, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_ASTAR;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_NONE;
    symb_cfg.fw.pot_heur = pddl_true;
    symb_cfg.fw.pot_heur_config.opt.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    symb_cfg.fw.pot_heur_config.opt.all_syntactic_states.add_state_constr.init_state = pddl_true;
    symb_cfg.bw.pot_heur = pddl_false;
    run(&symb_cfg, pddl_true);
}

TEST_COND(symbolic_search_bw_astar_I, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_ASTAR;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_NONE;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.fw.pot_heur = pddl_false;
    symb_cfg.bw.pot_heur = pddl_true;
    symb_cfg.bw.pot_heur_config.opt.type = PDDL_HPOT_OPT_STATE_TYPE;
    run(&symb_cfg, pddl_true);
}

TEST_COND(symbolic_search_fwbw_astar_AI_I, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_ASTAR;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.fw.pot_heur = pddl_true;
    symb_cfg.fw.pot_heur_config.opt.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    symb_cfg.fw.pot_heur_config.opt.all_syntactic_states.add_state_constr.init_state = pddl_true;
    symb_cfg.bw.pot_heur = pddl_true;
    symb_cfg.bw.pot_heur_config.opt.type = PDDL_HPOT_OPT_STATE_TYPE;
    run(&symb_cfg, pddl_true);
}

TEST_COND(symbolic_search_fwbw_astar_AI_blind, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_ASTAR;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.fw.pot_heur = pddl_true;
    symb_cfg.fw.pot_heur_config.opt.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    symb_cfg.fw.pot_heur_config.opt.all_syntactic_states.add_state_constr.init_state = pddl_true;
    symb_cfg.bw.pot_heur = pddl_false;
    run(&symb_cfg, pddl_true);
}

TEST_COND(symbolic_search_fwbw_astar_blind_I, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_ASTAR;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_ASTAR;
    symb_cfg.fw.pot_heur = pddl_false;
    symb_cfg.bw.pot_heur = pddl_true;
    symb_cfg.bw.pot_heur_config.opt.type = PDDL_HPOT_OPT_STATE_TYPE;
    run(&symb_cfg, pddl_true);
}

TEST_COND(symbolic_search_fw_gbfs_AI_gc, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_hpot_config_opt_t gc_opt = PDDL_HPOT_CONFIG_OPT_INIT;
    gc_opt.type = PDDL_HPOT_OPT_GOAL_COUNT_TYPE;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_GBFS;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_GBFS;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_NONE;
    symb_cfg.fw.pot_heur = pddl_true;
    symb_cfg.fw.pot_heur_config.opt.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    symb_cfg.fw.pot_heur_config.opt.all_syntactic_states.add_state_constr.init_state = pddl_true;
    symb_cfg.fw.pot_heur_config.opt.next = &gc_opt;
    symb_cfg.bw.pot_heur = pddl_false;
    run(&symb_cfg, pddl_false);
}

TEST_COND(symbolic_search_fw_gbfs_gc, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_GBFS;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_GBFS;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_NONE;
    symb_cfg.fw.pot_heur = pddl_true;
    symb_cfg.fw.pot_heur_config.opt.type = PDDL_HPOT_OPT_GOAL_COUNT_TYPE;
    symb_cfg.bw.pot_heur = pddl_false;
    run(&symb_cfg, pddl_false);
}

TEST_COND(symbolic_search_bw_gbfs_I_gc, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_hpot_config_opt_t gc_opt = PDDL_HPOT_CONFIG_OPT_INIT;
    gc_opt.type = PDDL_HPOT_OPT_GOAL_COUNT_TYPE;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_GBFS;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_NONE;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_GBFS;
    symb_cfg.fw.pot_heur = pddl_false;
    symb_cfg.bw.pot_heur = pddl_true;
    symb_cfg.bw.pot_heur_config.opt.type = PDDL_HPOT_OPT_STATE_TYPE;
    symb_cfg.bw.pot_heur_config.opt.next = &gc_opt;
    run(&symb_cfg, pddl_false);
}

TEST_COND(symbolic_search_fwbw_gbfs_AI_gc_I_gc, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_hpot_config_opt_t fw_gc_opt = PDDL_HPOT_CONFIG_OPT_INIT;
    fw_gc_opt.type = PDDL_HPOT_OPT_GOAL_COUNT_TYPE;

    pddl_hpot_config_opt_t bw_gc_opt = PDDL_HPOT_CONFIG_OPT_INIT;
    bw_gc_opt.type = PDDL_HPOT_OPT_GOAL_COUNT_TYPE;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_GBFS;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_GBFS;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_GBFS;
    symb_cfg.fw.pot_heur = pddl_true;
    symb_cfg.fw.pot_heur_config.opt.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    symb_cfg.fw.pot_heur_config.opt.all_syntactic_states.add_state_constr.init_state = pddl_true;
    symb_cfg.fw.pot_heur_config.opt.next = &fw_gc_opt;
    symb_cfg.bw.pot_heur = pddl_true;
    symb_cfg.bw.pot_heur_config.opt.type = PDDL_HPOT_OPT_STATE_TYPE;
    symb_cfg.bw.pot_heur_config.opt.next = &bw_gc_opt;
    run(&symb_cfg, pddl_false);
}

TEST_COND(symbolic_search_fwbw_gbfs_gc_I_gc, symbolic_search, LP)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_hpot_config_opt_t bw_gc_opt = PDDL_HPOT_CONFIG_OPT_INIT;
    bw_gc_opt.type = PDDL_HPOT_OPT_GOAL_COUNT_TYPE;

    pddl_symbolic_search_config_t symb_cfg = PDDL_SYMBOLIC_SEARCH_CONFIG_INIT_GBFS;
    symb_cfg.fw.alg = PDDL_SYMBOLIC_SEARCH_GBFS;
    symb_cfg.bw.alg = PDDL_SYMBOLIC_SEARCH_GBFS;
    symb_cfg.fw.pot_heur = pddl_true;
    symb_cfg.fw.pot_heur_config.opt.type = PDDL_HPOT_OPT_GOAL_COUNT_TYPE;
    symb_cfg.bw.pot_heur = pddl_true;
    symb_cfg.bw.pot_heur_config.opt.type = PDDL_HPOT_OPT_STATE_TYPE;
    symb_cfg.bw.pot_heur_config.opt.next = &bw_gc_opt;
    run(&symb_cfg, pddl_false);
}
