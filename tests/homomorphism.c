#include "pddl/pddl.h"
#include <assert.h>
#include "test.h"
#include "context.h"

static pddl_homomorphic_task_t h;

TEST(homomorphism, pddl)
{
    pddlHomomorphicTaskInit(&h, &C.pddl);
    pddlHomomorphicTaskSeed(&h, 1234);
}

TEST_TEAR_DOWN(homomorphism)
{
    pddlHomomorphicTaskFree(&h);
}

TEST(homomorphism_type, homomorphism)
{
    if (C.pddl.type.type_size <= 1)
        return;

    int before = h.task.obj.obj_size;
    for (int type = 0; type < C.pddl.type.type_size; ++type){
        if (!pddlTypesIsMinimal(&C.pddl.type, type))
            continue;
        if (pddlTypeNumObjs(&C.pddl.type, type) <= 1)
            continue;
        printf("Collapsing type %d:%s with %d objects\n",
               type, C.pddl.type.type[type].name,
               pddlTypeNumObjs(&C.pddl.type, type));
        if (pddlHomomorphicTaskCollapseType(&h, type, &C.err) < 0){
            pddlErrPrint(&C.err, 1, stderr);
            return;
        }
        int after = h.task.obj.obj_size;
        assert(after < before);
        printf("Reduced %d -> %d\n", before, after);
        pddlPrintDebug(&h.task, stdout);
        break;
    }
}

TEST(homomorphism_random_pair, homomorphism)
{
    int before = h.task.obj.obj_size;
    for (int i = 0; i < 10; ++i){
        if (pddlHomomorphicTaskCollapseRandomPair(&h, 0, &C.err) < 0){
            pddlErrPrint(&C.err, 1, stderr);
            return;
        }
    }
    int after = h.task.obj.obj_size;
    printf("Reduced %d -> %d\n", before, after);
    pddlPrintDebug(&h.task, stdout);
}

TEST(homomorphism_random_pair_goal, homomorphism)
{
    int before = h.task.obj.obj_size;
    for (int i = 0; i < 10; ++i){
        if (pddlHomomorphicTaskCollapseRandomPair(&h, 1, &C.err) < 0){
            pddlErrPrint(&C.err, 1, stderr);
            return;
        }
    }
    int after = h.task.obj.obj_size;
    printf("Reduced %d -> %d\n", before, after);
    pddlPrintDebug(&h.task, stdout);
}

TEST(homomorphism_gaifman, homomorphism)
{
    int before = h.task.obj.obj_size;
    for (int i = 0; i < 10; ++i){
        if (pddlHomomorphicTaskCollapseGaifman(&h, 0, &C.err) < 0){
            pddlErrPrint(&C.err, 1, stderr);
            return;
        }
    }
    int after = h.task.obj.obj_size;
    printf("Reduced %d -> %d\n", before, after);
    pddlPrintDebug(&h.task, stdout);
}

TEST(homomorphism_gaifman_goal, homomorphism)
{
    int before = h.task.obj.obj_size;
    for (int i = 0; i < 10; ++i){
        if (pddlHomomorphicTaskCollapseGaifman(&h, 1, &C.err) < 0){
            pddlErrPrint(&C.err, 1, stderr);
            return;
        }
    }
    int after = h.task.obj.obj_size;
    printf("Reduced %d -> %d\n", before, after);
    pddlPrintDebug(&h.task, stdout);
}

TEST(homomorphism_rpg1, homomorphism)
{
    int before = h.task.obj.obj_size;
    for (int i = 0; i < 10; ++i){
        if (pddlHomomorphicTaskCollapseRPG(&h, 0, 1, &C.err) < 0){
            pddlErrPrint(&C.err, 1, stderr);
            return;
        }
    }
    int after = h.task.obj.obj_size;
    printf("Reduced %d -> %d\n", before, after);
    pddlPrintDebug(&h.task, stdout);
}

TEST(homomorphism_rpg1_goal, homomorphism)
{
    int before = h.task.obj.obj_size;
    for (int i = 0; i < 10; ++i){
        if (pddlHomomorphicTaskCollapseRPG(&h, 1, 1, &C.err) < 0){
            pddlErrPrint(&C.err, 1, stderr);
            return;
        }
    }
    int after = h.task.obj.obj_size;
    printf("Reduced %d -> %d\n", before, after);
    pddlPrintDebug(&h.task, stdout);
}


TEST_COND(homomorphism_endomorph, homomorphism, CPOPTIMIZER)
{
    int before = h.task.obj.obj_size;
    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;
    if (pddlHomomorphicTaskApplyRelaxedEndomorphism(&h, &cfg, &C.err) < 0){
        pddlErrPrint(&C.err, 1, stderr);
        return;
    }
    int after = h.task.obj.obj_size;
    printf("Reduced %d -> %d\n", before, after);
    pddlPrintDebug(&h.task, stdout);
}

TEST_COND(homomorphism_endomorph_nocost, homomorphism, CPOPTIMIZER)
{
    int before = h.task.obj.obj_size;
    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;
    cfg.ignore_costs = 1;
    if (pddlHomomorphicTaskApplyRelaxedEndomorphism(&h, &cfg, &C.err) < 0){
        pddlErrPrint(&C.err, 1, stderr);
        return;
    }
    int after = h.task.obj.obj_size;
    printf("Reduced %d -> %d\n", before, after);
    pddlPrintDebug(&h.task, stdout);
}

TEST_COND(homomorphism_reduce, homomorphism, CPOPTIMIZER)
{
    if (h.task.obj.obj_size / 2 == 0)
        return;

    //printf("target: %d\n", h.task.obj.obj_size / 2);
    //pddlErrInfoEnable(&C.err, stdout);
    int before = h.task.obj.obj_size;
    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;
    pddl_homomorphic_task_reduce_t r;
    pddlHomomorphicTaskReduceInit(&r, C.pddl.obj.obj_size / 2);
    pddlHomomorphicTaskReduceAddRandomPair(&r, 1);
    pddlHomomorphicTaskReduceAddGaifman(&r, 1);
    pddlHomomorphicTaskReduceAddRelaxedEndomorphism(&r, &cfg);
    if (pddlHomomorphicTaskReduce(&r, &h, &C.err) < 0){
        pddlErrPrint(&C.err, 1, stderr);
        return;
    }
    int after = h.task.obj.obj_size;
    printf("Reduced %d -> %d\n", before, after);
    pddlPrintDebug(&h.task, stdout);
    pddlHomomorphicTaskReduceFree(&r);
}
