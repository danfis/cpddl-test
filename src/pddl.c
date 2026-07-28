#include "test.h"
#include "context.h"
#include <assert.h>
#include <string.h>

TEST(pddl, r)
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 1;
    cfg.force_adl = 1;
    int ret = pddlInit(&C.pddl, C.files.domain_pddl, C.files.problem_pddl,
                       &cfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);
    C.pddl_set = 1;

    pddlPrintDebug(&C.pddl, stdout);
}

TEST(pddl_has_numeric_fluents, pddl)
{
    // Numeric tasks live under ipc-2023/num/, ipc-2026/num/, and
    // various/num-*; everything else, including plain :action-costs tasks,
    // is classified as non-numeric
    if (strncmp(TEST_TASK, "ipc-2023/num/", 13) == 0
            || strncmp(TEST_TASK, "ipc-2026/num/", 13) == 0
            || strncmp(TEST_TASK, "various/num-", 12) == 0){
        assert(pddlHasNumericFluents(&C.pddl));
    }else{
        assert(!pddlHasNumericFluents(&C.pddl));
    }
}

TEST(pddl_unit_cost, r)
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 1;
    cfg.force_adl = 1;
    cfg.enforce_unit_cost = 1;
    int ret = pddlInit(&C.pddl, C.files.domain_pddl, C.files.problem_pddl,
                       &cfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);
    C.pddl_set = 1;

    pddlPrintDebug(&C.pddl, stdout);
}

TEST(pddl_compile_away_cond_eff, pddl)
{
    pddlCompileAwayNonStaticCondEff(&C.pddl);
    pddlPrintDebug(&C.pddl, stdout);
}

TEST(pddl_is_metric_expressible_as_non_neg_int_action_costs, pddl)
{
    pddl_bool_t res;
    res = pddlIsMetricExpressibleAsNonNegIntActionCosts(&C.pddl, NULL);

    if (strncmp(TEST_TASK, "ipc-2023/num/", 13) == 0
            || strncmp(TEST_TASK, "ipc-2026/num/", 13) == 0
            || strncmp(TEST_TASK, "various/num-", 12) == 0){
        if (!res){
            printf("Metric not expressible as non-neg int action costs\n");
        }
    }else{
        assert(res);
    }
}

TEST(pddl_compile_metric_into_action_costs, pddl)
{
    pddl_t copy;
    pddlInitCopy(&copy, &C.pddl);

    pddl_compile_metric_into_action_costs_status_t ret;
    ret = pddlCompileMetricIntoActionCosts(&C.pddl, &C.err);

    switch (ret){
    case PDDL_COMPILE_METRIC_INTO_ACTION_COSTS_ERR:
        pddlErrPrint(&C.err, 1, stderr);
        assert(ret != PDDL_COMPILE_METRIC_INTO_ACTION_COSTS_ERR);
        break;
    case PDDL_COMPILE_METRIC_INTO_ACTION_COSTS_CHANGED:
        assert(C.pddl.metric);
        assert(pddlFmIsNumExpFluent(&C.pddl.minimize->fm));
        assert(C.pddl.minimize->e.fluent->pred
                == C.pddl.func.total_cost_func);
        pddlPrintDebug(&C.pddl, stdout);
        break;
    case PDDL_COMPILE_METRIC_INTO_ACTION_COSTS_OK:
    case PDDL_COMPILE_METRIC_INTO_ACTION_COSTS_NOT_COMPILABLE:
        assert(C.pddl.metric == copy.metric);
        if (C.pddl.minimize != NULL)
            assert(pddlFmEq(&C.pddl.minimize->fm, &copy.minimize->fm));
        assert(pddlFmEq(&C.pddl.init->fm, &copy.init->fm));
        for (int i = 0; i < copy.action.action_size; ++i){
            assert(pddlFmEq(C.pddl.action.action[i].pre,
                            copy.action.action[i].pre));
            assert(pddlFmEq(C.pddl.action.action[i].eff,
                            copy.action.action[i].eff));
        }
        assert(C.pddl.func.pred_size == copy.func.pred_size);
        break;
    default:
        assert(0 && "Unexpected return value");
    }
    pddlFree(&copy);
}

static int fmFltPre(pddl_fm_t *fm, void *ud)
{
    if (pddlFmIsNumExpNumFlt(fm)){
        *(int *)ud = 1;
        return -2;
    }
    return 0;
}

static int fmHasFlt(pddl_fm_t *fm)
{
    int has = 0;
    pddlFmTraverseAll(fm, fmFltPre, NULL, &has);
    return has;
}

