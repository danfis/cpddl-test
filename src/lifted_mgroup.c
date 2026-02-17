#include "test.h"
#include "context.h"
#include <assert.h>

TEST_EXPLICIT(lifted_mgroup_tests)
{
}

static int actionHasAddEff(const pddl_action_t *a, int pred)
{

    if (a->eff == NULL)
        return 0;
    if (a->eff->type == PDDL_FM_ATOM){
        const pddl_fm_atom_t *atom = pddlFmToAtomConst(a->eff);
        return atom->pred == pred;
    }

    if (a->eff->type == PDDL_FM_AND){
        const pddl_fm_junc_t *and = pddlFmToJunc(a->eff);
        PDDL_LIST_FOR_EACH(&and->part, item){
            const pddl_fm_t *c = PDDL_LIST_ENTRY(item, pddl_fm_t, conn);
            if (c->type != PDDL_FM_ATOM)
                continue;
            const pddl_fm_atom_t *atom = pddlFmToAtomConst(c);
            if (!atom->neg && atom->pred == pred)
                return 1;
        }
    }

    return 0;
}

TEST(lifted_mgroup_tests_transport, lifted_mgroup_tests)
{
    const char *domain_fn = "pddl-data/ipc-2014/seq-opt/transport/domain.pddl";
    const char *problem_fn = "pddl-data/ipc-2014/seq-opt/transport/p01.pddl";
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    pddl_t pddl;
    pddl_err_t err = PDDL_ERR_INIT;

    cfg.force_adl = 1;
    if (pddlInit(&pddl, domain_fn, problem_fn, &cfg, &err) != 0){
        printf("Could not parse!\n");
        pddlErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl, &C.err);


    int pred_at = pddlPredsGet(&pddl.pred, "at");
    int pred_capacity = pddlPredsGet(&pddl.pred, "capacity");

    //pddlFmPrint(&pddl, &pddl.init->fm, NULL, stderr);

    pddl_lifted_mgroup_t mg;

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_at, 0);
    //pddlLiftedMGroupPrint(&pddl, &mg, stderr);
    assert(pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(ret == 0);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (strcmp(a->name, "pick-up") == 0){
            assert(ret);
        }else{
            assert(ret == 0);
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_at, 1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stderr);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(ret == 0);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (strcmp(a->name, "drop") == 0){
            assert(ret == 0);
        }else{
            assert(ret);
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_at, -1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(ret == 0);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (strcmp(a->name, "pick-up") == 0){
            assert(ret);
        }else{
            assert(ret == 0);
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_capacity, 0);
    //pddlLiftedMGroupPrint(&pddl, &mg, stderr);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (strcmp(a->name, "drive") == 0){
            assert(ret);
        }else{
            assert(!ret);
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_capacity, 1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stderr);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        assert(ret);
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_capacity, -1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (strcmp(a->name, "drive") == 0){
            assert(ret);
        }else{
            assert(!ret);
        }
    }
    pddlLiftedMGroupFree(&mg);

    //pddlPrintPDDLDomain(&pddl, stdout);
    pddlFree(&pddl);
}

