#include <assert.h>
#include "test.h"
#include "context.h"

static pddl_mutex_pairs_t h2;
static pddl_iset_t h2_unreachable_op;
static pddl_iset_t h2_unreachable_fact;

TEST(h2, strips_pruned)
{
    pddlMutexPairsInitStrips(&h2, &C.strips);
    pddlISetInit(&h2_unreachable_op);
    pddlISetInit(&h2_unreachable_fact);

    //pddlErrInfoEnable(&err, stdout);
    int ret = pddlH2(&C.strips, &h2, &h2_unreachable_fact, &h2_unreachable_op,
                     0., &C.err);
    assert(ret == 0);

    if (h2.num_mutex_pairs > 0)
        fprintf(stdout, "Mutex pairs: %lu\n", (unsigned long)h2.num_mutex_pairs);

    if (pddlISetSize(&h2_unreachable_fact) > 0){
        fprintf(stdout, "Unreachable facts [%d/%d]:\n",
                pddlISetSize(&h2_unreachable_fact), C.strips.fact.fact_size);
        int fact;
        PDDL_ISET_FOR_EACH(&h2_unreachable_fact, fact){
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
        }
    }
    if (pddlISetSize(&h2_unreachable_op) > 0){
        fprintf(stdout, "Unreachable ops [%d/%d]:\n",
                pddlISetSize(&h2_unreachable_op), C.strips.op.op_size);
        int op;
        PDDL_ISET_FOR_EACH(&h2_unreachable_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
}

TEST_TEAR_DOWN(h2)
{
    pddlMutexPairsFree(&h2);
    pddlISetFree(&h2_unreachable_op);
    pddlISetFree(&h2_unreachable_fact);
}

TEST(h2fwbw, h2)
{
    PDDL_ISET(h2fwbw_unreachable_op);
    PDDL_ISET(h2fwbw_unreachable_fact);
    pddl_mutex_pairs_t h2fwbw;
    pddlMutexPairsInitStrips(&h2fwbw, &C.strips);
    pddlISetInit(&h2fwbw_unreachable_fact);
    pddlISetInit(&h2fwbw_unreachable_op);

    //pddlErrInfoEnable(&err, stdout);
    int ret = pddlH2FwBw(&C.strips, &C.mg, &h2fwbw,
                         &h2fwbw_unreachable_fact, &h2fwbw_unreachable_op,
                         0., &C.err);
    assert(ret == 0);

    assert(pddlISetIsSubset(&h2_unreachable_fact, &h2fwbw_unreachable_fact));
    assert(pddlISetIsSubset(&h2_unreachable_op, &h2fwbw_unreachable_op));

    if (h2fwbw.num_mutex_pairs - h2.num_mutex_pairs > 0){
        unsigned long num_fw = 0;
        unsigned long num_bw = 0;
        unsigned long num = 0;
        PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw, f1, f2){
            if (f1 == f2)
                continue;
            ++num;
            if (pddlMutexPairsIsFwMutex(&h2fwbw, f1, f2))
                ++num_fw;
            if (pddlMutexPairsIsBwMutex(&h2fwbw, f1, f2))
                ++num_bw;
        }
        assert(num == h2fwbw.num_mutex_pairs);
        fprintf(stdout, "Mutex pairs: %lu -> %lu, fw: %lu, bw: %lu\n",
                (unsigned long)h2.num_mutex_pairs,
                (unsigned long)h2fwbw.num_mutex_pairs,
                num_fw,
                num_bw);
        assert(num_fw + num_bw == h2fwbw.num_mutex_pairs);
    }

    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw, f1, f2){
        if (f1 == f2){
            assert(pddlISetIn(f1, &h2fwbw_unreachable_fact));
        }
    }

    PDDL_ISET(rm);
    pddlISetMinus2(&rm, &h2fwbw_unreachable_fact, &h2_unreachable_fact);
    if (pddlISetSize(&rm) > 0){
        fprintf(stdout, "Unreachable facts [%d + %d/%d]:\n",
                pddlISetSize(&h2_unreachable_fact),
                pddlISetSize(&rm), C.strips.fact.fact_size);
        int fact;
        PDDL_ISET_FOR_EACH(&rm, fact){
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
        }
    }

    pddlISetMinus2(&rm, &h2fwbw_unreachable_op, &h2_unreachable_op);
    if (pddlISetSize(&rm) > 0){
        fprintf(stdout, "Unreachable ops [%d + %d/%d]:\n",
                pddlISetSize(&h2_unreachable_op),
                pddlISetSize(&rm), C.strips.op.op_size);
        int op;
        PDDL_ISET_FOR_EACH(&rm, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    pddlISetFree(&rm);

    pddl_mutex_pairs_t mutex2;
    pddlMutexPairsInitCopy(&mutex2, &h2fwbw);
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw, f1, f2){
        assert(pddlMutexPairsIsMutex(&mutex2, f1, f2));
        if (pddlMutexPairsIsFwMutex(&h2fwbw, f1, f2)){
            assert(pddlMutexPairsIsFwMutex(&mutex2, f1, f2));
        }
        if (pddlMutexPairsIsBwMutex(&h2fwbw, f1, f2)){
            assert(pddlMutexPairsIsBwMutex(&mutex2, f1, f2));
        }
    }

    if (pddlISetSize(&h2fwbw_unreachable_fact) > 0){
        int *remap = PDDL_CALLOC_ARR(int, C.strips.fact.fact_size);
        int new_size = pddlFactsDelFactsGenRemap(C.strips.fact.fact_size,
                                                 &h2fwbw_unreachable_fact, remap);
        pddlMutexPairsRemapFacts(&mutex2, new_size, remap);
        PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw, f1, f2){
            if (remap[f1] < 0 || remap[f2] < 0)
                continue;
            if (pddlMutexPairsIsMutex(&h2fwbw, f1, f2)){
                assert(pddlMutexPairsIsMutex(&mutex2, remap[f1], remap[f2]));
            }
            if (pddlMutexPairsIsFwMutex(&h2fwbw, f1, f2)){
                assert(pddlMutexPairsIsFwMutex(&mutex2, remap[f1], remap[f2]));
            }
            if (pddlMutexPairsIsBwMutex(&h2fwbw, f1, f2)){
                assert(pddlMutexPairsIsBwMutex(&mutex2, remap[f1], remap[f2]));
            }
        }
        PDDL_FREE(remap);
    }
    pddlMutexPairsFree(&mutex2);
    

    pddlMutexPairsFree(&h2fwbw);
    pddlISetFree(&h2fwbw_unreachable_fact);
    pddlISetFree(&h2fwbw_unreachable_op);
}

