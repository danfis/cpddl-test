#include <assert.h>
#include "test.h"
#include "context.h"

TEST(irrelevance, strips_pruned)
{
    BOR_ISET(irr_fact);
    BOR_ISET(irr_op);
    BOR_ISET(static_fact);

    int ret = pddlIrrelevanceAnalysis(&C.strips, &irr_fact, &irr_op,
                                      &static_fact, &C.err);
    assert(ret == 0);
    int fact, op;
    if (borISetSize(&irr_fact) > 0){
        fprintf(stdout, "Irrelevant facts [%d/%d, static: %d]:\n",
                borISetSize(&irr_fact), C.strips.fact.fact_size,
                borISetSize(&static_fact));
        BOR_ISET_FOR_EACH(&irr_fact, fact){
            fprintf(stdout, "  ");
            if (borISetIn(fact, &static_fact))
                fprintf(stdout, "S:");
            fprintf(stdout, "(%s)\n", C.strips.fact.fact[fact]->name);
        }
    }
    if (borISetSize(&irr_op) > 0){
        fprintf(stdout, "Irrelevant ops [%d/%d]:\n",
                borISetSize(&irr_op), C.strips.op.op_size);
        BOR_ISET_FOR_EACH(&irr_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    borISetFree(&irr_fact);
    borISetFree(&irr_op);
    borISetFree(&static_fact);
}
