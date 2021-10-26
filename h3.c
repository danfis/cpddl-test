#include <assert.h>
#include "test.h"
#include "context.h"

static void dominance(const pddl_strips_t *strips,
                      const pddl_mutex_pairs_t *mutex,
                      const bor_iset_t *ufacts,
                      const bor_iset_t *uops,
                      bor_err_t *err)
{
    BOR_ISET(ufacts2);
    BOR_ISET(uops2);
    pddl_mutex_pairs_t m2;
    pddlMutexPairsInitStrips(&m2, strips);

    //borErrInfoEnable(&err, stdout);
    int ret = pddlH2(strips, &m2, &ufacts2, &uops2, 0., err);

    if (strips->has_cond_eff){
        assert(ret == -1);
    }else{
        assert(ret == 0);
        assert(borISetIsSubset(&ufacts2, ufacts));
        assert(borISetIsSubset(&uops2, uops));
        PDDL_MUTEX_PAIRS_FOR_EACH(&m2, f1, f2){
            assert(pddlMutexPairsIsMutex(mutex, f1, f2));
        }
        /*
        PDDL_MUTEX_PAIRS_FOR_EACH(mutex, f1, f2){
            if (!pddlMutexPairsIsMutex(&m2, f1, f2))
                fprintf(stderr, "%d(%s) %d(%s)\n",
                        f1, strips->fact.fact[f1]->name,
                        f2, strips->fact.fact[f2]->name);
        }
        */
    }
    pddlMutexPairsFree(&m2);
}

TEST(h3, strips_noce)
{
    BOR_ISET(unreachable_fact);
    BOR_ISET(unreachable_op);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &C.strips);

    //borErrInfoEnable(&err, stdout);
    int ret = pddlH3(&C.strips, &mutex, &unreachable_fact, &unreachable_op,
                     10., 0, &C.err);
    assert(ret == 0);
    dominance(&C.strips, &mutex, &unreachable_fact, &unreachable_op, &C.err);

    if (mutex.num_mutex_pairs > 0)
        fprintf(stdout, "Mutex pairs: %lu\n",
                (unsigned long)mutex.num_mutex_pairs);

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

#if 0
    if (strcmp("petri_net_alignment18_p01", outfn) != 0
            && strcmp("petri_net_alignment18_p20", outfn) != 0
            && strcmp("airport04_p20", outfn) != 0
            && strcmp("tidybot11_p20", outfn) != 0
            && strcmp("tidybot14_p10", outfn) != 0
            && strcmp("tidybot14_p20", outfn) != 0){
        pddl_mgroups_t mgs;
        pddlMGroupsInitEmpty(&mgs);
        pddlMutexPairsInferMutexGroups(&mutex, &mgs, &err);
        pddlMGroupsSortUniq(&mgs);
        /*
        sprintf(fn, "reg/tmp.TSH3.testH3_%s.mutex_groups.out", outfn);
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
#endif

    pddlMutexPairsFree(&mutex);
    borISetFree(&unreachable_fact);
    borISetFree(&unreachable_op);
}
