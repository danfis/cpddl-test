#include <assert.h>
#include "test.h"
#include "context.h"

context_t C = { 0 };

TEST(root, _)
{
    borErrInit(&C.err);
    if (pddlFiles(&C.files, "pddl-data/", TEST_TASK, &C.err) != 0){
        borErrPrint(&C.err, 1, stderr);
        assert(0);
    }
}

TEST_TEAR_DOWN(root)
{
}

TEST_GLOBAL_TEAR_DOWN()
{
    if (C.trans_systems_set)
        pddlTransSystemsFree(&C.trans_systems);
    if (C.mg_strips_set)
        pddlMGStripsFree(&C.mg_strips);
    if (C.mutex3_set)
        pddlMutexPairsFree(&C.mutex3);
    borISetFree(&C.mutex_unreachable_fact);
    borISetFree(&C.mutex_unreachable_op);
    if (C.mutex_set)
        pddlMutexPairsFree(&C.mutex);
    if (C.strips_sym_set)
        pddlStripsSymFree(&C.strips_sym);
    if (C.mg_set)
        pddlMGroupsFree(&C.mg);
    if (C.strips_set)
        pddlStripsFree(&C.strips);
    if (C.lmg_set)
        pddlLiftedMGroupsFree(&C.lmg);
    if (C.pddl_set)
        pddlFree(&C.pddl);
}
