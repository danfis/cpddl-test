#include "test.h"
#include "context.h"
#include <assert.h>

TEST(irrelevance, strips_pruned)
{
    PDDL_ISET(irr_fact);
    PDDL_ISET(irr_op);
    PDDL_ISET(static_fact);

    int ret = pddlIrrelevanceAnalysis(&C.strips, &irr_fact, &irr_op,
                                      &static_fact, &C.err);
    assert(ret == 0);
    if (pddlISetSize(&irr_fact) > 0){
        fprintf(stdout, "Irrelevant facts [%d/%d, static: %d]:\n",
                pddlISetSize(&irr_fact), C.strips.fact.fact_size,
                pddlISetSize(&static_fact));
        PDDL_ISET_FOR_EACH(&irr_fact, fact){
            fprintf(stdout, "  ");
            if (pddlISetIn(fact, &static_fact))
                fprintf(stdout, "S:");
            fprintf(stdout, "(%s)\n", C.strips.fact.fact[fact]->name);
        }
    }
    if (pddlISetSize(&irr_op) > 0){
        fprintf(stdout, "Irrelevant ops [%d/%d]:\n",
                pddlISetSize(&irr_op), C.strips.op.op_size);
        PDDL_ISET_FOR_EACH(&irr_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    pddlISetFree(&irr_fact);
    pddlISetFree(&irr_op);
    pddlISetFree(&static_fact);
}
