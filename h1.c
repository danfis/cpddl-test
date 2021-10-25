#include <assert.h>
#include "test.h"
#include "context.h"

TEST(h1, strips_noce)
{
    pddl_strips_t strips;
    pddlStripsInitCopy(&strips, &C.strips);

    if (borISetSize(&strips.init) > 0)
        strips.init.size = BOR_MAX(1, strips.init.size - 1);

    BOR_ISET(unreachable_fact);
    BOR_ISET(unreachable_op);

    //borErrInfoEnable(&err, stdout);
    int ret = pddlH1(&strips, &unreachable_fact, &unreachable_op, &C.err);
    //pddlStripsPrintDebug(&strips, stdout);
    assert(ret == 0);

    if (borISetSize(&unreachable_fact) > 0
            || borISetSize(&unreachable_op) > 0){
        fprintf(stdout, "Init:");
        int fact;
        BOR_ISET_FOR_EACH(&strips.init, fact)
            fprintf(stdout, " (%s)", strips.fact.fact[fact]->name);
        fprintf(stdout, "\n");
    }

    if (borISetSize(&unreachable_fact) > 0){
        fprintf(stdout, "Unreachable facts [%d/%d]:\n",
                borISetSize(&unreachable_fact), strips.fact.fact_size);
        int fact;
        BOR_ISET_FOR_EACH(&unreachable_fact, fact){
            fprintf(stdout, "  (%s)\n", strips.fact.fact[fact]->name);
        }
    }
    if (borISetSize(&unreachable_op) > 0){
        fprintf(stdout, "Unreachable ops [%d/%d]:\n",
                borISetSize(&unreachable_op), strips.op.op_size);
        int op;
        BOR_ISET_FOR_EACH(&unreachable_op, op)
            fprintf(stdout, "  (%s)\n", strips.op.op[op]->name);
    }

    borISetFree(&unreachable_fact);
    borISetFree(&unreachable_op);
    pddlStripsFree(&strips);
}
