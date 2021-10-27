#include <assert.h>
#include "test.h"
#include "context.h"

TEST_EXPLICIT(lifted_mgroup_tests)
{
}

static int actionHasAddEff(const pddl_action_t *a, int pred)
{
    bor_list_t *item;
    const pddl_cond_t *c;
    const pddl_cond_part_t *and;
    const pddl_cond_atom_t *atom;

    if (a->eff == NULL)
        return 0;
    if (a->eff->type == PDDL_COND_ATOM){
        atom = PDDL_COND_CAST(a->eff, atom);
        return atom->pred == pred;
    }

    if (a->eff->type == PDDL_COND_AND){
        and = PDDL_COND_CAST(a->eff, part);
        BOR_LIST_FOR_EACH(&and->part, item){
            c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
            if (c->type != PDDL_COND_ATOM)
                continue;
            atom = PDDL_COND_CAST(c, atom);
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
    bor_err_t err = BOR_ERR_INIT;

    cfg.force_adl = 1;
    if (pddlInit(&pddl, domain_fn, problem_fn, &cfg, &err) != 0){
        printf("Could not parse!\n");
        borErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl);


    int pred_at = pddlPredsGet(&pddl.pred, "at");
    int pred_capacity = pddlPredsGet(&pddl.pred, "capacity");

    //pddlCondPrint(&pddl, &pddl.init->cls, NULL, stderr);

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
    bor_err_t err = BOR_ERR_INIT;

    cfg.force_adl = 1;
    if (pddlInit(&pddl, domain_fn, problem_fn, &cfg, &err) != 0){
        printf("Could not parse!\n");
        borErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl);

    int pred_base_obs = pddlPredsGet(&pddl.pred, "base-obstacle");
    int pred_gripper_empty = pddlPredsGet(&pddl.pred, "gripper-empty");

    //pddlCondPrint(&pddl, &pddl.init->cls, NULL, stderr);

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
    bor_err_t err = BOR_ERR_INIT;
    int too_heavy;

    cfg.force_adl = 1;
    if (pddlInit(&pddl, domain_fn, problem_fn, &cfg, &err) != 0){
        printf("Could not parse!\n");
        borErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl);
    pddlCompileAwayCondEff(&pddl);


    int pred_at_car_jun = pddlPredsGet(&pddl.pred, "at_car_jun");

    //pddlCondPrint(&pddl, &pddl.init->cls, NULL, stderr);

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
    bor_err_t err = BOR_ERR_INIT;

    cfg.force_adl = 1;
    if (pddlInit(&pddl, domain_fn, problem_fn, &cfg, &err) != 0){
        printf("Could not parse!\n");
        borErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl);


    int pred_bond = pddlPredsGet(&pddl.pred, "bond");

    //pddlCondPrint(&pddl, &pddl.init->cls, stderr);

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
                    || strcmp(a->name, "oxidationofborane") == 0
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
                    || strcmp(a->name, "oxidationofborane") == 0
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
                    || strcmp(a->name, "oxidationofborane") == 0
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
    bor_err_t err = BOR_ERR_INIT;

    if (pddlFiles1(&files, "pddl-data/ipc-2014/seq-opt/barman/p435.1",
                   &err) != 0){
        borErrPrint(&err, 1, stderr);
        return;
    }

    pddl_t pddl;
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.force_adl = 1;
    if (pddlInit(&pddl, files.domain_pddl, files.problem_pddl, &cfg, &err) != 0){
        printf("Could not parse!\n");
        borErrPrint(&err, 1, stderr);
        return;
    }
    pddlNormalize(&pddl);

    pddl_lifted_mgroup_t mg;
    pddl_cond_atom_t *atom;
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

    atom = pddlCondNewEmptyAtom(1);
    atom->pred = pddlPredsGet(&pddl.pred, "clean");
    atom->arg[0].param = 0;
    pddlCondArrAdd(&mg.cond, &atom->cls);

    atom = pddlCondNewEmptyAtom(2);
    atom->pred = pddlPredsGet(&pddl.pred, "contains");
    atom->arg[0].param = 0;
    atom->arg[1].param = 1;
    pddlCondArrAdd(&mg.cond, &atom->cls);

    atom = pddlCondNewEmptyAtom(2);
    atom->pred = pddlPredsGet(&pddl.pred, "used");
    atom->arg[0].param = 0;
    atom->arg[1].param = 2;
    pddlCondArrAdd(&mg.cond, &atom->cls);

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

    atom = pddlCondNewEmptyAtom(1);
    atom->pred = pddlPredsGet(&pddl.pred, "empty");
    atom->arg[0].param = 0;
    pddlCondArrAdd(&mg.cond, &atom->cls);

    atom = pddlCondNewEmptyAtom(2);
    atom->pred = pddlPredsGet(&pddl.pred, "contains");
    atom->arg[0].param = 0;
    atom->arg[1].param = 1;
    pddlCondArrAdd(&mg.cond, &atom->cls);

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

    atom = pddlCondNewEmptyAtom(2);
    atom->pred = pddlPredsGet(&pddl.pred, "holding");
    atom->arg[0].param = 1;
    atom->arg[1].param = 0;
    pddlCondArrAdd(&mg.cond, &atom->cls);

    atom = pddlCondNewEmptyAtom(1);
    atom->pred = pddlPredsGet(&pddl.pred, "ontable");
    atom->arg[0].param = 0;
    pddlCondArrAdd(&mg.cond, &atom->cls);

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


TEST(lifted_mgroup, pddl)
{
    pddl_lifted_mgroups_infer_limits_t infer_limit
                = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    pddlLiftedMGroupsInit(&C.lmg);
    C.lmg_set = 1;
    pddlLiftedMGroupsInferFAMGroups(&C.pddl, &infer_limit, &C.lmg, &C.err);
    pddlLiftedMGroupsSetExactlyOne(&C.pddl, &C.lmg, &C.err);
    pddlLiftedMGroupsSetStatic(&C.pddl, &C.lmg, &C.err);
    pddlLiftedMGroupsPrint(&C.pddl, &C.lmg, stdout);
    fflush(stdout);

    pddl_lifted_mgroups_t goal;
    pddlLiftedMGroupsInit(&goal);
    pddlLiftedMGroupsExtractGoalAware(&goal, &C.lmg, &C.pddl);
    if (goal.mgroup_size > 0){
        fprintf(stdout, "Goal aware:\n");
        pddlLiftedMGroupsPrint(&C.pddl, &goal, stdout);
        //fprintf(stdout, "Goal:\n");
        //pddlCondPrint(&pddl, pddl.goal, stdout);
        fflush(stdout);
    }
    pddlLiftedMGroupsFree(&goal);

    pddl_lifted_mgroups_t invs;
    pddl_lifted_mgroups_t mgs;
    pddlLiftedMGroupsInit(&invs);
    pddlLiftedMGroupsInit(&mgs);
    pddlLiftedMGroupsInferMonotonicity(&C.pddl, &infer_limit, &invs, &mgs, &C.err);
    fprintf(stdout, "Monotonicity invariants:\n");
    pddlLiftedMGroupsPrint(&C.pddl, &invs, stdout);
    fprintf(stdout, "FD Mutex Groups:\n");
    pddlLiftedMGroupsPrint(&C.pddl, &mgs, stdout);
    fflush(stdout);

    pddlLiftedMGroupsInit(&goal);
    pddlLiftedMGroupsExtractGoalAware(&goal, &mgs, &C.pddl);
    if (goal.mgroup_size > 0){
        fprintf(stdout, "FD Goal aware:\n");
        pddlLiftedMGroupsPrint(&C.pddl, &goal, stdout);
        //fprintf(stdout, "Goal:\n");
        //pddlCondPrint(&pddl, pddl.goal, stdout);
        fflush(stdout);
    }
    pddlLiftedMGroupsFree(&goal);
    pddlLiftedMGroupsFree(&invs);
    pddlLiftedMGroupsFree(&mgs);
}
