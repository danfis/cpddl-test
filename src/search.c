#include "test.h"
#include "context.h"
#include <assert.h>

TEST(search, fdr)
{
}

static void testOptimalSearch(const pddl_search_config_t *cfg)
{
    pddl_search_t *search;
    search = pddlSearchNew(cfg, &C.err);
    int ret = pddlSearchInitStep(search, &C.err);
    assert(ret != PDDL_SEARCH_ABORT);
    while (ret == PDDL_SEARCH_CONT){
        ret = pddlSearchStep(search, &C.err);
    }
    pddlSearchLogFullStats(search, &C.err);
    if (ret == PDDL_SEARCH_FOUND){
        pddl_plan_t plan;
        pddlPlanInit(&plan);
        int eret = pddlSearchExtractPlan(search, &plan, &C.err);
        assert(eret == 0);
        printf("Cost: %d\n", plan.cost);
        fflush(stdout);

        int val = validateGroundPlan(&C.fdr, &plan);
        assert(val == 0);
        if (C.optimal_cost >= 0)
            assert(plan.cost == C.optimal_cost);
        pddlPlanFree(&plan);

    }else{
        assert(C.optimal_cost < 0);
        printf("Unsolvable\n");
    }
    pddlSearchDel(search);
}

static void testGreedySearch(const pddl_search_config_t *cfg)
{
    pddl_search_t *search;
    search = pddlSearchNew(cfg, &C.err);
    int ret = pddlSearchInitStep(search, &C.err);
    assert(ret != PDDL_SEARCH_ABORT);
    while (ret == PDDL_SEARCH_CONT){
        ret = pddlSearchStep(search, &C.err);
    }
    pddlSearchLogFullStats(search, &C.err);
    if (ret == PDDL_SEARCH_FOUND){
        pddl_plan_t plan;
        pddlPlanInit(&plan);
        int eret = pddlSearchExtractPlan(search, &plan, &C.err);
        assert(eret == 0);
        //printf("Cost: %d\n", plan.cost);
        //fflush(stdout);

        int val = validateGroundPlan(&C.fdr, &plan);
        assert(val == 0);
        if (C.optimal_cost >= 0)
            assert(plan.cost >= C.optimal_cost);
        pddlPlanFree(&plan);

    }else{
        assert(C.optimal_cost < 0);
        printf("Unsolvable\n");
    }
    pddlSearchDel(search);
}

static void _search(pddl_heur_t *heur, pddl_search_alg_t search, int h_weight)
{
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = search;
    cfg.heur = heur;
    cfg.h_weight = h_weight;
    if (search == PDDL_SEARCH_ASTAR){
        testOptimalSearch(&cfg);
    }else{
        testGreedySearch(&cfg);
    }
    pddlHeurDel(heur);
}

TEST(search_blind, search)
{
    _search(pddlHeurBlind(), PDDL_SEARCH_ASTAR, 1);
}

TEST(search_astar_hmax, search)
{
    _search(pddlHeurHMax(&C.fdr, &C.err), PDDL_SEARCH_ASTAR, 1);
}

TEST(search_astar_lmc, search)
{
    _search(pddlHeurLMCut(&C.fdr, &C.err), PDDL_SEARCH_ASTAR, 1);
}

TEST(search_gbfs_ff, search)
{
    _search(pddlHeurHFF(&C.fdr, &C.err), PDDL_SEARCH_GBFS, 1);
}

TEST(search_lazy_ff, search)
{
    _search(pddlHeurHFF(&C.fdr, &C.err), PDDL_SEARCH_LAZY, 1);
}

TEST(search_gbfs_add, search)
{
    _search(pddlHeurHAdd(&C.fdr, &C.err), PDDL_SEARCH_GBFS, 1);
}

TEST(search_lazy_add, search)
{
    _search(pddlHeurHAdd(&C.fdr, &C.err), PDDL_SEARCH_LAZY, 1);
}

TEST(search_wastar_ff, search)
{
    _search(pddlHeurHFF(&C.fdr, &C.err), PDDL_SEARCH_WEIGHTED_ASTAR, 2);
}

/* Runs the search configured by CFG to the end; it fills STAT with its
 * statistics and returns the cost of the found (and validated) plan, or -1
 * if the task is unsolvable. */
