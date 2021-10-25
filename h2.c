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
    /*
       sprintf(fn, "reg/tmp.TSH2.testH2_%s.mutex_groups.out", outfn);
       fout = fopen(fn, "w");
       if (fout != NULL){
       pddlMGroupsPrint(&pddl, &strips, &mgs, fout);
       fclose(fout);
       }
     */
    if (mgs.mgroup_size > 0)
        fprintf(stdout, "Mutex groups: %d\n", mgs.mgroup_size);
    pddlMGroupsFree(&mgs);
}

