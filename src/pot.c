#include "test.h"
#include "context.h"
#include <assert.h>

static void checkOpChangeState(const pddl_fdr_t *fdr,
                               const int *state,
                               const pddl_pot_solution_t *pot,
                               int depth)
{
    size_t state_size = sizeof(int) * fdr->var.var_size;
    double heur = pddlPotSolutionEvalFDRStateFlt(pot, &fdr->var, state);

    for (int opi = 0; opi < fdr->op.op_size; ++opi){
        const pddl_fdr_op_t *op = fdr->op.op[opi];
        if (pddlFDROpIsApplicable(op, state)){
            int *state2 = alloca(state_size);
            pddlFDROpApplyOnState(op, fdr->var.var_size, state, state2);
            double heur2;
            heur2 = pddlPotSolutionEvalFDRStateFlt(pot, &fdr->var, state2);
            double heur_op = heur + pot->op_pot[opi];
            assert((int)(heur - heur_op) <= op->cost);
            assert(heur_op <= (heur2 + 1E-4));

            if (depth > 0){
                checkOpChangeState(fdr, state2, pot, depth - 1);
            }
        }
    }

}

static void checkOpChange(const pddl_fdr_t *fdr, const pddl_pot_solution_t *pot)
{
    size_t state_size = sizeof(int) * fdr->var.var_size;
    int *state = alloca(state_size);
    memcpy(state, fdr->init, state_size);
    checkOpChangeState(fdr, state, pot, 2);
}

TEST_COND(pot, fdr, LP)
{
}

static int pot_value_init = -1;

TEST(pot_init, pot)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_pot_config_t cfg = PDDL_POT_CONFIG_INIT;
    cfg.fdr = &C.fdr;

    pddl_pot_t pot;
    pddlPotInit(&pot, &cfg);
    pddlPotSetObjFDRState(&pot, &C.fdr.var, C.fdr.init);

    pddl_pot_solve_config_t sol_cfg = PDDL_POT_SOLVE_CONFIG_INIT;
    sol_cfg.op_pot = pddl_true;

    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    int ret = pddlPotSolve(&pot, &sol_cfg, &sol, &C.err);
    assert(ret == 0);
    assert(sol.found);
    assert(!sol.suboptimal);
    assert(!sol.timed_out);
    assert(!sol.error);
    checkOpChange(&C.fdr, &sol);

    int val = pddlPotSolutionEvalFDRState(&sol, &C.fdr.var, C.fdr.init);
    pot_value_init = val;
    if (C.optimal_cost >= 0)
        assert(val <= C.optimal_cost);
    fprintf(stdout, "Value: %d\n", val);
    pddlPotSolutionFree(&sol);
    pddlPotFree(&pot);
}

TEST(pot_all_states, pot_init)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_pot_config_t cfg = PDDL_POT_CONFIG_INIT;
    cfg.fdr = &C.fdr;

    pddl_pot_t pot;
    pddlPotInit(&pot, &cfg);
    pddlPotSetObjFDRAllSyntacticStates(&pot, &C.fdr.var);

    pddl_pot_solve_config_t sol_cfg = PDDL_POT_SOLVE_CONFIG_INIT;
    sol_cfg.op_pot = pddl_true;

    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    int ret = pddlPotSolve(&pot, &sol_cfg, &sol, &C.err);
    assert(ret == 0);
    assert(sol.found);
    assert(!sol.suboptimal);
    assert(!sol.timed_out);
    assert(!sol.error);
    checkOpChange(&C.fdr, &sol);

    int val = pddlPotSolutionEvalFDRState(&sol, &C.fdr.var, C.fdr.init);
    assert(val <= pot_value_init);
    if (C.optimal_cost >= 0)
        assert(val <= C.optimal_cost);
    pddlPotSolutionFree(&sol);
    pddlPotFree(&pot);
}