TEST(h3, h2)
{
    pddl_mutex_pairs_t h3;
    PDDL_ISET(unreachable_fact);
    PDDL_ISET(unreachable_op);

    pddlMutexPairsInitStrips(&h3, &C.strips);

    int ret = pddlH3(&C.strips, &h3, &unreachable_fact, &unreachable_op,
                     -1., 0, &C.err);
    assert(ret == 0);

    assert(pddlISetIsSubset(&h2_unreachable_fact, &unreachable_fact));
    assert(pddlISetIsSubset(&h2_unreachable_op, &unreachable_op));
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2, f1, f2){
        assert(pddlMutexPairsIsMutex(&h3, f1, f2));
    }

    if (h3.num_mutex_pairs > 0)
        fprintf(stdout, "Mutex pairs: %lu\n",
                (unsigned long)h3.num_mutex_pairs);

    if (pddlISetSize(&unreachable_fact) > 0){
        fprintf(stdout, "Unreachable facts [%d/%d]:\n",
                pddlISetSize(&unreachable_fact), C.strips.fact.fact_size);
        int fact;
        PDDL_ISET_FOR_EACH(&unreachable_fact, fact){
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
        }
    }
    if (pddlISetSize(&unreachable_op) > 0){
        fprintf(stdout, "Unreachable ops [%d/%d]:\n",
                pddlISetSize(&unreachable_op), C.strips.op.op_size);
        int op;
        PDDL_ISET_FOR_EACH(&unreachable_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }

    pddlISetFree(&unreachable_fact);
    pddlISetFree(&unreachable_op);
    pddlMutexPairsFree(&h3);
}


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

TEST(disambiguation, h2)
{
    pddl_strips_t strips2;

    pddlStripsInitCopy(&strips2, &C.strips);

    pddl_disambiguate_t dis;
    pddlDisambiguateInit(&dis, C.strips.fact.fact_size, &h2, &C.mg);
    if (disamb(&dis, &C.strips, &C.strips.goal, &strips2.goal, "Goal:") < 0){
        fprintf(stdout, "Unsolvable\n");
    }else{
        for (int op_id = 0; op_id < C.strips.op.op_size && op_id < 500; ++op_id){
            const pddl_strips_op_t *op = C.strips.op.op[op_id];
            pddl_strips_op_t *op2 = strips2.op.op[op_id];
            char header[128];
            snprintf(header, 128, "(%s)", op->name);
            header[127] = 0;
            int ret = disamb(&dis, &C.strips, &op->pre, &op2->pre, header);
            if (ret < 0)
                fprintf(stdout, "Unreachable: (%s)\n", op->name);
        }
    }
    pddlDisambiguateFree(&dis);

    pddlStripsFree(&strips2);
}
