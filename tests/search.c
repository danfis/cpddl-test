#include "test.h"
#include "context.h"
#include <assert.h>

#if 0
static void printState(const char *prefix,
                       const pddl_iset_t *state,
                       const pddl_strips_maker_t *smaker)
{
    printf("%s", prefix);
    int fact;
    PDDL_ISET_FOR_EACH(state, fact){
        const pddl_ground_atom_t *ga;
        ga = pddlStripsMakerGroundAtomConst(smaker, fact);
        printf(" (%s", C.pddl.pred.pred[ga->pred].name);
        for (int i = 0; i < ga->arg_size; ++i){
            printf(" %s", C.pddl.obj.obj[ga->arg[i]].name);
        }
        printf(")");
    }
    printf("\n");
}

static void testSuccGen(pddl_lifted_app_action_backend_t backend)
{
    pddl_strips_maker_t smaker;
    pddlStripsMakerInit(&smaker, &C.pddl);

    PDDL_ISET(init);
    pddl_list_t *item;
    PDDL_LIST_FOR_EACH(&C.pddl.init->part, item){
        const pddl_fm_t *c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
        if (c->type == PDDL_FM_ATOM){
            const pddl_fm_atom_t *a = pddlFmToAtomConst(c);
            const pddl_ground_atom_t *ga;
            if (pddlPredIsStatic(&C.pddl.pred.pred[a->pred])){
                pddlStripsMakerAddStaticAtom(&smaker, a, NULL, NULL);
            }else{
                ga = pddlStripsMakerAddAtom(&smaker, a, NULL, NULL);
                pddlISetAdd(&init, ga->id);
            }

        }else if (c->type == PDDL_FM_ASSIGN){
            const pddl_fm_func_op_t *ass = pddlFmToFuncOpConst(c);
            pddlStripsMakerAddFunc(&smaker, ass, NULL, NULL);
        }
    }


    pddl_lifted_app_action_t *aa;
    aa = pddlAppActionNew(&C.pddl, backend, &C.err);
    assert(pddlAppActionSize(aa) == 0);

    int ret;
    pddlAppActionClearState(aa);

    PDDL_ISET(cur_state);
    pddlISetUnion(&cur_state, &init);
    for (int step = 0; step < 10; ++step){
        char prefix[32];
        sprintf(prefix, "Step %d", step);
        printState(prefix, &cur_state, &smaker);

        ret = pddlAppActionSetStripsState(aa, &smaker, &cur_state);
        assert(ret == 0);
        ret = pddlAppActionFindAppActions(aa);
        assert(ret == 0);
        pddlAppActionSort(aa, &C.pddl);

        int size = pddlAppActionSize(aa);
        printf("size: %d\n", size);
        if (size == 0)
            break;

        PDDL_ISET(next_state);
        for (int i = 0; i < size; ++i){
            int aid = pddlAppActionId(aa, i);
            const pddl_action_t *action = C.pddl.action.action + aid;
            const int *args = pddlAppActionArgs(aa, i);

            PDDL_ISET(add_eff);
            PDDL_ISET(del_eff);
            int cost;
            pddlStripsMakerActionEffInState(&smaker, action, args, &cur_state,
                                            &add_eff, &del_eff, &cost);
            if (!C.pddl.metric)
                cost = 1;


            printf("%s", action->name);
            for (int j = 0; j < action->param.param_size; ++j){
                printf(" %s", C.pddl.obj.obj[args[j]].name);
            }
            printf(" :: cost: %d\n", cost);
            printState("    add:", &add_eff, &smaker);
            printState("    del:", &del_eff, &smaker);

            if (i == step % size){
                pddlISetMinus2(&next_state, &cur_state, &del_eff);
                pddlISetUnion(&next_state, &add_eff);
            }

            pddlISetFree(&del_eff);
            pddlISetFree(&add_eff);
        }
        pddlISetEmpty(&cur_state);
        pddlISetUnion(&cur_state, &next_state);
        pddlISetFree(&next_state);
    }
    pddlISetFree(&cur_state);

    pddlAppActionDel(aa);
    pddlISetFree(&init);
    pddlStripsMakerFree(&smaker);
}

TEST(lifted_succ_gen_sql, pddl)
{
    testSuccGen(PDDL_APP_ACTION_SQL);
}

TEST(lifted_succ_gen_dl, pddl)
{
    testSuccGen(PDDL_APP_ACTION_DL);
}

#endif

TEST(search, fdr)
{
}

static void testOptimalSearch(const pddl_search_config_t *cfg)
{
    pddl_search_t *search;
    search = pddlSearchNew(cfg, &C.err);
    int ret = pddlSearchInitStep(search);
    assert(ret != PDDL_SEARCH_ABORT);
    while (ret == PDDL_SEARCH_CONT){
        ret = pddlSearchStep(search);
    }
    if (ret == PDDL_SEARCH_FOUND){
        pddl_plan_t plan;
        pddlPlanInit(&plan);
        pddlSearchExtractPlan(search, &plan);
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
    int ret = pddlSearchInitStep(search);
    assert(ret != PDDL_SEARCH_ABORT);
    while (ret == PDDL_SEARCH_CONT){
        ret = pddlSearchStep(search);
    }
    if (ret == PDDL_SEARCH_FOUND){
        pddl_plan_t plan;
        pddlPlanInit(&plan);
        pddlSearchExtractPlan(search, &plan);
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

static void _search(pddl_heur_t *heur, pddl_search_alg_t search)
{
    pddl_search_config_t cfg = PDDL_SEARCH_CONFIG_INIT;
    cfg.fdr = &C.fdr;
    cfg.alg = search;
    cfg.heur = heur;
    if (search == PDDL_SEARCH_ASTAR){
        testOptimalSearch(&cfg);
    }else{
        testGreedySearch(&cfg);
    }
    pddlHeurDel(heur);
}

TEST(search_blind, search)
{
    _search(pddlHeurBlind(), PDDL_SEARCH_ASTAR);
}

TEST(search_astar_hmax, search)
{
    _search(pddlHeurHMax(&C.fdr, &C.err), PDDL_SEARCH_ASTAR);
}

TEST(search_astar_lmc, search)
{
    _search(pddlHeurLMCut(&C.fdr, &C.err), PDDL_SEARCH_ASTAR);
}

TEST(search_gbfs_ff, search)
{
    _search(pddlHeurHFF(&C.fdr, &C.err), PDDL_SEARCH_GBFS);
}

TEST(search_lazy_ff, search)
{
    _search(pddlHeurHFF(&C.fdr, &C.err), PDDL_SEARCH_LAZY);
}

TEST(search_gbfs_add, search)
{
    _search(pddlHeurHAdd(&C.fdr, &C.err), PDDL_SEARCH_GBFS);
}

TEST(search_lazy_add, search)
{
    _search(pddlHeurHAdd(&C.fdr, &C.err), PDDL_SEARCH_LAZY);
}