TEST_COND(pddl_compile_flt_to_int, pddl, LP)
{
    pddl_t copy;
    pddlInitCopy(&copy, &C.pddl);

    pddl_compile_flt_to_int_status_t ret;
    ret = pddlCompileFltToInt(&C.pddl, &C.err);

    switch (ret){
    case PDDL_COMPILE_FLT_TO_INT_ERR:
        pddlErrPrint(&C.err, 1, stderr);
        assert(ret != PDDL_COMPILE_FLT_TO_INT_ERR);
        break;
    case PDDL_COMPILE_FLT_TO_INT_CHANGED:
        // No float constant may survive anywhere in the task
        assert(!fmHasFlt(&C.pddl.init->fm));
        if (C.pddl.goal != NULL)
            assert(!fmHasFlt(C.pddl.goal));
        for (int i = 0; i < C.pddl.action.action_size; ++i){
            assert(!fmHasFlt(C.pddl.action.action[i].pre));
            assert(!fmHasFlt(C.pddl.action.action[i].eff));
        }
        if (C.pddl.minimize != NULL)
            assert(!fmHasFlt(&C.pddl.minimize->fm));
        pddlPrintDebug(&C.pddl, stdout);
        break;
    case PDDL_COMPILE_FLT_TO_INT_OK:
    case PDDL_COMPILE_FLT_TO_INT_NOT_COMPILABLE:
        // Task must be completely untouched
        assert(C.pddl.metric == copy.metric);
        if (C.pddl.minimize != NULL)
            assert(pddlFmEq(&C.pddl.minimize->fm, &copy.minimize->fm));
        assert(pddlFmEq(&C.pddl.init->fm, &copy.init->fm));
        for (int i = 0; i < copy.action.action_size; ++i){
            assert(pddlFmEq(C.pddl.action.action[i].pre,
                            copy.action.action[i].pre));
            assert(pddlFmEq(C.pddl.action.action[i].eff,
                            copy.action.action[i].eff));
        }
        break;
    }
    pddlFree(&copy);
}

TEST(pddl_action_simplify_cond_effs, r)
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 0;
    cfg.force_adl = 1;
    pddl_t pddl;
    int ret = pddlInit(&pddl, C.files.domain_pddl, C.files.problem_pddl,
                       &cfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    for (int i = 0; i < pddl.action.action_size; ++i){
        pddl_action_t *a = pddl.action.action + i;
        pddlActionNormalize(a, &pddl);
        pddl_fm_t *pre = pddlFmClone(a->pre);
        pddl_fm_t *eff = pddlFmClone(a->eff);
        if (pddlActionSimplifyCondEffs(a, &pddl)){
            pddlActionPrint(&pddl, a, stdout);
        }else{
            // Returning false means the action was not changed
            assert(pddlFmEq(pre, a->pre) && pddlFmEq(eff, a->eff));
        }
        pddlFmDel(pre);
        pddlFmDel(eff);
    }
    pddlFree(&pddl);
}

TEST(pddl_compile_away_neg_pre, r)
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 1;
    cfg.force_adl = 1;
    cfg.normalize_compile_away_dynamic_neg_cond = pddl_false;
    pddl_t pddl;
    int ret = pddlInit(&pddl, C.files.domain_pddl, C.files.problem_pddl,
                       &cfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    ret = pddlCompileAwayNegativeConditions(&pddl, pddl_false, pddl_false,
                                            pddl_true, &C.err);
    assert(ret == 0);
    pddlPrintDebug(&pddl, stdout);
    pddlFree(&pddl);
}

TEST(pddl_no_normalize, r)
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 0;
    cfg.remove_empty_types = 0;
    cfg.force_adl = 1;
    int ret = pddlInit(&C.pddl, C.files.domain_pddl, C.files.problem_pddl,
                       &cfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);
    C.pddl_set = 1;

    pddlPrintDebug(&C.pddl, stdout);
}


TEST(pddl_clone, pddl)
{
    pddl_t pddl;
    pddlInitCopy(&pddl, &C.pddl);
    pddlPrintDebug(&pddl, stdout);
    pddlFree(&pddl);
}

TEST(pddl_compile_away_eq_pred_no_norm, pddl_no_normalize)
{
    pddl_t copy;
    pddlInitCopy(&copy, &C.pddl);
    int ret = pddlCompileAwayEqPred(&C.pddl);
    if (ret > 0){
        //pddlPrintDebug(&copy, stdout);
        //printf("======== AFTER ==========\n");
        pddlPrintDebug(&C.pddl, stdout);
    }
    pddlFree(&copy);
}

TEST(pddl_compile_away_eq_pred_lmg, pddl)
{
    pddl_compile_in_lmg_config_t cfg = PDDL_COMPILE_IN_LMG_CONFIG_INIT;
    cfg.prune_mutex = 1;
    cfg.prune_dead_end = 1;

    pddl_lifted_mgroups_infer_limits_t infer_limit
                = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    pddl_lifted_mgroups_t lmg;
    pddlLiftedMGroupsInit(&lmg);
    pddlLiftedMGroupsInferFAMGroups(&C.pddl, &infer_limit, &lmg, &C.err);
    pddlLiftedMGroupsSetExactlyOne(&C.pddl, &lmg, &C.err);
    pddlLiftedMGroupsSetStatic(&C.pddl, &lmg, &C.err);

    pddlCompileInLiftedMGroups(&C.pddl, &lmg, &cfg, &C.err);

    pddl_t copy;
    pddlInitCopy(&copy, &C.pddl);
    int ret = pddlCompileAwayEqPred(&C.pddl);
    if (ret > 0){
        //pddlPrintDebug(&copy, stdout);
        //printf("======== AFTER ==========\n");
        pddlPrintDebug(&C.pddl, stdout);
    }
    pddlFree(&copy);
    pddlLiftedMGroupsFree(&lmg);
}