static int runSearch(const pddl_search_config_t *cfg, pddl_search_stat_t *stat)
{
    pddl_search_t *search = pddlSearchNew(cfg, &C.err);
    assert(search != NULL);
    int ret = pddlSearchInitStep(search, &C.err);
    assert(ret != PDDL_SEARCH_ABORT);
    while (ret == PDDL_SEARCH_CONT)
        ret = pddlSearchStep(search, &C.err);
    pddlSearchStat(search, stat);

    int cost = -1;
    if (ret == PDDL_SEARCH_FOUND){
        pddl_plan_t plan;
        pddlPlanInit(&plan);
        int eret = pddlSearchExtractPlan(search, &plan, &C.err);
        assert(eret == 0);
        int val = validateGroundPlan(&C.fdr, &plan);
        assert(val == 0);
        cost = plan.cost;
        pddlPlanFree(&plan);
    }else{
        assert(ret == PDDL_SEARCH_UNSOLVABLE);
    }
    pddlSearchDel(search);
    return cost;
}

/*
 * Asserts that A* with LM-cut behaves the same with the state packer
 * LAYOUT (with the ILP time limit TIME_LIMIT) as with the default FFD
 * packer: the same plan cost and the same numbers of expanded, evaluated,
 * and generated states (state IDs do not depend on the packing).
 */
static void searchAStarLMCutPacker(pddl_fdr_state_packer_layout_t layout,
                                   float time_limit)
{
    pddl_heur_t *heur = pddlHeurLMCut(&C.fdr, &C.err);
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = PDDL_SEARCH_ASTAR;
    cfg.heur = heur;
    assert(cfg.state_pool.packer_cfg.layout
                == PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
    pddl_search_stat_t stat_ffd;
    int cost_ffd = runSearch(&cfg, &stat_ffd);

    cfg.state_pool.packer_cfg.layout = layout;
    cfg.state_pool.packer_cfg.ilp_time_limit = time_limit;
    pddl_search_stat_t stat;
    int cost = runSearch(&cfg, &stat);

    assert(cost_ffd == cost);
    if (C.optimal_cost >= 0)
        assert(cost == C.optimal_cost);
    assert(stat_ffd.expanded == stat.expanded);
    assert(stat_ffd.evaluated == stat.evaluated);
    assert(stat_ffd.generated == stat.generated);
    pddlHeurDel(heur);
}

/*
 * A* with LM-cut behaves the same with the ILP state packer as with the
 * default FFD packer (see searchAStarLMCutPacker()).
 */
TEST_COND(search_astar_lmc_packer_ilp, search, LP)
{
    searchAStarLMCutPacker(PDDL_FDR_STATE_PACKER_LAYOUT_ILP, 5.f);
}

/*
 * A* with LM-cut behaves the same with the effect affinity state packer
 * as with the default FFD packer (see searchAStarLMCutPacker()).
 */
TEST(search_astar_lmc_packer_eff_affinity, search)
{
    searchAStarLMCutPacker(PDDL_FDR_STATE_PACKER_LAYOUT_EFF_AFFINITY, 1.f);
}

/*
 * Asserts that A* with the heuristic HEUR behaves the same with the state
 * pool of the type TYPE (the tree used for all states, the check of
 * tree-or-ffd-array at CHECK_STATES states with the minimum ratio
 * MIN_RATIO, and the incremental insertion of states if INCREMENTAL_INSERT
 * is true) as with the default array of the state pool: the same plan cost
 * and the same numbers of expanded, evaluated, and generated states (state
 * IDs do not depend on the representation of the pool).
 */
static void searchAStarPool(pddl_heur_t *heur,
                            pddl_fdr_state_pool_type_t type,
                            int check_states,
                            float min_ratio,
                            pddl_bool_t incremental_insert)
{
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = PDDL_SEARCH_ASTAR;
    cfg.heur = heur;
    assert(cfg.state_pool.type == PDDL_FDR_STATE_POOL_ARRAY);
    pddl_search_stat_t stat_array;
    int cost_array = runSearch(&cfg, &stat_array);

    cfg.state_pool.type = type;
    cfg.state_pool.tree_min_state_size = 0;
    cfg.state_pool.fallback_to_ffd_array_check_states = check_states;
    cfg.state_pool.fallback_to_ffd_array_min_ratio = min_ratio;
    cfg.state_pool_incremental_insert = incremental_insert;
    pddl_search_stat_t stat;
    int cost = runSearch(&cfg, &stat);

    assert(cost_array == cost);
    if (C.optimal_cost >= 0)
        assert(cost == C.optimal_cost);
    assert(stat_array.expanded == stat.expanded);
    assert(stat_array.evaluated == stat.evaluated);
    assert(stat_array.generated == stat.generated);
    pddlHeurDel(heur);
}

/*
 * Blind A* behaves the same with the tree representation of the state pool
 * as with the array (see searchAStarPool()).
 */
TEST(search_blind_pool_tree, search)
{
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE, 1, 1.f,
                    pddl_false);
}

