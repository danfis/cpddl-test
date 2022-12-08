#include "test.h"
#include "context.h"
#include <assert.h>

TEST(h1, strips_pruned)
{
    pddl_strips_t strips;
    pddlStripsInitCopy(&strips, &C.strips);

    if (pddlISetSize(&strips.init) > 0)
        strips.init.size = PDDL_MAX(1, strips.init.size - 1);

    PDDL_ISET(unreachable_fact);
    PDDL_ISET(unreachable_op);

    //pddlErrInfoEnable(&err, stdout);
    int ret = pddlH1(&strips, &unreachable_fact, &unreachable_op, &C.err);
    //pddlStripsPrintDebug(&strips, stdout);
    assert(ret == 0);

    if (pddlISetSize(&unreachable_fact) > 0
            || pddlISetSize(&unreachable_op) > 0){
        fprintf(stdout, "Init:");
        int fact;
        PDDL_ISET_FOR_EACH(&strips.init, fact)
            fprintf(stdout, " (%s)", strips.fact.fact[fact]->name);
        fprintf(stdout, "\n");
    }

    if (pddlISetSize(&unreachable_fact) > 0){
        fprintf(stdout, "Unreachable facts [%d/%d]:\n",
                pddlISetSize(&unreachable_fact), strips.fact.fact_size);
        int fact;
        PDDL_ISET_FOR_EACH(&unreachable_fact, fact){
            fprintf(stdout, "  (%s)\n", strips.fact.fact[fact]->name);
        }
    }
    if (pddlISetSize(&unreachable_op) > 0){
        fprintf(stdout, "Unreachable ops [%d/%d]:\n",
                pddlISetSize(&unreachable_op), strips.op.op_size);
        int op;
        PDDL_ISET_FOR_EACH(&unreachable_op, op)
            fprintf(stdout, "  (%s)\n", strips.op.op[op]->name);
    }

    pddlISetFree(&unreachable_fact);
    pddlISetFree(&unreachable_op);
    pddlStripsFree(&strips);
}
