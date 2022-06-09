#include "pddl/pddl.h"
#include <assert.h>
#include "test.h"
#include "context.h"

TEST_COND(endomorphism_lifted, lmg, CPOPTIMIZER)
{
    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;

    PDDL_ISET(redundant);
    pddl_obj_id_t *map = PDDL_CALLOC_ARR(pddl_obj_id_t, C.pddl.obj.obj_size);
    int ret = pddlEndomorphismLifted(&C.pddl, &C.lmg, &cfg,
                                     &redundant, map, &C.err);
    assert(ret == 0);
    if (pddlISetSize(&redundant) == 0){
        for (int i = 0; i < C.pddl.obj.obj_size; ++i)
            assert(map[i] == i);

    }else{
        printf("num redundant objects: %d\n", pddlISetSize(&redundant));

        int obj;
        PDDL_ISET_FOR_EACH(&redundant, obj){
            assert(map[obj] != obj);
            assert(map[map[obj]] == map[obj]);
        }

        int cnt = 0;
        for (int i = 0; i < C.pddl.obj.obj_size; ++i){
            if (map[i] == i)
                cnt++;
        }
        assert(cnt == C.pddl.obj.obj_size - pddlISetSize(&redundant));
    }

    PDDL_ISET(redundant2);
    ret = pddlEndomorphismLifted(&C.pddl, &C.lmg, &cfg,
                                 &redundant2, NULL, &C.err);
    assert(ret == 0);
    assert(pddlISetSize(&redundant) == pddlISetSize(&redundant2));
    pddlISetFree(&redundant2);

    pddl_obj_id_t *map2 = PDDL_CALLOC_ARR(pddl_obj_id_t, C.pddl.obj.obj_size);
    ret = pddlEndomorphismLifted(&C.pddl, &C.lmg, &cfg, NULL, map2, &C.err);
    assert(ret == 0);
    int cnt = 0;
    for (int i = 0; i < C.pddl.obj.obj_size; ++i){
        if (map2[i] == i)
            cnt++;
    }
    assert(cnt == C.pddl.obj.obj_size - pddlISetSize(&redundant));
    PDDL_FREE(map2);

    pddlISetFree(&redundant);
    PDDL_FREE(map);
}

TEST_COND(endomorphism_lifted_minizinc, lmg, MINIZINC)
{
    pddlCPSetDefaultSolver(PDDL_CP_SOLVER_MINIZINC);
    test_endomorphism_lifted();
}

TEST_COND(endomorphism_relaxed_lifted, lmg, CPOPTIMIZER)
{
    //pddlErrInfoEnable(&C.err, stderr);
    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;

    PDDL_ISET(redundant);
    pddl_obj_id_t *map = PDDL_CALLOC_ARR(pddl_obj_id_t, C.pddl.obj.obj_size);
    int ret = pddlEndomorphismRelaxedLifted(&C.pddl, &cfg,
                                            &redundant, map, &C.err);
    assert(ret == 0);
    if (pddlISetSize(&redundant) == 0){
        for (int i = 0; i < C.pddl.obj.obj_size; ++i)
            assert(map[i] == i);

    }else{
        printf("num redundant objects: %d\n", pddlISetSize(&redundant));

        int obj;
        PDDL_ISET_FOR_EACH(&redundant, obj){
            assert(map[obj] != obj);
            assert(map[map[obj]] == map[obj]);
        }

        int cnt = 0;
        for (int i = 0; i < C.pddl.obj.obj_size; ++i){
            if (map[i] == i)
                cnt++;
        }
        assert(cnt == C.pddl.obj.obj_size - pddlISetSize(&redundant));
    }

    PDDL_ISET(redundant2);
    ret = pddlEndomorphismRelaxedLifted(&C.pddl, &cfg,
                                        &redundant2, NULL, &C.err);
    assert(ret == 0);
    assert(pddlISetSize(&redundant) == pddlISetSize(&redundant2));
    pddlISetFree(&redundant2);

    pddl_obj_id_t *map2 = PDDL_CALLOC_ARR(pddl_obj_id_t, C.pddl.obj.obj_size);
    ret = pddlEndomorphismRelaxedLifted(&C.pddl, &cfg, NULL, map2, &C.err);
    assert(ret == 0);
    int cnt = 0;
    for (int i = 0; i < C.pddl.obj.obj_size; ++i){
        if (map2[i] == i)
            cnt++;
    }
    assert(cnt == C.pddl.obj.obj_size - pddlISetSize(&redundant));
    PDDL_FREE(map2);

    pddlISetFree(&redundant);
    PDDL_FREE(map);
}

