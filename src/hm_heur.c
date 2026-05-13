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
TEST(h2_heur_dyn_prog_plan_states, fdr)
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
        int h2 = pddlHMHeurDynProg(2, &C.strips, &plan.state[i], NULL, &C.err);
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
    int h2 = pddlHMHeurDynProg(2, &C.strips, &C.strips.init, NULL, &C.err);
    fprintf(stdout, "h^2(I) = %d\n", h2);

    if (C.optimal_cost >= 0)
        assert(h2 <= C.optimal_cost);

    pddl_hmax_t hmax;
    pddlHMaxInitStrips(&hmax, &C.strips);
    int hm = pddlHMaxStrips(&hmax, &C.strips.init);
    assert(h2 == PDDL_COST_DEAD_END || h2 >= hm);
    pddlHMaxFree(&hmax);
}

TEST(h2_heur_dyn_prog_mutex, fdr)
{
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &C.strips);
    pddlMutexPairsAddMGroups(&mutex, &C.mg);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfg.strips = &C.strips;
    cfg.mgroups = &C.mg;
    cfg.mutex_pairs = &mutex;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    int hval_no_mutex = pddlHMHeurDynProg(2, &C.strips, &C.strips.init, NULL, &C.err);
    int hval = pddlHMHeurDynProg(2, &C.strips, &C.strips.init, &mutex, &C.err);
    printf("h^2(I) = %d | without-mutex: %d\n", hval, hval_no_mutex);
    assert(hval >= hval_no_mutex);

    if (C.optimal_cost >= 0)
        assert(hval <= C.optimal_cost);

    if (C.optimal_cost < 0){
        pddlMutexPairsFree(&mutex);
        return;
    }
    if (C.plan_file_path[0] == '\x0'){
        pddlMutexPairsFree(&mutex);
        return;
    }

    pddl_plan_file_strips_t plan;
    ret = pddlPlanFileStripsInit(&plan, &C.strips, C.plan_file_path, &C.err);
    if (ret < 0){
        pddlMutexPairsFree(&mutex);
        return;
    }

    int cost = plan.cost;
    for (int i = 0; i < plan.state_size; ++i){
        int hval_no_mutex = pddlHMHeurDynProg(2, &C.strips, &plan.state[i], NULL, &C.err);
        int hval = pddlHMHeurDynProg(2, &C.strips, &plan.state[i], &mutex, &C.err);

        assert(hval <= cost);
        assert(hval >= hval_no_mutex);

        /*
        if (hval > hval_no_mutex)
            printf("h^2(s[%d]) = %d | without-mutex: %d\n", i, hval, hval_no_mutex);
        */

        if (i < pddlIArrSize(&plan.op))
            cost -= C.strips.op.op[pddlIArrGet(&plan.op, i)]->cost;
    }

    pddlPlanFileStripsFree(&plan);
    pddlMutexPairsFree(&mutex);
}

/*
 * Initialize the bitset-based h^2 heuristic and compare its result on the
 * initial state against the reference (dynamic programming) implementation.
 */
TEST(h2_heur_bitset_cmp_init_state, h2_heur_dyn_prog_init_state)
{
    pddl_h2_heur_bitset_t h;
    int ret = pddlH2HeurBitsetInit(&h, &C.strips, NULL, &C.err);
    assert(ret == 0);
    int h2 = pddlH2HeurBitset(&h, &C.strips.init);
    int ref = pddlHMHeurDynProg(2, &C.strips, &C.strips.init, NULL, &C.err);
    assert(h2 == ref);
    pddlH2HeurBitsetFree(&h);
}

TEST(h2_heur_bitset_init_state, strips)
{
    pddl_h2_heur_bitset_t h;
    int ret = pddlH2HeurBitsetInit(&h, &C.strips, NULL, &C.err);
    assert(ret == 0);
    int h2 = pddlH2HeurBitset(&h, &C.strips.init);
    if (C.optimal_cost >= 0)
        assert(h2 <= C.optimal_cost);
    fprintf(stdout, "h^2(I) = %d\n", h2);
    pddlH2HeurBitsetFree(&h);
}

TEST(h2_heur_bitset_init_state_mutex, strips)
{
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &C.strips);
    pddlMutexPairsAddMGroups(&mutex, &C.mg);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfg.strips = &C.strips;
    cfg.mgroups = &C.mg;
    cfg.mutex_pairs = &mutex;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    pddl_h2_heur_bitset_t h;
    ret = pddlH2HeurBitsetInit(&h, &C.strips, &mutex, &C.err);
    assert(ret == 0);
    pddl_h2_heur_bitset_t h_no_mutex;
    ret = pddlH2HeurBitsetInit(&h_no_mutex, &C.strips, NULL, &C.err);
    assert(ret == 0);
    int h2 = pddlH2HeurBitset(&h, &C.strips.init);
    int h2_no_mutex = pddlH2HeurBitset(&h_no_mutex, &C.strips.init);
    if (C.optimal_cost >= 0)
        assert(h2 <= C.optimal_cost);
    fprintf(stdout, "h^2(I) = %d | no-mutex: %d\n", h2, h2_no_mutex);
    assert(h2 >= h2_no_mutex);
    pddlH2HeurBitsetFree(&h);
    pddlH2HeurBitsetFree(&h_no_mutex);
    pddlMutexPairsFree(&mutex);
}

/*
 * Walk through all optimal-plan intermediate states and compare the
 * bitset-based h^2 heuristic against the reference implementation at each
 * state.
 */