TEST(lifted_mgroup_tests_tidybot, lifted_mgroup_tests)
{
    const char *domain_fn = "pddl-data/ipc-2014/seq-opt/tidybot/domain.pddl";
    const char *problem_fn = "pddl-data/ipc-2014/seq-opt/tidybot/p01.pddl";
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    pddl_t pddl;
    pddl_err_t err = PDDL_ERR_INIT;

    cfg.force_adl = 1;
    if (pddlInit(&pddl, domain_fn, problem_fn, &cfg, &err) != 0){
        printf("Could not parse!\n");
        pddlErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl, &C.err);

    int pred_base_obs = pddlPredsGet(&pddl.pred, "base-obstacle");
    int pred_gripper_empty = pddlPredsGet(&pddl.pred, "gripper-empty");

    //pddlFmPrint(&pddl, &pddl.init->fm, NULL, stderr);

    pddl_lifted_mgroup_t mg;

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_base_obs, 0);
    //pddlLiftedMGroupPrint(&pddl, &mg, stderr);
    assert(pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        if (strcmp(a->name, "base-cart-left") == 0
                || strcmp(a->name, "base-cart-right") == 0
                || strcmp(a->name, "base-cart-up") == 0
                || strcmp(a->name, "base-cart-down") == 0){
            assert(ret);
        }else{
            assert(!ret);
        }

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (strncmp(a->name, "base-", 5) != 0){
            assert(ret);
        }else{
            assert(!ret);
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_base_obs, 1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        if (strcmp(a->name, "base-cart-left") == 0
                || strcmp(a->name, "base-cart-right") == 0
                || strcmp(a->name, "base-cart-up") == 0
                || strcmp(a->name, "base-cart-down") == 0){
            assert(ret);
        }else{
            assert(!ret);
        }

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (strncmp(a->name, "base-", 5) != 0){
            assert(ret);
        }else{
            assert(!ret);
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_base_obs, -1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (strncmp(a->name, "base-", 5) != 0){
            assert(ret);
        }else{
            assert(!ret);
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_gripper_empty,
                                     -1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stderr);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (strncmp(a->name, "put-", 4) == 0){
            assert(!ret);
        }else{
            assert(ret);
        }
    }
    pddlLiftedMGroupFree(&mg);

    //pddlPrintPDDLDomain(&pddl, stdout);
    pddlFree(&pddl);
}

TEST(lifted_mgroup_tests_citycar, lifted_mgroup_tests)
{
    const char *domain_fn = "pddl-data/ipc-2014/seq-opt/citycar/domain.pddl";
    const char *problem_fn = "pddl-data/ipc-2014/seq-opt/citycar/p2-2-2-2-1.pddl";
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    pddl_t pddl;
    pddl_err_t err = PDDL_ERR_INIT;
    int too_heavy;

    cfg.force_adl = 1;
    if (pddlInit(&pddl, domain_fn, problem_fn, &cfg, &err) != 0){
        printf("Could not parse!\n");
        pddlErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl, &C.err);
    pddlCompileAwayCondEff(&pddl);


    int pred_at_car_jun = pddlPredsGet(&pddl.pred, "at_car_jun");

    //pddlFmPrint(&pddl, &pddl.init->fm, NULL, stderr);

    pddl_lifted_mgroup_t mg;

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_at_car_jun, 0);
    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    too_heavy = 0;
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        too_heavy |= ret;
        if (strcmp(a->name, "destroy_road") != 0){
            assert(!ret);
        }

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (actionHasAddEff(a, pred_at_car_jun)){
            assert(!ret);
        }else{
            assert(ret);
        }
    }
    assert(too_heavy);
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_at_car_jun, 1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (actionHasAddEff(a, pred_at_car_jun)){
            assert(!ret);
        }else{
            assert(ret);
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_at_car_jun, -1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);

        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        if (actionHasAddEff(a, pred_at_car_jun)){
            assert(!ret);
        }else{
            assert(ret);
        }
    }
    pddlLiftedMGroupFree(&mg);

    //pddlPrintPDDLDomain(&pddl, stdout);
    pddlFree(&pddl);
}

TEST(lifted_mgroup_tests_orgsynth, lifted_mgroup_tests)
{
    const char *domain_fn
            = "pddl-data/ipc-2018/seq-opt/organic-synthesis/domain-p01.pddl";
    const char *problem_fn
            = "pddl-data/ipc-2018/seq-opt/organic-synthesis/p01.pddl";
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    pddl_t pddl;
    pddl_err_t err = PDDL_ERR_INIT;

    cfg.force_adl = 1;
    if (pddlInit(&pddl, domain_fn, problem_fn, &cfg, &err) != 0){
        printf("Could not parse!\n");
        pddlErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl, &C.err);


    int pred_bond = pddlPredsGet(&pddl.pred, "bond");

    //pddlFmPrint(&pddl, &pddl.init->fm, stderr);

    pddl_lifted_mgroup_t mg;

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_bond, 0);
    //pddlLiftedMGroupPrint(&pddl, &mg, stderr);
    assert(pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        if (strcmp(a->name, "etherformationbysulfonatedisplacement") == 0
                || strcmp(a->name, "sulfonylationofalcohol") == 0){
            assert(!ret);
        }

        if (actionHasAddEff(a, pred_bond)){
            ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
            if (strcmp(a->name, "etherformationbysulfonatedisplacement") == 0
                    || strcmp(a->name, "oxidationofpddlane") == 0
                    || strcmp(a->name, "sulfonylationofalcohol") == 0){
                assert(ret);
            }else{
                assert(!ret);
            }
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_bond, 1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stderr);
    assert(pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        if (strcmp(a->name, "etherformationbysulfonatedisplacement") == 0
                || strcmp(a->name, "sulfonylationofalcohol") == 0){
            assert(!ret);
        }

        if (actionHasAddEff(a, pred_bond)){
            ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
            if (strcmp(a->name, "etherformationbysulfonatedisplacement") == 0
                    || strcmp(a->name, "oxidationofpddlane") == 0
                    || strcmp(a->name, "sulfonylationofalcohol") == 0){
                assert(ret);
            }else{
                assert(!ret);
            }
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlLiftedMGroupInitCandFromPred(&mg, pddl.pred.pred + pred_bond, -1);
    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);

        if (actionHasAddEff(a, pred_bond)){
            ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
            if (strcmp(a->name, "etherformationbysulfonatedisplacement") == 0
                    || strcmp(a->name, "oxidationofpddlane") == 0
                    || strcmp(a->name, "sulfonylationofalcohol") == 0){
                assert(ret);
            }else{
                assert(!ret);
            }
        }
    }
    pddlLiftedMGroupFree(&mg);

    //pddlPrintPDDLDomain(&pddl, stdout);
    pddlFree(&pddl);
}

