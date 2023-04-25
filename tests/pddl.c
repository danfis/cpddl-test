#include "test.h"
#include "context.h"
#include <assert.h>

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

TEST(pddl_no_normalize, r)
{
    pddl_t pddl;
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 0;
    cfg.remove_empty_types = 0;
    cfg.force_adl = 1;
    int ret = pddlInit(&pddl, C.files.domain_pddl, C.files.problem_pddl,
                       &cfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    pddlPrintDebug(&pddl, stdout);
    pddlFree(&pddl);
}


TEST(pddl_clone, pddl)
{
    pddl_t pddl;
    pddlInitCopy(&pddl, &C.pddl);
    pddlPrintDebug(&pddl, stdout);
    pddlFree(&pddl);
}