/*
 * A* with LM-cut behaves the same with the tree representation of the
 * state pool as with the array (see searchAStarPool()).
 */
TEST(search_astar_lmc_pool_tree, search)
{
    searchAStarPool(pddlHeurLMCut(&C.fdr, &C.err),
                    PDDL_FDR_STATE_POOL_TREE, 1, 1.f, pddl_false);
}

/*
 * Blind A* behaves the same with tree-or-ffd-array as with the array (see
 * searchAStarPool()): with the check at 10 states and a minimum ratio that
 * is never reached, so the pool switches from the tree to the array of
 * FFD-packed states during the search (if it stores at least 10 states),
 * and with the minimum ratio 0, so it keeps the tree.
 */
TEST(search_blind_pool_tree_or_ffd_array, search)
{
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE_OR_FFD_ARRAY,
                    10, 1e9f, pddl_false);
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE_OR_FFD_ARRAY,
                    10, 0.f, pddl_false);
}

/*
 * A* with LM-cut behaves the same with tree-or-ffd-array switching to the
 * array during the search as with the array (see
 * search_blind_pool_tree_or_ffd_array).
 */
TEST(search_astar_lmc_pool_tree_or_ffd_array, search)
{
    searchAStarPool(pddlHeurLMCut(&C.fdr, &C.err),
                    PDDL_FDR_STATE_POOL_TREE_OR_FFD_ARRAY, 10, 1e9f,
                    pddl_false);
}

/*
 * Blind A* behaves the same with the incremental insertion into the tree
 * representation of the state pool as with the array (see
 * searchAStarPool()).
 */
TEST(search_blind_pool_tree_incremental, search)
{
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE, 1, 1.f,
                    pddl_true);
}

/*
 * A* with LM-cut behaves the same with the incremental insertion into the
 * tree representation of the state pool as with the array (see
 * searchAStarPool()).
 */
TEST(search_astar_lmc_pool_tree_incremental, search)
{
    searchAStarPool(pddlHeurLMCut(&C.fdr, &C.err),
                    PDDL_FDR_STATE_POOL_TREE, 1, 1.f, pddl_true);
}

/*
 * Blind A* behaves the same with the incremental insertion into
 * tree-or-ffd-array as with the array (see
 * search_blind_pool_tree_or_ffd_array).
 */
TEST(search_blind_pool_tree_or_ffd_array_incremental, search)
{
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE_OR_FFD_ARRAY,
                    10, 1e9f, pddl_true);
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE_OR_FFD_ARRAY,
                    10, 0.f, pddl_true);
}

/*
 * Asserts that the greedy search ALG with the heuristic HEUR never re-opens
 * closed states by default, and that with
 * pddl_search_config_t.greedy_reopen_closed_states it finds a valid plan
 * (no cheaper than the optimal one) iff the default configuration does.
 */
static void searchGreedyReopen(pddl_heur_t *heur, pddl_search_alg_t alg)
{
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = alg;
    cfg.heur = heur;
    assert(!cfg.greedy_reopen_closed_states);
    pddl_search_stat_t stat;
    int cost = runSearch(&cfg, &stat);
    assert(stat.reopen == 0);

    cfg.greedy_reopen_closed_states = pddl_true;
    pddl_search_stat_t stat_reopen;
    int cost_reopen = runSearch(&cfg, &stat_reopen);

    assert((cost < 0) == (cost_reopen < 0));
    if (cost_reopen < 0){
        assert(C.optimal_cost < 0);
    }else if (C.optimal_cost >= 0){
        assert(cost_reopen >= C.optimal_cost);
    }
    pddlHeurDel(heur);
}

