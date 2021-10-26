#include <assert.h>
#include "test.h"
#include "context.h"


TEST(h3, h2)
{
    BOR_ISET(unreachable_fact);
    BOR_ISET(unreachable_op);

    pddlMutexPairsInitStrips(&C.mutex3, &C.strips);

    int ret = pddlH3(&C.strips, &C.mutex3, &unreachable_fact, &unreachable_op,
                     10., 0, &C.err);
    assert(ret == 0);

    assert(borISetIsSubset(&C.mutex_unreachable_fact, &unreachable_fact));
    assert(borISetIsSubset(&C.mutex_unreachable_op, &unreachable_op));
    PDDL_MUTEX_PAIRS_FOR_EACH(&C.mutex, f1, f2){
        assert(pddlMutexPairsIsMutex(&C.mutex3, f1, f2));
    }

    if (C.mutex3.num_mutex_pairs > 0)
        fprintf(stdout, "Mutex pairs: %lu\n",
                (unsigned long)C.mutex3.num_mutex_pairs);

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

TEST_TEAR_DOWN(h3)
{
    pddlMutexPairsFree(&C.mutex3);
}

TEST(h3mgroup, h3)
{
    pddl_mgroups_t mgs;
    pddlMGroupsInitEmpty(&mgs);
    pddlMutexPairsInferMutexGroups(&C.mutex3, &mgs, &C.err);
    pddlMGroupsSortUniq(&mgs);
    fprintf(stdout, "Mutex groups: %d\n", mgs.mgroup_size);
    pddlMGroupsPrint(&C.pddl, &C.strips, &mgs, stdout);
    pddlMGroupsFree(&mgs);
}