TEST_COND(endomorphism_relaxed_lifted_minizinc, lmg, MINIZINC)
{
    pddlCPSetDefaultSolver(PDDL_CP_SOLVER_MINIZINC);
    test_endomorphism_relaxed_lifted();
}

static pddl_iset_t redundant;

TEST_COND(endomorphism_fdr, fdr, CPOPTIMIZER)
{
    pddlErrInfoEnable(&C.err, stderr);
    pddlISetInit(&redundant);
    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;

    int ret = pddlEndomorphismFDRRedundantOps(&C.fdr, &cfg, &redundant, &C.err);
    assert(ret >= 0);
    if (pddlISetSize(&redundant) > 0 && ret == 0){
        printf("num redundant objects: %d\n", pddlISetSize(&redundant));
    }
}

TEST_TEAR_DOWN(endomorphism_fdr)
{
    pddlISetFree(&redundant);
}

static pddl_mg_strips_t mg_strips;
TEST_COND(endomorphism_mg_strips, endomorphism_fdr, CPOPTIMIZER)
{
    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;

    pddlMGStripsInitFDR(&mg_strips, &C.fdr);

    PDDL_ISET(redundant2);
    int ret = pddlEndomorphismMGStripsRedundantOps(&mg_strips, &cfg, &redundant2, &C.err);
    assert(ret >= 0);
    if (pddlISetSize(&redundant2) > 0 && ret == 0)
        printf("num redundant objects: %d\n", pddlISetSize(&redundant2));
    if (ret == 0)
        assert(pddlISetSize(&redundant2) == pddlISetSize(&redundant));
    pddlISetFree(&redundant2);

}

TEST_TEAR_DOWN(endomorphism_mg_strips)
{
    pddlMGStripsFree(&mg_strips);
}

TEST_COND(endomorphism_tss, endomorphism_mg_strips, CPOPTIMIZER)
{
    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);
    pddlH2(&mg_strips.strips, &mutex, NULL, NULL, -1, &C.err);

    pddl_trans_systems_t tss;
    pddlTransSystemsInit(&tss, &mg_strips, &mutex);

    PDDL_ISET(redundant3);
    int ret = pddlEndomorphismTransSystemRedundantOps(&tss, &cfg, &redundant3, &C.err);
    assert(ret >= 0);
    if (pddlISetSize(&redundant3) > 0 && ret == 0)
        printf("num redundant objects: %d\n", pddlISetSize(&redundant3));
    if (ret == 0)
        assert(pddlISetSize(&redundant3) >= pddlISetSize(&redundant));
    pddlISetFree(&redundant3);

    pddlMutexPairsFree(&mutex);
    pddlTransSystemsFree(&tss);
}

TEST_COND(endomorphism_fdr_minizinc, fdr, MINIZINC)
{
    pddlCPSetDefaultSolver(PDDL_CP_SOLVER_MINIZINC);
    test_endomorphism_fdr();
}

TEST_TEAR_DOWN(endomorphism_fdr_minizinc)
{
    pddlISetFree(&redundant);
}

TEST_COND(endomorphism_mg_strips_minizinc, endomorphism_fdr_minizinc, MINIZINC)
{
    pddlCPSetDefaultSolver(PDDL_CP_SOLVER_MINIZINC);
    test_endomorphism_mg_strips();
}

TEST_TEAR_DOWN(endomorphism_mg_strips_minizinc)
{
    pddlMGStripsFree(&mg_strips);
}

TEST_COND(endomorphism_tss_minizinc, endomorphism_mg_strips_minizinc, MINIZINC)
{
    pddlCPSetDefaultSolver(PDDL_CP_SOLVER_MINIZINC);
    test_endomorphism_tss();
}
