#include <assert.h>
#include "test.h"
#include "context.h"

TEST(asnets, r)
{
}

TEST(asnets_task, asnets)
{
    pddl_asnets_task_t task;
    int ret = pddlASNetsTaskInit(&task, C.files.domain_pddl,
                                 C.files.problem_pddl, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    for (int oi = 0; oi < task.strips.op.op_size; ++oi){
        assert(strcmp(task.strips.op.op[oi]->name,
                      task.fdr.op.op[oi]->name) == 0);
        fprintf(stderr, "X %s\n", task.strips.op.op[oi]->name);
        fflush(stderr);
        PDDL_ISET(pos);
        for (int reli = 0; reli < task.relatedness.rel_size; ++reli){
            const pddl_asnets_task_relate_t *rel;
            rel = task.relatedness.rel + reli;
            if (rel->op_id == oi){
                pddlISetAdd(&pos, rel->position);
            }
        }
        if (pddlISetSize(&pos) > 0 && pddlISetGet(&pos, 0) != 0){
            fprintf(stdout, "%s\n", task.strips.op.op[oi]->name);
        }
        if (pddlISetSize(&pos) > 0){
            assert(pddlISetGet(&pos, 0) == 0);
            assert(pddlISetGet(&pos, pddlISetSize(&pos) - 1) == pddlISetSize(&pos) - 1);
        }
        int num_pos = task.pddl_action[task.strips.op.op[oi]->pddl_action_id].atom.size;
        assert(pddlISetSize(&pos) == num_pos);
        pddlISetFree(&pos);
    }

    /*
    pddlPrintPDDLDomain(&task.pddl, stdout);
    pddlStripsPrintDebug(&task.strips, stdout);

    printf("\n%d\n", task.relatedness.rel_size);
    for (int i = 0; i < task.relatedness.rel_size; ++i){
        printf("%d %d %d -- %s %s\n",
               task.relatedness.rel[i].op_id,
               task.relatedness.rel[i].fact_id,
               task.relatedness.rel[i].position,
               task.strips.op.op[task.relatedness.rel[i].op_id]->name,
               task.strips.fact.fact[task.relatedness.rel[i].fact_id]->name);
    }
    */
    pddlASNetsTaskFree(&task);
}