TEST(h2_heur_bitset_cmp_plan_states, h2_heur_dyn_prog_plan_states)
{
    if (C.optimal_cost < 0 || C.plan_file_path[0] == '\x0')
        return;

    pddl_plan_file_strips_t plan;
    int ret = pddlPlanFileStripsInit(&plan, &C.strips, C.plan_file_path, &C.err);
    if (ret < 0)
        return;

    pddl_h2_heur_bitset_t h;
    ret = pddlH2HeurBitsetInit(&h, &C.strips, NULL, &C.err);
    assert(ret == 0);

    for (int i = 0; i < plan.state_size; ++i){
        int h2 = pddlH2HeurBitset(&h, &plan.state[i]);
        int ref = pddlHMHeurDynProg(2, &C.strips, &plan.state[i], NULL, &C.err);
        assert(h2 == ref);
    }

    pddlH2HeurBitsetFree(&h);
    pddlPlanFileStripsFree(&plan);
}

TEST(h2_heur_bitset_plan_states, fdr)
{
    if (C.optimal_cost < 0 || C.plan_file_path[0] == '\x0')
        return;

    pddl_plan_file_strips_t plan;
    int ret = pddlPlanFileStripsInit(&plan, &C.strips, C.plan_file_path, &C.err);
    if (ret < 0)
        return;

    pddl_h2_heur_bitset_t h;
    ret = pddlH2HeurBitsetInit(&h, &C.strips, NULL, &C.err);
    assert(ret == 0);

    int cost = plan.cost;
    for (int i = 0; i < plan.state_size; ++i){
        int h2 = pddlH2HeurBitset(&h, &plan.state[i]);
        assert(h2 <= cost);
        if (i < pddlIArrSize(&plan.op))
            cost -= C.strips.op.op[pddlIArrGet(&plan.op, i)]->cost;
    }

    pddlH2HeurBitsetFree(&h);
    pddlPlanFileStripsFree(&plan);
}

TEST(h2_heur_bitset_plan_states_mutex, fdr)
{
    if (C.optimal_cost < 0 || C.plan_file_path[0] == '\x0')
        return;

    pddl_plan_file_strips_t plan;
    int ret = pddlPlanFileStripsInit(&plan, &C.strips, C.plan_file_path, &C.err);
    if (ret < 0)
        return;

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &C.strips);
    pddlMutexPairsAddMGroups(&mutex, &C.mg);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfg.strips = &C.strips;
    cfg.mgroups = &C.mg;
    cfg.mutex_pairs = &mutex;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;

    ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    pddl_h2_heur_bitset_t h;
    ret = pddlH2HeurBitsetInit(&h, &C.strips, &mutex, &C.err);
    assert(ret == 0);
    pddl_h2_heur_bitset_t h_no_mutex;
    ret = pddlH2HeurBitsetInit(&h_no_mutex, &C.strips, NULL, &C.err);
    assert(ret == 0);
    pddlMutexPairsFree(&mutex);

    int cost = plan.cost;
    for (int i = 0; i < plan.state_size; ++i){
        int h2 = pddlH2HeurBitset(&h, &plan.state[i]);
        int h2_no_mutex = pddlH2HeurBitset(&h_no_mutex, &plan.state[i]);
        assert(h2 <= cost);
        assert(h2_no_mutex <= h2);
        if (i < pddlIArrSize(&plan.op))
            cost -= C.strips.op.op[pddlIArrGet(&plan.op, i)]->cost;
    }

    pddlH2HeurBitsetFree(&h);
    pddlH2HeurBitsetFree(&h_no_mutex);
    pddlPlanFileStripsFree(&plan);
}

/*
 * Compare the bitset-based h^2 heuristic against the reference when mutex
 * information is used.
 */
TEST(h2_heur_bitset_cmp_mutex, h2_heur_dyn_prog_mutex)
{
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &C.strips);
    pddlMutexPairsAddMGroups(&mutex, &C.mg);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfg.strips = &C.strips;
    cfg.mgroups = &C.mg;
    cfg.mutex_pairs = &mutex;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;

    int hm_ret = pddlHm(&cfg, &res, &C.err);
    assert(hm_ret == 0);

    pddl_h2_heur_bitset_t h_mutex, h_no_mutex;
    int ret = pddlH2HeurBitsetInit(&h_mutex, &C.strips, &mutex, &C.err);
    assert(ret == 0);
    ret = pddlH2HeurBitsetInit(&h_no_mutex, &C.strips, NULL, &C.err);
    assert(ret == 0);

    int h2 = pddlH2HeurBitset(&h_mutex, &C.strips.init);
    int ref = pddlHMHeurDynProg(2, &C.strips, &C.strips.init, &mutex, &C.err);
    assert(h2 == ref);

    int h2_no_m = pddlH2HeurBitset(&h_no_mutex, &C.strips.init);
    int ref_no_m = pddlHMHeurDynProg(2, &C.strips, &C.strips.init, NULL, &C.err);
    assert(h2_no_m == ref_no_m);
    assert(h2 >= h2_no_m);

    if (C.optimal_cost >= 0 && C.plan_file_path[0] != '\x0'){
        pddl_plan_file_strips_t plan;
        int ret = pddlPlanFileStripsInit(&plan, &C.strips, C.plan_file_path, &C.err);
        if (ret == 0){
            for (int i = 0; i < plan.state_size; ++i){
                int h2_p = pddlH2HeurBitset(&h_mutex, &plan.state[i]);
                int ref_p = pddlHMHeurDynProg(2, &C.strips, &plan.state[i],
                                              &mutex, &C.err);
                assert(h2_p == ref_p);
            }
            pddlPlanFileStripsFree(&plan);
        }
    }

    pddlH2HeurBitsetFree(&h_mutex);
    pddlH2HeurBitsetFree(&h_no_mutex);
    pddlMutexPairsFree(&mutex);
}
