#include "test.h"
#include "context.h"
#include <assert.h>

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
    aa = pddlLiftedAppActionNew(&C.pddl, backend, &C.err);
    assert(pddlLiftedAppActionSize(aa) == 0);

    int ret;
    pddlLiftedAppActionClearState(aa);

    PDDL_ISET(cur_state);
    pddlISetUnion(&cur_state, &init);
    for (int step = 0; step < 10; ++step){
        char prefix[32];
        sprintf(prefix, "Step %d", step);
        printState(prefix, &cur_state, &smaker);

        ret = pddlLiftedAppActionSetStripsState(aa, &smaker, &cur_state);
        assert(ret == 0);
        ret = pddlLiftedAppActionFindAppActions(aa);
        assert(ret == 0);
        pddlLiftedAppActionSort(aa, &C.pddl);

        int size = pddlLiftedAppActionSize(aa);
        printf("size: %d\n", size);
        if (size == 0)
            break;

        PDDL_ISET(next_state);
        for (int i = 0; i < size; ++i){
            int aid = pddlLiftedAppActionId(aa, i);
            const pddl_action_t *action = C.pddl.action.action + aid;
            const int *args = pddlLiftedAppActionArgs(aa, i);

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

    pddlLiftedAppActionDel(aa);
    pddlISetFree(&init);
    pddlStripsMakerFree(&smaker);
}

TEST(lifted_succ_gen_sql, pddl)
{
    testSuccGen(PDDL_LIFTED_APP_ACTION_SQL);
}

TEST(lifted_succ_gen_dl, pddl)
{
    testSuccGen(PDDL_LIFTED_APP_ACTION_DL);
}

TEST(lifted_search, pddl)
{
}

TEST(lifted_search_unit_cost, pddl_unit_cost)
{
}

static void testOptimalSearch(const pddl_lifted_search_config_t *cfg,
                              int compare_to_optimal_cost)
{
    pddl_lifted_search_t *search;
    search = pddlLiftedSearchNew(cfg, &C.err);
    int ret = pddlLiftedSearchInitStep(search);
    assert(ret != PDDL_LIFTED_SEARCH_ABORT);
    while (ret == PDDL_LIFTED_SEARCH_CONT){
        ret = pddlLiftedSearchStep(search);
    }
    if (ret == PDDL_LIFTED_SEARCH_FOUND){
        const pddl_lifted_plan_t *plan = pddlLiftedSearchPlan(search);
        printf("Cost: %d\n", plan->plan_cost);
        fflush(stdout);
        int val = validateLiftedPlan(plan);
        assert(val == 0);
        if (compare_to_optimal_cost && C.optimal_cost >= 0)
            assert(plan->plan_cost == C.optimal_cost);

    }else{
        assert(C.optimal_cost < 0);
        printf("Unsolvable\n");
    }
    pddlLiftedSearchDel(search);
}

static void testGreedySearch(const pddl_lifted_search_config_t *cfg)
{
    pddl_lifted_search_t *search;
    search = pddlLiftedSearchNew(cfg, &C.err);
    int ret = pddlLiftedSearchInitStep(search);
    assert(ret != PDDL_LIFTED_SEARCH_ABORT);
    while (ret == PDDL_LIFTED_SEARCH_CONT){
        ret = pddlLiftedSearchStep(search);
    }
    pddl_lifted_search_stat_t stat;
    pddlLiftedSearchStat(search, &stat);
    printf("Expanded: %lu\n", (unsigned long)stat.expanded);

    if (ret == PDDL_LIFTED_SEARCH_FOUND){
        const pddl_lifted_plan_t *plan = pddlLiftedSearchPlan(search);
        //printf("Cost: %d\n", plan->plan_cost);
        int val = validateLiftedPlan(plan);
        assert(val == 0);
        if (C.optimal_cost >= 0)
            assert(plan->plan_cost >= C.optimal_cost);

    }else{
        assert(C.optimal_cost < 0);
        printf("Unsolvable\n");
    }
    pddlLiftedSearchDel(search);
    fflush(stdout);
}

static void _lifted_search(pddl_lifted_heur_t *heur,
                           pddl_lifted_search_alg_t search,
                           pddl_lifted_app_action_backend_t app_action,
                           int compare_to_optimal_cost)
{
    pddl_lifted_search_config_t cfg = PDDL_LIFTED_SEARCH_CONFIG_INIT;
    cfg.pddl = &C.pddl;
    cfg.alg = search;
    cfg.heur = heur;
    cfg.succ_gen = app_action;
    if (search == PDDL_LIFTED_SEARCH_ASTAR){
        testOptimalSearch(&cfg, compare_to_optimal_cost);
    }else{
        testGreedySearch(&cfg);
    }
    pddlLiftedHeurDel(heur);
}

TEST(lifted_blind_search_sql, lifted_search)
{
    _lifted_search(pddlLiftedHeurBlind(),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_SQL, 1);
}

TEST(lifted_blind_search_dl, lifted_search)
{
    _lifted_search(pddlLiftedHeurBlind(),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_astar_hmax_sql, lifted_search)
{
    _lifted_search(pddlLiftedHeurHMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_SQL, 1);
}

TEST(lifted_search_astar_hmax_dl, lifted_search)
{
    _lifted_search(pddlLiftedHeurHMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_gbfs_hadd_dl, lifted_search)
{
    _lifted_search(pddlLiftedHeurHAdd(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_GBFS,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_gbfs_hff_add_dl, lifted_search)
{
    _lifted_search(pddlLiftedHeurHFFAdd(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_GBFS,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_gbfs_hff_max_dl, lifted_search)
{
    _lifted_search(pddlLiftedHeurHFFMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_GBFS,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_lazy_hadd_dl, lifted_search)
{
    _lifted_search(pddlLiftedHeurHAdd(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_LAZY,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_lazy_hff_add_dl, lifted_search)
{
    _lifted_search(pddlLiftedHeurHFFAdd(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_LAZY,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_blind_search_unit_cost_sql, lifted_search_unit_cost)
{
    _lifted_search(pddlLiftedHeurBlind(),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_SQL, 0);
}

TEST(lifted_blind_search_unit_cost_dl, lifted_search_unit_cost)
{
    _lifted_search(pddlLiftedHeurBlind(),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_DL, 0);
}

TEST(lifted_search_astar_unit_cost_hmax_sql, lifted_search_unit_cost)
{
    _lifted_search(pddlLiftedHeurHMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_SQL, 0);
}

TEST(lifted_search_astar_unit_cost_hmax_dl, lifted_search_unit_cost)
{
    _lifted_search(pddlLiftedHeurHMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_DL, 0);
}