/*
 * GBFS with FF re-opens closed states only if configured to do so (see
 * searchGreedyReopen()).
 */
TEST(search_gbfs_ff_reopen, search)
{
    searchGreedyReopen(pddlHeurHFF(&C.fdr, &C.err), PDDL_SEARCH_GBFS);
}

/*
 * Lazy GBFS with FF re-opens closed states only if configured to do so (see
 * searchGreedyReopen()).
 */
TEST(search_lazy_ff_reopen, search)
{
    searchGreedyReopen(pddlHeurHFF(&C.fdr, &C.err), PDDL_SEARCH_LAZY);
}

/* Runs the search configured by CFG to the end; it fills STAT with its
 * statistics and returns the final status. */
static pddl_search_status_t runSearchStatus(const pddl_search_config_t *cfg,
                                            pddl_search_stat_t *stat)
{
    pddl_search_t *search = pddlSearchNew(cfg, &C.err);
    assert(search != NULL);
    pddl_search_status_t ret = pddlSearchInitStep(search, &C.err);
    while (ret == PDDL_SEARCH_CONT)
        ret = pddlSearchStep(search, &C.err);
    pddlSearchStat(search, stat);
    pddlSearchDel(search);
    return ret;
}

/* Initializes FDR as a copy of C.fdr with the cost of every operator set to
 * COST. */
static void initFDRWithOpCost(pddl_fdr_t *fdr, int cost)
{
    pddlFDRInitCopy(fdr, &C.fdr);
    for (int i = 0; i < fdr->op.op_size; ++i)
        fdr->op.op[i]->cost = cost;
}

/*
 * Blind A* on the task with all operator costs set to INT_MAX / 2: the
 * g-values of states up to the depth 2 fit into int, deeper states have
 * saturated g-values. A* either finds a plan of length at most 2, or it
 * terminates with PDDL_SEARCH_ABORT on the first popped state with a
 * saturated f-value.
 */
TEST(search_astar_saturated_cost, search)
{
    const int cost = INT_MAX / 2;
    pddl_fdr_t fdr;
    initFDRWithOpCost(&fdr, cost);

    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &fdr;
    cfg.alg = PDDL_SEARCH_ASTAR;
    pddl_search_t *search = pddlSearchNew(&cfg, &C.err);
    assert(search != NULL);
    pddl_search_status_t ret = pddlSearchInitStep(search, &C.err);
    while (ret == PDDL_SEARCH_CONT)
        ret = pddlSearchStep(search, &C.err);
    pddl_search_stat_t stat;
    pddlSearchStat(search, &stat);

    switch (ret){
    case PDDL_SEARCH_CONT:
        assert(0);
        break;
    case PDDL_SEARCH_UNSOLVABLE:
        assert(C.optimal_cost < 0);
        assert(pddlNumIsFinite(&stat.last_f_value));
        break;
    case PDDL_SEARCH_FOUND:
        {
            pddl_plan_t plan;
            pddlPlanInit(&plan);
            int eret = pddlSearchExtractPlan(search, &plan, &C.err);
            assert(eret == 0);
            assert(plan.length <= 2);
            assert(plan.cost == plan.length * cost);
            int val = validateGroundPlan(&fdr, &plan);
            assert(val == 0);
            pddlPlanFree(&plan);
        }
        break;
    case PDDL_SEARCH_ABORT:
        assert(pddlNumIsHuge(&stat.last_f_value));
        assert(stat.expanded > 0);
        break;
    }

    pddlSearchDel(search);
    pddlFDRFree(&fdr);
}

/*
 * Weighted A* with LM-cut and the weight INT_MAX: f = g + w * h saturates
 * for almost every state, so the search either terminates with
 * PDDL_SEARCH_ABORT with a saturated last f-value, or it ends regularly
 * (with a valid plan if found).
 */
