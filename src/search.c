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
 * tree-ffd at CHECK_STATES states with the minimum ratio MIN_RATIO, and
 * the incremental insertion of states if INCREMENTAL_INSERT is true) as
 * with the default array of the state pool: the same plan cost and the
 * same numbers of expanded, evaluated, and generated states (state IDs do
 * not depend on the representation of the pool).
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
    assert(!cfg.state_pool_incremental_insert);
    pddl_search_stat_t stat_array;
    int cost_array = runSearch(&cfg, &stat_array);

    cfg.state_pool.type = type;
    cfg.state_pool.tree_min_state_size = 0;
    cfg.state_pool.tree_ffd_check_states = check_states;
    cfg.state_pool.tree_ffd_min_ratio = min_ratio;
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
 * Blind A* behaves the same with tree-ffd as with the array (see
 * searchAStarPool()): with the check at 10 states and a minimum ratio that
 * is never reached, so the pool switches from the tree to the array of
 * FFD-packed states during the search (if it stores at least 10 states),
 * and with the minimum ratio 0, so it keeps the tree.
 */
TEST(search_blind_pool_tree_ffd, search)
{
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE_FFD, 10, 1e9f,
                    pddl_false);
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE_FFD, 10, 0.f,
                    pddl_false);
}

/*
 * A* with LM-cut behaves the same with tree-ffd switching to the array
 * during the search as with the array (see search_blind_pool_tree_ffd).
 */
TEST(search_astar_lmc_pool_tree_ffd, search)
{
    searchAStarPool(pddlHeurLMCut(&C.fdr, &C.err),
                    PDDL_FDR_STATE_POOL_TREE_FFD, 10, 1e9f, pddl_false);
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
 * Blind A* behaves the same with the incremental insertion into tree-ffd
 * as with the array (see search_blind_pool_tree_ffd).
 */
TEST(search_blind_pool_tree_ffd_incremental, search)
{
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE_FFD, 10, 1e9f,
                    pddl_true);
    searchAStarPool(pddlHeurBlind(), PDDL_FDR_STATE_POOL_TREE_FFD, 10, 0.f,
                    pddl_true);
}