static int pot_value_mg_strips_init = -1;
TEST(pot_mg_strips_init, pot_init)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_mg_strips_t mg_strips;
    pddlMGStripsInitFDR(&mg_strips, &C.fdr);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);
    pddlMutexPairsAddMGroups(&mutex, &mg_strips.mg);
    pddlStripsPrintDebug(&mg_strips.strips, stdout);

    pddl_pot_config_t cfg = PDDL_POT_CONFIG_INIT;
    cfg.mg_strips = &mg_strips;
    cfg.mutex = &mutex;

    pddl_pot_t pot;
    pddlPotInit(&pot, &cfg);
    pddlPotSetObjStripsState(&pot, &mg_strips.strips.init);

    pddl_pot_solve_config_t sol_cfg = PDDL_POT_SOLVE_CONFIG_INIT;

    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    pddlPotSolve(&pot, &sol_cfg, &sol, &C.err);
    assert(sol.found);
    assert(!sol.suboptimal);
    assert(!sol.timed_out);
    assert(!sol.error);
    int val = pddlPotSolutionEvalStripsState(&sol, &mg_strips.strips.init);
    assert(val >= pot_value_init);
    if (C.optimal_cost >= 0)
        assert(val <= C.optimal_cost);
    pot_value_mg_strips_init = val;
    fprintf(stdout, "Value: %d\n", val);
    pddlPotSolutionFree(&sol);
    pddlPotFree(&pot);
    pddlMutexPairsFree(&mutex);
    pddlMGStripsFree(&mg_strips);
}

TEST(pot_mg_strips_mutex_init, pot_mg_strips_init)
{
    if (C.fdr.goal_is_unreachable)
        return;

    pddl_mg_strips_t mg_strips;
    pddlMGStripsInitFDR(&mg_strips, &C.fdr);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);
    pddlMutexPairsAddMGroups(&mutex, &mg_strips.mg);
    pddlH2FwMutex(&mg_strips.strips, &mutex, &C.err);

    pddl_pot_config_t cfg = PDDL_POT_CONFIG_INIT;
    cfg.mg_strips = &mg_strips;
    cfg.mutex = &mutex;

    pddl_pot_t pot;
    pddlPotInit(&pot, &cfg);
    pddlPotSetObjStripsState(&pot, &mg_strips.strips.init);

    pddl_pot_solve_config_t sol_cfg = PDDL_POT_SOLVE_CONFIG_INIT;

    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    pddlPotSolve(&pot, &sol_cfg, &sol, &C.err);
    assert(sol.found);
    assert(!sol.suboptimal);
    assert(!sol.timed_out);
    assert(!sol.error);
    int val = pddlPotSolutionEvalStripsState(&sol, &mg_strips.strips.init);
    assert(val >= pot_value_init);
    assert(val >= pot_value_mg_strips_init);
    if (C.optimal_cost >= 0)
        assert(val <= C.optimal_cost);
    fprintf(stdout, "Value: %d\n", val);
    pddlPotSolutionFree(&sol);
    pddlPotFree(&pot);
    pddlMutexPairsFree(&mutex);
    pddlMGStripsFree(&mg_strips);
}

static double roundFlt(double v)
{
    v = ceil(v - 0.1);
    if (v == -0.)
        v = 0.;
    return v;
}

static pddl_hpot_config_t hcfg = PDDL_HPOT_CONFIG_INIT;
static pddl_task_t *task = NULL;

static void _test_hpot1(const pddl_hpot_config_t *cfg, pddl_task_t *task)
{
    pddl_pot_solutions_t sols;
    pddlPotSolutionsInit(&sols);

    int ret = pddlHPot(&sols, cfg, &C.err);
    if (ret < 0){
        pddlErrPrint(&C.err, 1, stderr);
        pddlPotSolutionsFree(&sols);
    }
    assert(ret == 0);
    if (sols.unsolvable){
        fprintf(stdout, "unsolvable\n");
        pddlPotSolutionsFree(&sols);
        return;
    }

    for (int i = 0; i < sols.sol_size; ++i){
        const pddl_pot_solution_t *sol = sols.sol + i;
        fprintf(stdout, "objval[%d]: %.0f\n", i, roundFlt(sol->objval));
        if (cfg->op_pot)
            assert(sol->op_pot_size > 0);
    }

    pddlPotSolutionsFree(&sols);
}

static pddl_mg_strips_t mg_strips;
static pddl_mutex_pairs_t mutex;
TEST_COND(hpot, fdr, LP)
{
    pddlMGStripsInitFDR(&mg_strips, &C.fdr);
    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);
    pddlMutexPairsAddMGroups(&mutex, &mg_strips.mg);
    pddlH2FwMutex(&mg_strips.strips, &mutex, &C.err);
    pddlStripsDisambiguatePres(&mg_strips.strips, &mutex, &mg_strips.mg,
                               NULL, NULL, &C.err);
    pddlStripsRemoveUselessDelEffs(&mg_strips.strips, &mutex, NULL, &C.err);

    pddlErrLogEnable(&C.err, stderr);
    //pddlLPSetDefault(PDDL_LP_GUROBI, NULL);
    hcfg.fdr = &C.fdr;
    hcfg.mg_strips = &mg_strips;
    hcfg.mutex = &mutex;
    hcfg.disambiguation = 1;
    hcfg.op_pot = 0;
    hcfg.op_pot_real = 0;
}

