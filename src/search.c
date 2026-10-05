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
 * A* with LM-cut behaves the same with the ILP state packer as with the
 * default FFD packer: the same plan cost and the same numbers of expanded,
 * evaluated, and generated states (state IDs do not depend on the
 * packing).
 */
TEST_COND(search_astar_lmc_packer_ilp, search, LP)
{
    pddl_heur_t *heur = pddlHeurLMCut(&C.fdr, &C.err);
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = PDDL_SEARCH_ASTAR;
    cfg.heur = heur;
    assert(cfg.state_packer.layout == PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
    pddl_search_stat_t stat_ffd;
    int cost_ffd = runSearch(&cfg, &stat_ffd);

    cfg.state_packer.layout = PDDL_FDR_STATE_PACKER_LAYOUT_ILP;
    cfg.state_packer.ilp_time_limit = 5.f;
    pddl_search_stat_t stat_ilp;
    int cost_ilp = runSearch(&cfg, &stat_ilp);

    assert(cost_ffd == cost_ilp);
    if (C.optimal_cost >= 0)
        assert(cost_ilp == C.optimal_cost);
    assert(stat_ffd.expanded == stat_ilp.expanded);
    assert(stat_ffd.evaluated == stat_ilp.evaluated);
    assert(stat_ffd.generated == stat_ilp.generated);
    pddlHeurDel(heur);
}
