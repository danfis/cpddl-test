#include "test.h"
#include "context.h"
#include <assert.h>
#include <sys/wait.h>

static void printState(const char *prefix,
                       const pddl_iset_t *state,
                       const pddl_strips_maker_t *smaker)
{
    printf("%s", prefix);
    PDDL_ISET_FOR_EACH(state, fact){
        const pddl_ground_atom_t *ga;
        ga = pddlStripsMakerGroundAtomConst(smaker, fact);
        printf(" (%s", C.pddl.pred.pred[ga->pred].name);
        for (int i = 0; i < ga->arity; ++i){
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
    pddlStripsMakerAddInitAndCollect(&smaker, &init, NULL);

    pddl_lifted_app_action_t *aa;
    aa = pddlLiftedAppActionNew(&C.pddl, backend, &C.err);
    if (aa == NULL){
        pddlErrPrint(&C.err, 1, stderr);
        assert(0 && "pddlLiftedAppActionNew failed");
    }
    assert(pddlLiftedAppActionSize(aa) == 0);

    int ret;
    pddlLiftedAppActionClearState(aa);

    PDDL_ISET(cur_state);
    pddlISetUnion(&cur_state, &init);
    int num_state_size = pddlStripsMakerNonStaticFluentSize(&smaker);
    pddl_num_val_t *cur_num_state = NULL;
    if (num_state_size > 0){
        cur_num_state = PDDL_ZALLOC_ARR(pddl_num_val_t, num_state_size);
        pddlStripsMakerInitNumState(&smaker, cur_num_state);
    }
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
        pddl_strips_maker_eff_t eff = PDDL_STRIPS_MAKER_EFF_INIT;
        for (int i = 0; i < size; ++i){
            int aid = pddlLiftedAppActionId(aa, i);
            const pddl_action_t *action = C.pddl.action.action + aid;
            const int *args = pddlLiftedAppActionArgs(aa, i);

            if (num_state_size > 0){
                int sat = pddlStripsMakerIsNumCondSatisfied(&smaker,
                                                            action->pre,
                                                            cur_num_state,
                                                            args, &C.err);
                if (sat < 0){
                    pddlErrPrint(&C.err, 1, stderr);
                    assert(0 && "Error checking numeric precondition");
                }
                if (!sat)
                    continue;
            }

            ret = pddlStripsMakerActionEffInState(&smaker, action, args,
                                                  &cur_state, cur_num_state,
                                                  &eff, &C.err);
            if (ret < 0)
                pddlErrPrint(&C.err, 1, stderr);
            assert(ret == 0);

            printf("%s", action->name);
            for (int j = 0; j < action->param.param_size; ++j){
                printf(" %s", C.pddl.obj.obj[args[j]].name);
            }
            printf(" :: cost:");
            char num_val_buf[32];
            switch (eff.cost_type){
            case PDDL_STRIPS_MAKER_EFF_INT_ACTION_COST:
                printf(" %d", eff.cost.int_action_cost);
                break;
            case PDDL_STRIPS_MAKER_EFF_GENERAL_ACTION_COST:
                printf(" %s", pddlNumValFmt(&eff.cost.general_action_cost,
                                            num_val_buf, 32));
                break;
            case PDDL_STRIPS_MAKER_EFF_STATE_METRIC:
                printf(" metric:%s", pddlNumValFmt(&eff.cost.state_metric,
                                            num_val_buf, 32));
                break;
            }
            printf("\n");
            printState("    add:", &eff.add_eff, &smaker);
            printState("    del:", &eff.del_eff, &smaker);
            if (num_state_size > 0){
                printf("    num:");
                for (int j = 0; j < num_state_size; ++j){
                    char buf[32];
                    printf(" %s", pddlNumValFmt(&eff.num_eff[j], buf, 32));
                }
                printf("\n");
            }

            if (i == step % size){
                pddlISetMinus2(&next_state, &cur_state, &eff.del_eff);
                pddlISetUnion(&next_state, &eff.add_eff);
                if (num_state_size > 0){
                    memcpy(cur_num_state, eff.num_eff,
                           sizeof(pddl_num_val_t) * num_state_size);
                }
            }
        }
        pddlStripsMakerEffFree(&eff);
        pddlISetEmpty(&cur_state);
        pddlISetUnion(&cur_state, &next_state);
        pddlISetFree(&next_state);
    }
    pddlISetFree(&cur_state);

    pddlLiftedAppActionDel(aa);
    pddlISetFree(&init);
    pddlStripsMakerFree(&smaker);
}

TEST_COND(lifted_succ_gen_sql, pddl, SQLITE)
{
    if (pddlIsNumeric(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }

    testSuccGen(PDDL_LIFTED_APP_ACTION_SQL);
}

TEST(lifted_succ_gen_dl, pddl)
{
    testSuccGen(PDDL_LIFTED_APP_ACTION_DL);
}

TEST(lifted_search, pddl)
{
    int st = pddlCompileAwayDisjunctiveGoalsWithAuxActions(&C.pddl, &C.err);
    assert(st == 0);

    pddl_compile_metric_into_action_costs_status_t st2;
    st2 = pddlCompileMetricIntoActionCosts(&C.pddl, &C.err);
    if (st2 == PDDL_COMPILE_METRIC_INTO_ACTION_COSTS_ERR){
        pddlErrPrint(&C.err, 1, stderr);
        assert(0 && "pddlCompileMetricIntoActionCosts failed");
    }
}

TEST(lifted_search_heur, lifted_search)
{
}

TEST(lifted_search_unit_cost, pddl_unit_cost)
{
    int st = pddlCompileAwayDisjunctiveGoalsWithAuxActions(&C.pddl, &C.err);
    assert(st == 0);
}

static void _lifted_search(pddl_lifted_heur_t *heur,
                           pddl_lifted_search_alg_t search_alg,
                           pddl_lifted_app_action_backend_t app_action,
                           int compare_to_optimal_cost)
{
    if (heur == NULL){
        if (pddlErrIsSet(&C.err))
            pddlErrPrint(&C.err, 1, stderr);
        assert(heur != NULL && "Could not create a lifted heuristic");
        return;
    }

    pddl_lifted_search_config_t cfg = PDDL_LIFTED_SEARCH_CONFIG_INIT;
    cfg.pddl = &C.pddl;
    cfg.alg = search_alg;
    cfg.heur = heur;
    cfg.succ_gen = app_action;
    //pddlErrLogEnable(&C.err, stdout);

    pddl_lifted_search_t *search = pddlLiftedSearchNew(&cfg, &C.err);
    if (search == NULL){
        pddlErrPrint(&C.err, 1, stderr);
        assert(0 && "pddlLiftedSearchNew failed");
    }
    pddl_lifted_search_status_t st = pddlLiftedSearchInitStep(search);
    while (st == PDDL_LIFTED_SEARCH_CONT)
        st = pddlLiftedSearchStep(search);

    if (st == PDDL_LIFTED_SEARCH_FOUND
            && search_alg != PDDL_LIFTED_SEARCH_ASTAR){
        pddl_lifted_search_stat_t stat;
        pddlLiftedSearchStat(search, &stat);
        printf("Expanded: %lu\n", (unsigned long)stat.expanded);
    }

    switch (st){
        case PDDL_LIFTED_SEARCH_FOUND: {
            const pddl_lifted_plan_t *plan = pddlLiftedSearchPlan(search);
            if (search_alg == PDDL_LIFTED_SEARCH_ASTAR)
                printf("Cost: %d\n", plan->plan_cost);
            fflush(stdout);
            int val = validateLiftedPlan(plan);
            assert(val == 0);
            if (search_alg == PDDL_LIFTED_SEARCH_ASTAR
                    && compare_to_optimal_cost && C.optimal_cost >= 0){
                assert(plan->plan_cost == C.optimal_cost);
            }
            if (compare_to_optimal_cost && C.optimal_cost >= 0){
                assert(plan->plan_cost >= C.optimal_cost);
            }
            break;
        }
        case PDDL_LIFTED_SEARCH_UNSOLVABLE:
            assert(C.optimal_cost < 0);
            printf("Unsolvable\n");
            break;
        case PDDL_LIFTED_SEARCH_ABORT:
            if (pddlErrIsSet(&C.err))
                pddlErrPrint(&C.err, 1, stderr);
            assert(0 && "ABORT status");
            break;
        case PDDL_LIFTED_SEARCH_CONT:
            assert(0);
            break;
    }
    fflush(stdout);
    pddlLiftedSearchDel(search);
    pddlLiftedHeurDel(heur);
}

TEST_COND(lifted_blind_search_sql, lifted_search, SQLITE)
{
    if (pddlIsNumeric(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }
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

TEST_COND(lifted_search_astar_hmax_sql, lifted_search_heur, SQLITE)
{
    if (pddlIsNumeric(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }
    _lifted_search(pddlLiftedHeurHMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_SQL, 1);
}

TEST(lifted_search_astar_hmax_dl, lifted_search_heur)
{
    _lifted_search(pddlLiftedHeurHMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_gbfs_hadd_dl, lifted_search_heur)
{
    _lifted_search(pddlLiftedHeurHAdd(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_GBFS,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_gbfs_hff_add_dl, lifted_search_heur)
{
    _lifted_search(pddlLiftedHeurHFFAdd(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_GBFS,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_gbfs_hff_max_dl, lifted_search_heur)
{
    _lifted_search(pddlLiftedHeurHFFMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_GBFS,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_lazy_hadd_dl, lifted_search_heur)
{
    _lifted_search(pddlLiftedHeurHAdd(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_LAZY,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST(lifted_search_lazy_hff_add_dl, lifted_search_heur)
{
    _lifted_search(pddlLiftedHeurHFFAdd(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_LAZY,
                   PDDL_LIFTED_APP_ACTION_DL, 1);
}

TEST_COND(lifted_blind_search_unit_cost_sql, lifted_search_unit_cost, SQLITE)
{
    if (pddlIsNumeric(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }
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

TEST_COND(lifted_search_astar_unit_cost_hmax_sql, lifted_search_unit_cost, SQLITE)
{
    if (pddlIsNumeric(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }
    _lifted_search(pddlLiftedHeurHMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_SQL, 0);
}

TEST(lifted_search_astar_unit_cost_hmax_dl, lifted_search_unit_cost)
{
    if (pddlIsNumeric(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }
    _lifted_search(pddlLiftedHeurHMax(&C.pddl, &C.err),
                   PDDL_LIFTED_SEARCH_ASTAR,
                   PDDL_LIFTED_APP_ACTION_DL, 0);
}