TEST(lifted_mgroup_tests_barman, lifted_mgroup_tests)
{
    pddl_files_t files;
    pddl_err_t err = PDDL_ERR_INIT;

    if (pddlFiles1(&files, "pddl-data/ipc-2014/seq-opt/barman/p435.1",
                   &err) != 0){
        pddlErrPrint(&err, 1, stderr);
        return;
    }

    pddl_t pddl;
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.force_adl = 1;
    if (pddlInit(&pddl, files.domain_pddl, files.problem_pddl, &cfg, &err) != 0){
        printf("Could not parse!\n");
        pddlErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl, &C.err);

    pddl_lifted_mgroup_t mg;
    pddl_fm_atom_t *atom;
    pddl_param_t *param;

    pddlLiftedMGroupInitEmpty(&mg);

    param = pddlParamsAdd(&mg.param);
    param->type = pddlTypesGet(&pddl.type, "shot");
    param = pddlParamsAdd(&mg.param);
    param->type = pddlTypesGet(&pddl.type, "cocktail");
    param->is_counted_var = 1;
    param = pddlParamsAdd(&mg.param);
    param->type = pddlTypesGet(&pddl.type, "beverage");
    param->is_counted_var = 1;

    atom = pddlFmNewEmptyAtom(1);
    atom->pred = pddlPredsGet(&pddl.pred, "clean");
    atom->arg[0].param = 0;
    pddlFmArrAdd(&mg.cond, &atom->fm);

    atom = pddlFmNewEmptyAtom(2);
    atom->pred = pddlPredsGet(&pddl.pred, "contains");
    atom->arg[0].param = 0;
    atom->arg[1].param = 1;
    pddlFmArrAdd(&mg.cond, &atom->fm);

    atom = pddlFmNewEmptyAtom(2);
    atom->pred = pddlPredsGet(&pddl.pred, "used");
    atom->arg[0].param = 0;
    atom->arg[1].param = 2;
    pddlFmArrAdd(&mg.cond, &atom->fm);

    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);
        if (ret){
            fprintf(stdout, "%s is too heavy\n", a->name);
        }
        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        assert(ret);
        if (!ret){
            fprintf(stdout, "%s is not balanced\n", a->name);
        }
    }
    pddlLiftedMGroupFree(&mg);


    pddlLiftedMGroupInitEmpty(&mg);
    param = pddlParamsAdd(&mg.param);
    param->type = pddlTypesGet(&pddl.type, "shot");
    param = pddlParamsAdd(&mg.param);
    param->type = pddlTypesGet(&pddl.type, "beverage");
    param->is_counted_var = 1;

    atom = pddlFmNewEmptyAtom(1);
    atom->pred = pddlPredsGet(&pddl.pred, "empty");
    atom->arg[0].param = 0;
    pddlFmArrAdd(&mg.cond, &atom->fm);

    atom = pddlFmNewEmptyAtom(2);
    atom->pred = pddlPredsGet(&pddl.pred, "contains");
    atom->arg[0].param = 0;
    atom->arg[1].param = 1;
    pddlFmArrAdd(&mg.cond, &atom->fm);

    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);
        if (ret){
            fprintf(stdout, "%s is too heavy\n", a->name);
        }
        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        assert(ret);
        if (!ret){
            fprintf(stdout, "%s is not balanced\n", a->name);
        }
    }
    pddlLiftedMGroupFree(&mg);


    pddlLiftedMGroupInitEmpty(&mg);
    param = pddlParamsAdd(&mg.param);
    param->type = pddlTypesGet(&pddl.type, "shot");
    param = pddlParamsAdd(&mg.param);
    param->type = pddlTypesGet(&pddl.type, "hand");
    param->is_counted_var = 1;

    atom = pddlFmNewEmptyAtom(2);
    atom->pred = pddlPredsGet(&pddl.pred, "holding");
    atom->arg[0].param = 1;
    atom->arg[1].param = 0;
    pddlFmArrAdd(&mg.cond, &atom->fm);

    atom = pddlFmNewEmptyAtom(1);
    atom->pred = pddlPredsGet(&pddl.pred, "ontable");
    atom->arg[0].param = 0;
    pddlFmArrAdd(&mg.cond, &atom->fm);

    //pddlLiftedMGroupPrint(&pddl, &mg, stdout);
    assert(!pddlLiftedMGroupIsInitTooHeavy(&mg, &pddl));
    for (int ai = 0; ai < pddl.action.action_size; ++ai){
        const pddl_action_t *a = pddl.action.action + ai;
        int ret = pddlLiftedMGroupIsActionTooHeavy(&mg, &pddl, ai);
        assert(!ret);
        if (ret){
            fprintf(stdout, "%s is too heavy\n", a->name);
        }
        ret = pddlLiftedMGroupIsActionBalanced(&mg, &pddl, ai);
        assert(ret);
        if (!ret){
            fprintf(stdout, "%s is not balanced\n", a->name);
        }
    }
    pddlLiftedMGroupFree(&mg);

    pddlFree(&pddl);
}