TEST(search_wastar_saturated_weight, search)
{
    pddl_heur_t *heur = pddlHeurLMCut(&C.fdr, &C.err);
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = PDDL_SEARCH_WEIGHTED_ASTAR;
    cfg.heur = heur;
    cfg.h_weight = INT_MAX;
    pddl_search_t *search = pddlSearchNew(&cfg, &C.err);
    assert(search != NULL);
    pddl_search_status_t ret = pddlSearchInitStep(search, &C.err);
    while (ret == PDDL_SEARCH_CONT)
        ret = pddlSearchStep(search, &C.err);
    pddl_search_stat_t stat;
    pddlSearchStat(search, &stat);

    switch (ret){
    case PDDL_SEARCH_CONT:
        assert(0);
        break;
    case PDDL_SEARCH_UNSOLVABLE:
        assert(C.optimal_cost < 0);
        break;
    case PDDL_SEARCH_FOUND:
        {
            pddl_plan_t plan;
            pddlPlanInit(&plan);
            int eret = pddlSearchExtractPlan(search, &plan, &C.err);
            assert(eret == 0);
            int val = validateGroundPlan(&C.fdr, &plan);
            assert(val == 0);
            if (C.optimal_cost >= 0)
                assert(plan.cost >= C.optimal_cost);
            pddlPlanFree(&plan);
        }
        break;
    case PDDL_SEARCH_ABORT:
        assert(pddlNumIsHuge(&stat.last_f_value));
        break;
    }

    pddlSearchDel(search);
    pddlHeurDel(heur);
}

/*
 * GBFS (guided by FF computed on the original task) on the task with all
 * operator costs set to INT_MAX / 2: the saturated g-values do not affect
 * the greedy search, which never aborts and finds a plan iff it finds one
 * on the original task.
 */
TEST(search_gbfs_saturated_cost, search)
{
    pddl_heur_t *heur = pddlHeurHFF(&C.fdr, &C.err);
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = PDDL_SEARCH_GBFS;
    cfg.heur = heur;
    pddl_search_stat_t stat;
    int orig_cost = runSearch(&cfg, &stat);

    pddl_fdr_t fdr;
    initFDRWithOpCost(&fdr, INT_MAX / 2);
    cfg.fdr = &fdr;
    pddl_search_status_t ret = runSearchStatus(&cfg, &stat);

    switch (ret){
    case PDDL_SEARCH_CONT:
    case PDDL_SEARCH_ABORT:
        assert(0);
        break;
    case PDDL_SEARCH_UNSOLVABLE:
        assert(orig_cost < 0);
        break;
    case PDDL_SEARCH_FOUND:
        assert(orig_cost >= 0);
        break;
    }

    pddlFDRFree(&fdr);
    pddlHeurDel(heur);
}

/* Runs the search configured by CFG to the end; it fills STAT with its
 * statistics, sets *COST to the cost of the found (and validated) plan, or
 * to -1 if no plan was found, and returns the final status. */
static pddl_search_status_t runSearchPlan(const pddl_search_config_t *cfg,
                                          pddl_search_stat_t *stat,
                                          int *cost)
{
    pddl_search_t *search = pddlSearchNew(cfg, &C.err);
    assert(search != NULL);
    pddl_search_status_t ret = pddlSearchInitStep(search, &C.err);
    while (ret == PDDL_SEARCH_CONT)
        ret = pddlSearchStep(search, &C.err);
    pddlSearchStat(search, stat);

    *cost = -1;
    if (ret == PDDL_SEARCH_FOUND){
        pddl_plan_t plan;
        pddlPlanInit(&plan);
        int eret = pddlSearchExtractPlan(search, &plan, &C.err);
        assert(eret == 0);
        int val = validateGroundPlan(cfg->fdr, &plan);
        assert(val == 0);
        *cost = plan.cost;
        pddlPlanFree(&plan);
    }
    pddlSearchDel(search);
    return ret;
}

/* Asserts that the search configured by CFG does not find a plan within its
 * upper bound on the plan cost. */
static void assertNoPlanWithinUpperBound(const pddl_search_config_t *cfg)
{
    pddl_search_stat_t stat;
    int cost;
    pddl_search_status_t ret = runSearchPlan(cfg, &stat, &cost);
    switch (ret){
    case PDDL_SEARCH_CONT:
    case PDDL_SEARCH_FOUND:
    case PDDL_SEARCH_ABORT:
        assert(0);
        break;
    case PDDL_SEARCH_UNSOLVABLE:
        break;
    }
}

