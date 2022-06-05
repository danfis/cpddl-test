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

TEST_COND(endomorphism_relaxed_lifted, lmg, CPOPTIMIZER)
{
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
