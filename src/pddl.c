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
    // All numeric tasks live under ipc-2023/num/; everything else,
    // including plain :action-costs tasks, is classified as non-numeric
    if (strncmp(TEST_TASK, "ipc-2023/num/", 13) == 0){
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
