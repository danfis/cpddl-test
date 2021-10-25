#include <assert.h>
#include "test.h"
#include "context.h"

TEST(pddl, root)
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 1;
    cfg.force_adl = 1;
    int ret = pddlInit(&C.pddl, C.files.domain_pddl, C.files.problem_pddl,
                       &cfg, &C.err);
    if (ret != 0)
        borErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    pddlCheckSizeTypes(&C.pddl);

    printf("---- Domain: %s | %s ----\n",
           C.files.domain_pddl, C.files.problem_pddl);
    pddlPrintDebug(&C.pddl, stdout);
    printf("---- Domain: %s | %s END ----\n",
           C.files.domain_pddl, C.files.problem_pddl);
}

TEST_TEAR_DOWN(pddl)
{
    pddlFree(&C.pddl);
}

TEST(pddl_noce, root)
{
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 1;
    cfg.force_adl = 1;
    int ret = pddlInit(&C.pddl, C.files.domain_pddl, C.files.problem_pddl,
                       &cfg, &C.err);
    if (ret != 0)
        borErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    pddlCheckSizeTypes(&C.pddl);
    pddlCompileAwayNonStaticCondEff(&C.pddl);

    pddlPrintDebug(&C.pddl, stdout);
}

TEST_TEAR_DOWN(pddl_noce)
{
    pddlFree(&C.pddl);
}

TEST(pddl_no_normalize, root)
{
    pddl_t pddl;
    pddl_config_t cfg = PDDL_CONFIG_INIT;
    cfg.normalize = 0;
    cfg.force_adl = 1;
    int ret = pddlInit(&pddl, C.files.domain_pddl, C.files.problem_pddl,
                       &cfg, &C.err);
    if (ret != 0)
        borErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    pddlCheckSizeTypes(&pddl);

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

TEST(pddl_noce_clone, pddl_noce)
{
    pddl_t pddl;
    pddlInitCopy(&pddl, &C.pddl);
    pddlPrintDebug(&pddl, stdout);
    pddlFree(&pddl);
}

