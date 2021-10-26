#include <assert.h>
#include "test.h"
#include "context.h"

TEST(h2, strips_noce)
{
    BOR_ISET(unreachable_fact);
    BOR_ISET(unreachable_op);
    pddlMutexPairsInitStrips(&C.mutex, &C.strips);

    //borErrInfoEnable(&err, stdout);
    int ret = pddlH2(&C.strips, &C.mutex, &unreachable_fact, &unreachable_op,
                     0., &C.err);
    assert(ret == 0);

    if (C.mutex.num_mutex_pairs > 0)
        fprintf(stdout, "Mutex pairs: %lu\n",
                (unsigned long)C.mutex.num_mutex_pairs);

    if (borISetSize(&unreachable_fact) > 0){
        fprintf(stdout, "Unreachable facts [%d/%d]:\n",
                borISetSize(&unreachable_fact), C.strips.fact.fact_size);
        int fact;
        BOR_ISET_FOR_EACH(&unreachable_fact, fact){
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
        }
    }
    if (borISetSize(&unreachable_op) > 0){
        fprintf(stdout, "Unreachable ops [%d/%d]:\n",
                borISetSize(&unreachable_op), C.strips.op.op_size);
        int op;
        BOR_ISET_FOR_EACH(&unreachable_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    borISetFree(&unreachable_fact);
    borISetFree(&unreachable_op);
}

TEST_TEAR_DOWN(h2)
{
    pddlMutexPairsFree(&C.mutex);
}

TEST(h2mgroup, h2)
{
    pddl_mgroups_t mgs;
    pddlMGroupsInitEmpty(&mgs);
    pddlMutexPairsInferMutexGroups(&C.mutex, &mgs, &C.err);
    pddlMGroupsSortUniq(&mgs);
    fprintf(stdout, "Mutex groups: %d\n", mgs.mgroup_size);
    pddlMGroupsPrint(&C.pddl, &C.strips, &mgs, stdout);
    pddlMGroupsFree(&mgs);
}

TEST(h2fwbw, ground_lifted_mgroup_noce)
{
    BOR_ISET(unreachable_fact_fw);
    BOR_ISET(unreachable_op_fw);

    pddl_mutex_pairs_t mutex_fw;
    pddlMutexPairsInitStrips(&mutex_fw, &C.strips);
    pddlH2(&C.strips, &mutex_fw, &unreachable_fact_fw, &unreachable_op_fw,
           0., &C.err);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &C.strips);

    BOR_ISET(unreachable_fact);
    BOR_ISET(unreachable_op);

    //borErrInfoEnable(&err, stdout);
    int ret = pddlH2FwBw(&C.strips, &C.mg, &mutex,
                         &unreachable_fact, &unreachable_op, 0., &C.err);
    assert(ret == 0);

    assert(borISetIsSubset(&unreachable_fact_fw, &unreachable_fact));
    assert(borISetIsSubset(&unreachable_op_fw, &unreachable_op));

    if (mutex.num_mutex_pairs - mutex_fw.num_mutex_pairs > 0){
        unsigned long num_fw = 0;
        unsigned long num_bw = 0;
        unsigned long num = 0;
        PDDL_MUTEX_PAIRS_FOR_EACH(&mutex, f1, f2){
            if (f1 == f2)
                continue;
            ++num;
            if (pddlMutexPairsIsFwMutex(&mutex, f1, f2))
                ++num_fw;
            if (pddlMutexPairsIsBwMutex(&mutex, f1, f2))
                ++num_bw;
        }
        assert(num == mutex.num_mutex_pairs);
        fprintf(stdout, "Mutex pairs: %lu -> %lu, fw: %lu, bw: %lu\n",
                (unsigned long)mutex_fw.num_mutex_pairs,
                (unsigned long)mutex.num_mutex_pairs,
                num_fw,
                num_bw);
        assert(num_fw + num_bw == mutex.num_mutex_pairs);
    }

    PDDL_MUTEX_PAIRS_FOR_EACH(&mutex, f1, f2){
        if (f1 == f2){
            assert(borISetIn(f1, &unreachable_fact));
        }
    }

    BOR_ISET(rm);
    borISetMinus2(&rm, &unreachable_fact, &unreachable_fact_fw);
    if (borISetSize(&rm) > 0){
        fprintf(stdout, "Unreachable facts [%d + %d/%d]:\n",
                borISetSize(&unreachable_fact_fw),
                borISetSize(&rm), C.strips.fact.fact_size);
        int fact;
        BOR_ISET_FOR_EACH(&rm, fact){
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
        }
    }

    borISetMinus2(&rm, &unreachable_op, &unreachable_op_fw);
    if (borISetSize(&rm) > 0){
        fprintf(stdout, "Unreachable ops [%d + %d/%d]:\n",
                borISetSize(&unreachable_op_fw),
                borISetSize(&rm), C.strips.op.op_size);
        int op;
        BOR_ISET_FOR_EACH(&rm, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    borISetFree(&rm);

    pddl_mutex_pairs_t mutex2;
    pddlMutexPairsInitCopy(&mutex2, &mutex);
    PDDL_MUTEX_PAIRS_FOR_EACH(&mutex, f1, f2){
        assert(pddlMutexPairsIsMutex(&mutex2, f1, f2));
        if (pddlMutexPairsIsFwMutex(&mutex, f1, f2)){
            assert(pddlMutexPairsIsFwMutex(&mutex2, f1, f2));
        }
        if (pddlMutexPairsIsBwMutex(&mutex, f1, f2)){
            assert(pddlMutexPairsIsBwMutex(&mutex2, f1, f2));
        }
    }

    if (borISetSize(&unreachable_fact) > 0){
        int *remap = BOR_CALLOC_ARR(int, C.strips.fact.fact_size);
        int new_size = pddlFactsDelFactsGenRemap(C.strips.fact.fact_size,
                                                 &unreachable_fact, remap);
        pddlMutexPairsRemapFacts(&mutex2, new_size, remap);
        PDDL_MUTEX_PAIRS_FOR_EACH(&mutex, f1, f2){
            if (remap[f1] < 0 || remap[f2] < 0)
                continue;
            if (pddlMutexPairsIsMutex(&mutex, f1, f2)){
                assert(pddlMutexPairsIsMutex(&mutex2, remap[f1], remap[f2]));
            }
            if (pddlMutexPairsIsFwMutex(&mutex, f1, f2)){
                assert(pddlMutexPairsIsFwMutex(&mutex2, remap[f1], remap[f2]));
            }
            if (pddlMutexPairsIsBwMutex(&mutex, f1, f2)){
                assert(pddlMutexPairsIsBwMutex(&mutex2, remap[f1], remap[f2]));
            }
        }
        BOR_FREE(remap);
    }
    pddlMutexPairsFree(&mutex2);
    

    pddlMutexPairsFree(&mutex);
    borISetFree(&unreachable_fact);
    borISetFree(&unreachable_op);
    pddlMutexPairsFree(&mutex_fw);
    borISetFree(&unreachable_fact_fw);
    borISetFree(&unreachable_op_fw);
}