/*
 * A* with LM-cut and an upper bound on the plan cost: it finds an optimal
 * plan if the bound is the optimal cost, and no plan if the bound is the
 * strict bound given as optimal - eps.
 */
TEST(search_astar_upper_bound, search)
{
    pddl_heur_t *heur = pddlHeurLMCut(&C.fdr, &C.err);
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = PDDL_SEARCH_ASTAR;
    cfg.heur = heur;

    if (C.optimal_cost < 0){
        pddlNumSetInt(&cfg.plan_cost_upper_bound, 0);
        assertNoPlanWithinUpperBound(&cfg);
        pddlHeurDel(heur);
        return;
    }

    pddlNumSetInt(&cfg.plan_cost_upper_bound, C.optimal_cost);
    pddl_search_stat_t stat;
    int cost;
    pddl_search_status_t ret = runSearchPlan(&cfg, &stat, &cost);
    switch (ret){
    case PDDL_SEARCH_CONT:
    case PDDL_SEARCH_UNSOLVABLE:
    case PDDL_SEARCH_ABORT:
        assert(0);
        break;
    case PDDL_SEARCH_FOUND:
        assert(cost == C.optimal_cost);
        break;
    }

    pddlNumSetIntDelta(&cfg.plan_cost_upper_bound, C.optimal_cost, -1);
    ret = runSearchPlan(&cfg, &stat, &cost);
    switch (ret){
    case PDDL_SEARCH_CONT:
    case PDDL_SEARCH_FOUND:
    case PDDL_SEARCH_ABORT:
        assert(0);
        break;
    case PDDL_SEARCH_UNSOLVABLE:
        if (C.optimal_cost > 0)
            assert(stat.pruned_by_upper_bound > 0);
        break;
    }

    pddlHeurDel(heur);
}

/*
 * GBFS with FF and the optimal cost as the upper bound on the plan cost:
 * with re-opening of closed states, it always finds a plan within the
 * bound; without re-opening, any plan it finds is within the bound.
 */
TEST(search_gbfs_upper_bound, search)
{
    if (C.optimal_cost < 0)
        return;

    pddl_heur_t *heur = pddlHeurHFF(&C.fdr, &C.err);
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = PDDL_SEARCH_GBFS;
    cfg.heur = heur;
    cfg.greedy_reopen_closed_states = pddl_true;
    pddlNumSetInt(&cfg.plan_cost_upper_bound, C.optimal_cost);

    pddl_search_stat_t stat;
    int cost;
    pddl_search_status_t ret = runSearchPlan(&cfg, &stat, &cost);
    switch (ret){
    case PDDL_SEARCH_CONT:
    case PDDL_SEARCH_UNSOLVABLE:
    case PDDL_SEARCH_ABORT:
        assert(0);
        break;
    case PDDL_SEARCH_FOUND:
        assert(cost == C.optimal_cost);
        break;
    }

    cfg.greedy_reopen_closed_states = pddl_false;
    ret = runSearchPlan(&cfg, &stat, &cost);
    switch (ret){
    case PDDL_SEARCH_CONT:
    case PDDL_SEARCH_ABORT:
        assert(0);
        break;
    case PDDL_SEARCH_UNSOLVABLE:
        break;
    case PDDL_SEARCH_FOUND:
        assert(cost == C.optimal_cost);
        break;
    }

    pddlHeurDel(heur);
}

/*
 * A negative upper bound on the plan cost prunes the initial state, so the
 * search terminates already in pddlSearchInitStep().
 */
TEST(search_upper_bound_negative, search)
{
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = PDDL_SEARCH_ASTAR;
    pddlNumSetInt(&cfg.plan_cost_upper_bound, -1);

    pddl_search_t *search = pddlSearchNew(&cfg, &C.err);
    assert(search != NULL);
    pddl_search_status_t ret = pddlSearchInitStep(search, &C.err);
    assert(ret == PDDL_SEARCH_UNSOLVABLE);
    pddl_search_stat_t stat;
    pddlSearchStat(search, &stat);
    assert(stat.expanded == 0);
    assert(stat.evaluated == 0);
    assert(stat.pruned_by_upper_bound == 1);
    pddlSearchDel(search);
}