TEST(lmg_fam, pddl)
{
    pddl_lifted_mgroups_infer_limits_t infer_limit
                = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    pddlLiftedMGroupsInit(&C.lmg);
    pddlLiftedMGroupsInferFAMGroups(&C.pddl, &infer_limit, &C.lmg, &C.err);
    pddlLiftedMGroupsSetExactlyOne(&C.pddl, &C.lmg, &C.err);
    pddlLiftedMGroupsSetStatic(&C.pddl, &C.lmg, &C.err);
    C.lmg_set = 1;

    pddlLiftedMGroupsPrint(&C.pddl, &C.lmg, stdout);
    fflush(stdout);

    pddl_lifted_mgroups_t goal;
    pddlLiftedMGroupsInit(&goal);
    pddlLiftedMGroupsExtractGoalAware(&goal, &C.lmg, &C.pddl);
    if (goal.mgroup_size > 0){
        fprintf(stdout, "Goal aware:\n");
        pddlLiftedMGroupsPrint(&C.pddl, &goal, stdout);
        //fprintf(stdout, "Goal:\n");
        //pddlFmPrint(&pddl, pddl.goal, stdout);
        fflush(stdout);
    }
    pddlLiftedMGroupsFree(&goal);
}

TEST(lmg_fam_double_counted, lmg_fam)
{
    pddl_lifted_mgroups_t lmgs;
    pddlLiftedMGroupsInitCopy(&lmgs, &C.lmg);
    pddlLiftedMGroupsDoubleCounted(&lmgs);
    pddlLiftedMGroupsPrint(&C.pddl, &lmgs, stdout);
    fflush(stdout);
    pddlLiftedMGroupsFree(&lmgs);
}

TEST(lmg_fd, lmg_fam)
{
    pddl_lifted_mgroups_infer_limits_t infer_limit
                = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    pddlLiftedMGroupsInit(&C.lmg_fd);
    pddlLiftedMGroupsInferMonotonicity(&C.pddl, &infer_limit,
                                       &C.lmg_mono, &C.lmg_fd, &C.err);
    pddlLiftedMGroupsSetExactlyOne(&C.pddl, &C.lmg_fd, &C.err);
    pddlLiftedMGroupsSetStatic(&C.pddl, &C.lmg_fd, &C.err);
    C.lmg_fd_set = 1;
    C.lmg_mono_set = 1;

    fprintf(stdout, "Monotonicity invariants:\n");
    pddlLiftedMGroupsPrint(&C.pddl, &C.lmg_mono, stdout);
    fprintf(stdout, "FD Mutex Groups:\n");
    pddlLiftedMGroupsPrint(&C.pddl, &C.lmg_fd, stdout);
    fflush(stdout);

    pddl_lifted_mgroups_t goal;
    pddlLiftedMGroupsInit(&goal);
    pddlLiftedMGroupsExtractGoalAware(&goal, &C.lmg_fd, &C.pddl);
    if (goal.mgroup_size > 0){
        fprintf(stdout, "FD Goal aware:\n");
        pddlLiftedMGroupsPrint(&C.pddl, &goal, stdout);
        //fprintf(stdout, "Goal:\n");
        //pddlFmPrint(&pddl, pddl.goal, stdout);
        fflush(stdout);
    }
    pddlLiftedMGroupsFree(&goal);
}