TEST_TEAR_DOWN(hpot)
{
    pddlMutexPairsFree(&mutex);
    pddlMGStripsFree(&mg_strips);
}

TEST(hpot_init, hpot)
{
    hcfg.opt.type = PDDL_HPOT_OPT_STATE_TYPE;
    hcfg.opt.state.fdr_state = C.fdr.init;
    _test_hpot1(&hcfg, task);
}

TEST(hpot_all_states, hpot)
{
    hcfg.opt.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    _test_hpot1(&hcfg, task);
}

TEST(hpot_all_states_cinit, hpot)
{
    hcfg.opt.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    hcfg.opt.all_syntactic_states.add_state_constr.fdr_state = C.fdr.init;
    _test_hpot1(&hcfg, task);
}

static pddl_set_iset_t conjs;
static int conjs_best_h_value;
TEST(pot_conj, hpot)
{
    pddlSetISetInit(&conjs);

    if (C.fdr.goal_is_unreachable)
        return;

    pddl_pot_conj_find_config_t cfg = PDDL_POT_CONJ_FIND_CONFIG_INIT;
    cfg.time_limit = 100000.;
    cfg.random_seed = 1;
    cfg.max_conj_dim = 3;
    cfg.max_num_conjs = 500;
    if (strstr(TEST_TASK, "agricola") != NULL)
        cfg.max_num_conjs = 50;

    pddl_hpot_config_t pot_cfg = PDDL_HPOT_CONFIG_INIT;
    pot_cfg.opt.type = PDDL_HPOT_OPT_STATE_TYPE;

    int ret = pddlPotConjFind(&conjs, &conjs_best_h_value, NULL,
                              &hcfg.mg_strips->strips, hcfg.mutex,
                              &hcfg.mg_strips->mg, &cfg, &pot_cfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    for (int f1 = 0; f1 < C.fdr.var.global_id_size; ++f1){
        if (pddlSetISetSize(&conjs) > 0)
            break;

        for (int f2 = f1 + 1; f2 < C.fdr.var.global_id_size; ++f2){
            if (!pddlMutexPairsIsMutex(&mutex, f1, f2)){
                PDDL_ISET(c);
                PDDL_ISET_SET(&c, f1, f2);
                pddlSetISetAdd(&conjs, &c);
                pddlISetFree(&c);
                break;
            }
        }
    }

    assert(pddlSetISetSize(&conjs) > 0);
    printf("conjs: %d\n", pddlSetISetSize(&conjs));
    for (int i = 0; i < pddlSetISetSize(&conjs); ++i){
        const pddl_iset_t *c = pddlSetISetGet(&conjs, i);
        int fact;
        printf("Conj %d:", i);
        PDDL_ISET_FOR_EACH(c, fact){
            printf(" (%s)", C.fdr.var.global_id_to_val[fact]->name);
        }
        printf("\n");
    }
}

TEST_TEAR_DOWN(pot_conj)
{
    pddlSetISetFree(&conjs);
}

TEST(pot_conj_init, pot_conj)
{
    if (pddlSetISetSize(&conjs) == 0)
        return;

    hcfg.opt.type = PDDL_HPOT_OPT_STATE_TYPE;

    pddl_pot_conj_t pot;
    int ret = pddlPotConjInit(&pot, &conjs, &hcfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    int hvalue = pddlPotConjEvalMaxFDRState(&pot, &C.fdr.var, C.fdr.init);
    printf("h-value for init: %d\n", hvalue);
    fflush(stdout);
    assert(hvalue == conjs_best_h_value);
    pddlPotConjFree(&pot);
}

TEST(pot_conj_all, pot_conj)
{
    if (pddlSetISetSize(&conjs) == 0)
        return;

    hcfg.opt.type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    hcfg.opt.all_syntactic_states.add_state_constr.init_state = pddl_true;

    pddl_pot_conj_t pot;
    int ret = pddlPotConjInit(&pot, &conjs, &hcfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    int hvalue = pddlPotConjEvalMaxFDRState(&pot, &C.fdr.var, C.fdr.init);
    printf("h-value for init: %d\n", hvalue);
    fflush(stdout);
    assert(hvalue == conjs_best_h_value);
    pddlPotConjFree(&pot);
}


#if 0
// It's hard to make the following tests work across different LP solvers...
TEST(hpot_mutex1, hpot)
{
    pddl_hpot_config_opt_all_states_mutex_t cfg_mutex1
            = PDDL_HPOT_CONFIG_OPT_ALL_STATES_MUTEX_INIT;
    cfg_mutex1.mutex_size = 1;
    PDDL_HPOT_CONFIG_ADD(&hcfg, &cfg_mutex1);
    _test_hpot1(&hcfg, task);
}

TEST(hpot_mutex1_cinit, hpot)
{
    pddl_hpot_config_opt_all_states_mutex_t cfg_mutex1_cinit
            = PDDL_HPOT_CONFIG_OPT_ALL_STATES_MUTEX_INIT;
    cfg_mutex1_cinit.mutex_size = 1;
    cfg_mutex1_cinit.add_fdr_state_constr = C.fdr.init;
    PDDL_HPOT_CONFIG_ADD(&hcfg, &cfg_mutex1_cinit);
    _test_hpot1(&hcfg, task);
}

TEST(hpot_mutex2, hpot)
{
    pddl_hpot_config_opt_all_states_mutex_t cfg_mutex2
            = PDDL_HPOT_CONFIG_OPT_ALL_STATES_MUTEX_INIT;
    cfg_mutex2.mutex_size = 2;
    PDDL_HPOT_CONFIG_ADD(&hcfg, &cfg_mutex2);
    _test_hpot1(&hcfg, task);
}

TEST(hpot_mutex2_cinit, hpot)
{
    pddl_hpot_config_opt_all_states_mutex_t cfg_mutex2_cinit
            = PDDL_HPOT_CONFIG_OPT_ALL_STATES_MUTEX_INIT;
    cfg_mutex2_cinit.mutex_size = 2;
    cfg_mutex2_cinit.add_fdr_state_constr = C.fdr.init;
    PDDL_HPOT_CONFIG_ADD(&hcfg, &cfg_mutex2_cinit);
    _test_hpot1(&hcfg, task);
}

TEST(hpot_sampled_states, hpot)
{
    pddl_hpot_config_opt_sampled_states_t cfg_sstates
            = PDDL_HPOT_CONFIG_OPT_SAMPLED_STATES_INIT;
    cfg_sstates.num_samples = 1000;
    PDDL_HPOT_CONFIG_ADD(&hcfg, &cfg_sstates);
    _test_hpot1(&hcfg, task);
}

TEST(hpot_all_sampled_states_cinit, hpot)
{
    pddl_hpot_config_opt_sampled_states_t cfg_sstates_cinit
            = PDDL_HPOT_CONFIG_OPT_SAMPLED_STATES_INIT;
    cfg_sstates_cinit.num_samples = 1000;
    cfg_sstates_cinit.add_fdr_state_constr = C.fdr.init;
    PDDL_HPOT_CONFIG_ADD(&hcfg, &cfg_sstates_cinit);
    _test_hpot1(&hcfg, task);
}

static void _test_hpot(int op_pot, pddl_task_t *task)
{
    pddl_pot_solutions_t sols;
    pddlPotSolutionsInit(&sols);

    pddl_hpot_config_t cfg = PDDL_HPOT_CONFIG_INIT;
    cfg.disambiguation = 1;
    cfg.weak_disambiguation = 0;
    cfg.op_pot = op_pot;

    pddl_hpot_config_opt_state_t cfg_init = PDDL_HPOT_CONFIG_OPT_STATE_INIT;
    cfg_init.fdr_state = C.fdr.init;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_init);

    pddl_hpot_config_opt_all_syntactic_states_t cfg_all
            = PDDL_HPOT_CONFIG_OPT_ALL_SYNTACTIC_STATES_INIT;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_all);

    pddl_hpot_config_opt_all_syntactic_states_t cfg_all_cinit
            = PDDL_HPOT_CONFIG_OPT_ALL_SYNTACTIC_STATES_INIT;
    cfg_all_cinit.add_fdr_state_constr = C.fdr.init;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_all_cinit);

    pddl_hpot_config_opt_all_states_mutex_t cfg_mutex1
            = PDDL_HPOT_CONFIG_OPT_ALL_STATES_MUTEX_INIT;
    cfg_mutex1.mutex_size = 1;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_mutex1);

    pddl_hpot_config_opt_all_states_mutex_t cfg_mutex1_cinit
            = PDDL_HPOT_CONFIG_OPT_ALL_STATES_MUTEX_INIT;
    cfg_mutex1_cinit.mutex_size = 1;
    cfg_mutex1_cinit.add_fdr_state_constr = C.fdr.init;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_mutex1_cinit);

    pddl_hpot_config_opt_all_states_mutex_t cfg_mutex2
            = PDDL_HPOT_CONFIG_OPT_ALL_STATES_MUTEX_INIT;
    cfg_mutex2.mutex_size = 2;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_mutex2);

    pddl_hpot_config_opt_all_states_mutex_t cfg_mutex2_cinit
            = PDDL_HPOT_CONFIG_OPT_ALL_STATES_MUTEX_INIT;
    cfg_mutex2_cinit.mutex_size = 2;
    cfg_mutex2_cinit.add_fdr_state_constr = C.fdr.init;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_mutex2_cinit);

    pddl_hpot_config_opt_sampled_states_t cfg_sstates
            = PDDL_HPOT_CONFIG_OPT_SAMPLED_STATES_INIT;
    cfg_sstates.num_samples = 1000;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_sstates);

    pddl_hpot_config_opt_sampled_states_t cfg_sstates_cinit
            = PDDL_HPOT_CONFIG_OPT_SAMPLED_STATES_INIT;
    cfg_sstates_cinit.num_samples = 1000;
    cfg_sstates_cinit.add_fdr_state_constr = C.fdr.init;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_sstates_cinit);

    int ret = pddlHPot(&sols, task, &cfg, &C.err);
    if (ret < 0){
        fprintf(stdout, "ret: %d\n", ret);
        pddlPotSolutionsFree(&sols);
        return;
    }
    assert(ret == 0);
    assert(sols.sol_size == 9);

    const pddl_pot_solution_t *sol = sols.sol + 0;
    fprintf(stdout, "init objval: %.0f\n", roundFlt(sol->objval));
    if (op_pot)
        assert(sol->op_pot_size > 0);
    sol = sols.sol + 1;
    fprintf(stdout, "all objval: %.0f\n", roundFlt(sol->objval));
    if (op_pot)
        assert(sol->op_pot_size > 0);
    sol = sols.sol + 2;
    fprintf(stdout, "all+cinit objval: %.0f\n", roundFlt(sol->objval));
    if (op_pot)
        assert(sol->op_pot_size > 0);
    sol = sols.sol + 3;
    fprintf(stdout, "mutex=1 objval: %.0f\n", roundFlt(sol->objval));
    if (op_pot)
        assert(sol->op_pot_size > 0);
    sol = sols.sol + 4;
    fprintf(stdout, "mutex=1+cinit objval: %.0f\n", roundFlt(sol->objval));
    if (op_pot)
        assert(sol->op_pot_size > 0);
    sol = sols.sol + 5;
    fprintf(stdout, "mutex=2 objval: %.0f\n", roundFlt(sol->objval));
    if (op_pot)
        assert(sol->op_pot_size > 0);
    sol = sols.sol + 6;
    fprintf(stdout, "mutex=2+cinit objval: %.0f\n", roundFlt(sol->objval));
    if (op_pot)
        assert(sol->op_pot_size > 0);
    sol = sols.sol + 7;
    fprintf(stdout, "samples=1000 objval: %.0f\n", roundFlt(sol->objval));
    if (op_pot)
        assert(sol->op_pot_size > 0);
    sol = sols.sol + 8;
    fprintf(stdout, "samples=1000+cinit objval: %.0f\n", roundFlt(sol->objval));
    if (op_pot)
        assert(sol->op_pot_size > 0);

    pddlPotSolutionsFree(&sols);
}

