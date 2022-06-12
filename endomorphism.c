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
    cfg.run_in_subprocess = 1;
    pddl_endomorphism_sol_t sol;
    int ret = pddlEndomorphismFDR(&C.fdr, &cfg, &sol, &C.err);
    assert(ret == 0);
    if (pddlISetSize(&sol.redundant_ops) == 0){
        for (int i = 0; i < C.fdr.op.op_size; ++i)
            assert(sol.op_map[i] == i);

    }else{
        pddlISetUnion(&redundant, &sol.redundant_ops);
        if (sol.is_optimal)
            printf("num redundant ops: %d\n", pddlISetSize(&sol.redundant_ops));

        int opi;
        PDDL_ISET_FOR_EACH(&sol.redundant_ops, opi){
            assert(sol.op_map[opi] != opi);
            assert(sol.op_map[sol.op_map[opi]] == sol.op_map[opi]);
        }

        int cnt = 0;
        for (int i = 0; i < C.fdr.op.op_size; ++i){
            if (sol.op_map[i] == i)
                cnt++;
        }
        assert(cnt == C.fdr.op.op_size - pddlISetSize(&sol.redundant_ops));
    }
    pddlEndomorphismSolFree(&sol);
}

TEST_TEAR_DOWN(endomorphism_fdr)
{
    pddlISetFree(&redundant);
}

TEST_COND(endomorphism_tss, endomorphism_fdr, CPOPTIMIZER)
{
    pddl_mg_strips_t mg_strips;
    pddlMGStripsInitFDR(&mg_strips, &C.fdr);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);
    pddlH2(&mg_strips.strips, &mutex, NULL, NULL, -1, &C.err);

    pddl_trans_systems_t tss;
    pddlTransSystemsInit(&tss, &mg_strips, &mutex);

    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;
    cfg.run_in_subprocess = 1;
    pddl_endomorphism_sol_t sol;
    int ret = pddlEndomorphismTransSystem(&tss, &cfg, &sol, &C.err);
    assert(ret == 0);
    if (pddlISetSize(&sol.redundant_ops) == 0){
        for (int i = 0; i < C.fdr.op.op_size; ++i)
            assert(sol.op_map[i] == i);

    }else{
        pddlISetUnion(&redundant, &sol.redundant_ops);
        if (sol.is_optimal){
            printf("num redundant ops: %d\n", pddlISetSize(&sol.redundant_ops));
            assert(pddlISetSize(&sol.redundant_ops) >= pddlISetSize(&redundant));
        }

        int opi;
        PDDL_ISET_FOR_EACH(&sol.redundant_ops, opi){
            assert(sol.op_map[opi] != opi);
            assert(sol.op_map[sol.op_map[opi]] == sol.op_map[opi]);
        }

        int cnt = 0;
        for (int i = 0; i < C.fdr.op.op_size; ++i){
            if (sol.op_map[i] == i)
                cnt++;
        }
        assert(cnt == C.fdr.op.op_size - pddlISetSize(&sol.redundant_ops));
    }
    pddlEndomorphismSolFree(&sol);

    pddlMutexPairsFree(&mutex);
    pddlTransSystemsFree(&tss);
    pddlMGStripsFree(&mg_strips);
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

TEST_COND(endomorphism_tss_minizinc, endomorphism_fdr_minizinc, MINIZINC)
{
    pddlCPSetDefaultSolver(PDDL_CP_SOLVER_MINIZINC);
    test_endomorphism_tss();
}