TEST(lmg, lmg_fd)
{
}

static void lmgCompileInCheckPruning(const pddl_t *pddl,
                                     const pddl_strips_t *strips_ref,
                                     int (*ground)(pddl_strips_t *,
                                                   const pddl_t *pddl,
                                                   const pddl_ground_config_t *cfg,
                                                   pddl_err_t *err))
{
    pddl_strips_t strips;
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.lifted_mgroups = NULL;
    ground_cfg.prune_op_pre_mutex = 0;
    ground_cfg.prune_op_dead_end = 0;
    int ret = ground(&strips, pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    pddlIrrelevanceAnalysis(&strips, &rm_fact, &rm_op, NULL, NULL);
    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0)
        pddlStripsReduce(&strips, &rm_fact, &rm_op);
    pddlISetFree(&rm_fact);
    pddlISetFree(&rm_op);

    for (int j = 0; j < strips.op.op_size; ++j){
        int found = 0;
        for (int i = 0; i < strips_ref->op.op_size; ++i){
            if (strcmp(strips.op.op[j]->name, strips_ref->op.op[i]->name) == 0){
                found = 1;
                break;
            }
        }
        if (!found)
            fprintf(stderr, "(%s)\n", strips.op.op[j]->name);
        assert(found);
    }

    for (int j = 0; j < strips.fact.fact_size; ++j){
        int found = 0;
        for (int i = 0; i < strips_ref->fact.fact_size; ++i){
            if (strcmp(strips.fact.fact[j]->name, strips_ref->fact.fact[i]->name) == 0){
                found = 1;
                break;
            }
        }
        if (!found)
            fprintf(stderr, "(%s)\n", strips.fact.fact[j]->name);
        assert(found);
    }

    for (int i = 0; i < strips_ref->fact.fact_size; ++i){
        int found = 0;
        for (int j = 0; j < strips.fact.fact_size; ++j){
            if (strcmp(strips.fact.fact[j]->name, strips_ref->fact.fact[i]->name) == 0){
                found = 1;
                break;
            }
        }
        if (!found)
            fprintf(stderr, "(%s)\n", strips_ref->fact.fact[i]->name);
        assert(found);
    }

    assert(strips.op.op_size == strips_ref->op.op_size);
    assert(strips.fact.fact_size == strips_ref->fact.fact_size);
    pddlStripsFree(&strips);
}

TEST(lmg_compile_in, lmg)
{
    pddl_compile_in_lmg_config_t cfg = PDDL_COMPILE_IN_LMG_CONFIG_INIT;
    cfg.prune_mutex = 1;
    cfg.prune_dead_end = 1;

    pddl_t pddl;
    pddlInitCopy(&pddl, &C.pddl);
    pddlErrLogDisablePrintResources(&C.err, 1);
    pddlErrLogEnable(&C.err, stdout);
    if (pddlCompileInLiftedMGroups(&pddl, &C.lmg, &cfg, &C.err))
        pddlPrintDebug(&pddl, stdout);
    pddlErrLogEnable(&C.err, NULL);
    fflush(stdout);

    pddl_strips_t strips_ref;
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.lifted_mgroups = &C.lmg;
    ground_cfg.prune_op_pre_mutex = 1;
    ground_cfg.prune_op_dead_end = 1;
    int ret = pddlStripsGroundTrie(&strips_ref, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    pddlIrrelevanceAnalysis(&strips_ref, &rm_fact, &rm_op, NULL, NULL);
    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0)
        pddlStripsReduce(&strips_ref, &rm_fact, &rm_op);
    pddlISetFree(&rm_fact);
    pddlISetFree(&rm_op);

    lmgCompileInCheckPruning(&pddl, &strips_ref, pddlStripsGroundTrie);
    lmgCompileInCheckPruning(&pddl, &strips_ref, pddlStripsGroundDatalog);
#ifdef PDDL_SQLITE
    lmgCompileInCheckPruning(&pddl, &strips_ref, pddlStripsGroundSql);
#endif

    pddlStripsFree(&strips_ref);

    pddlFree(&pddl);
}