TEST(hpot_all, hpot)
{
    _test_hpot(0, task);
}

TEST(hpot_all_op_pot, hpot_all)
{
    _test_hpot(1, task);
}

TEST(hpot_ensemble_sampled_states, hpot_all)
{
    pddl_pot_solutions_t sols;
    pddlPotSolutionsInit(&sols);

    pddl_hpot_config_t cfg = PDDL_HPOT_CONFIG_INIT;
    cfg.disambiguation = 1;

    pddl_hpot_config_opt_ensemble_sampled_states_t cfg_opt
            = PDDL_HPOT_CONFIG_OPT_ENSEMBLE_SAMPLED_STATES_INIT;
    cfg_opt.num_samples = 10;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_opt);

    int ret = pddlHPot(&sols, task, &cfg, &C.err);
    if (ret < 0){
        fprintf(stdout, "ret: %d\n", ret);
        pddlPotSolutionsFree(&sols);
        return;
    }
    assert(ret == 0);

    for (int i = 0; i < sols.sol_size; ++i){
        const pddl_pot_solution_t *sol = sols.sol + i;
        fprintf(stdout, "objval[%d]: %.0f\n", i, roundFlt(sol->objval));
    }

    pddlPotSolutionsFree(&sols);
}

TEST(hpot_ensemble_diversification, hpot_all)
{
    pddl_pot_solutions_t sols;
    pddlPotSolutionsInit(&sols);

    pddl_hpot_config_t cfg = PDDL_HPOT_CONFIG_INIT;
    cfg.disambiguation = 1;

    pddl_hpot_config_opt_ensemble_diversification_t cfg_opt
            = PDDL_HPOT_CONFIG_OPT_ENSEMBLE_DIVERSIFICATION_INIT;
    cfg_opt.num_samples = 10;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_opt);

    int ret = pddlHPot(&sols, task, &cfg, &C.err);
    if (ret < 0){
        fprintf(stdout, "ret: %d\n", ret);
        pddlPotSolutionsFree(&sols);
        return;
    }
    assert(ret == 0);

    for (int i = 0; i < sols.sol_size; ++i){
        const pddl_pot_solution_t *sol = sols.sol + i;
        fprintf(stdout, "objval[%d]: %.0f\n", i, roundFlt(sol->objval));
    }

    pddlPotSolutionsFree(&sols);
}

