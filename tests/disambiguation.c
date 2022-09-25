#include <assert.h>
#include "test.h"
#include "context.h"

static int disamb(pddl_disambiguate_t *dis,
                  const pddl_strips_t *strips,
                  const pddl_iset_t *s1,
                  pddl_iset_t *s2,
                  const char *header)
{
    int ret = pddlDisambiguateSet(dis, s2);
    if (ret > 0){
        fprintf(stdout, "%s\n", header);
        fprintf(stdout, "  ");
        pddlFactsPrintSet(s1, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");
        fprintf(stdout, "   +");
        PDDL_ISET(add);
        pddlISetMinus2(&add, s2, s1);
        pddlFactsPrintSet(&add, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");
        pddlISetFree(&add);
    }

    return ret;
}

TEST(disambiguation, ground_lifted_mgroup)
{
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &C.strips);
    pddlMutexPairsAddMGroups(&mutex, &C.mg);

    //pddlErrInfoEnable(&err, stdout);
    PDDL_ISET(irr_op2);
    int ret = pddlH2(&C.strips, &mutex, NULL, &irr_op2, 0., &C.err);

    if (C.strips.has_cond_eff){
        assert(ret == -1);
        pddlErrPrint(&C.err, 0, stdout);
    }else{
        assert(ret == 0);
    }

    /*
    pddlStripsPrintDebug(&strips, stdout);
    PDDL_MUTEX_PAIRS_FOR_EACH(&mutex, f1, f2){
        fprintf(stdout, "M (%s) (%s)\n", strips.fact.fact[f1]->name,
                strips.fact.fact[f2]->name);
    }
    */


    pddl_strips_t strips2;

    pddlStripsInitCopy(&strips2, &C.strips);

    pddl_disambiguate_t dis;
    pddlDisambiguateInit(&dis, C.strips.fact.fact_size, &mutex, &C.mg);
    if (disamb(&dis, &C.strips, &C.strips.goal, &strips2.goal, "Goal:") < 0){
        fprintf(stdout, "Unsolvable\n");
    }else{
        for (int op_id = 0; op_id < C.strips.op.op_size && op_id < 500; ++op_id){
            if (pddlISetIn(op_id, &irr_op2))
                continue;
            const pddl_strips_op_t *op = C.strips.op.op[op_id];
            pddl_strips_op_t *op2 = strips2.op.op[op_id];
            char header[128];
            snprintf(header, 128, "(%s)", op->name);
            header[127] = 0;
            ret = disamb(&dis, &C.strips, &op->pre, &op2->pre, header);
            if (ret < 0)
                fprintf(stdout, "Unreachable: (%s)\n", op->name);
        }
    }
    pddlDisambiguateFree(&dis);

    pddlISetFree(&irr_op2);

    pddlStripsFree(&strips2);
    pddlMutexPairsFree(&mutex);
}