TEST(hpot_ensemble_mutex_rand, hpot_all)
{
    pddl_pot_solutions_t sols;
    pddlPotSolutionsInit(&sols);

    pddl_hpot_config_t cfg = PDDL_HPOT_CONFIG_INIT;
    cfg.disambiguation = 1;

    pddl_hpot_config_opt_ensemble_all_states_mutex_t cfg_opt
            = PDDL_HPOT_CONFIG_OPT_ENSEMBLE_ALL_STATES_MUTEX_INIT;
    cfg_opt.cond_size = 1;
    cfg_opt.mutex_size = 1;
    cfg_opt.num_rand_samples = 10;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_opt);

    int ret = pddlHPot(&sols, task, &cfg, &C.err);
    if (ret < 0){
        fprintf(stdout, "ret: %d\n", ret);
        pddlPotSolutionsFree(&sols);
        return;
    }
    assert(ret == 0);

    for (int i = 0; i < sols.sol_size; ++i){
        const pddl_pot_solution_t *sol = sols.sol + i;
        fprintf(stdout, "objval[%d]: %.0f\n", i, roundFlt(sol->objval));
    }

    pddlPotSolutionsFree(&sols);
}

TEST(hpot_ensemble_mutex1, hpot_all)
{
    pddl_pot_solutions_t sols;
    pddlPotSolutionsInit(&sols);

    pddl_hpot_config_t cfg = PDDL_HPOT_CONFIG_INIT;
    cfg.disambiguation = 1;

    pddl_hpot_config_opt_ensemble_all_states_mutex_t cfg_opt
            = PDDL_HPOT_CONFIG_OPT_ENSEMBLE_ALL_STATES_MUTEX_INIT;
    cfg_opt.cond_size = 1;
    cfg_opt.mutex_size = 1;
    cfg_opt.num_rand_samples = 0;
    PDDL_HPOT_CONFIG_ADD(&cfg, &cfg_opt);

    int ret = pddlHPot(&sols, task, &cfg, &C.err);
    if (ret < 0){
        fprintf(stdout, "ret: %d\n", ret);
        pddlPotSolutionsFree(&sols);
        return;
    }
    assert(ret == 0);

    for (int i = 0; i < sols.sol_size; ++i){
        const pddl_pot_solution_t *sol = sols.sol + i;
        fprintf(stdout, "objval[%d]: %.0f\n", i, roundFlt(sol->objval));
    }

    pddlPotSolutionsFree(&sols);
}
#endif
