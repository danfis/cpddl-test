#include "test.h"
#include "context.h"
#include <assert.h>

TEST(mutex_pair_copy_fdr, fdr)
{
    pddl_mutex_pairs_t fdr_mutex;
    pddlFDRMutexPairsInitCopy(&fdr_mutex, &C.mutex, &C.fdr);

    for (int f1 = 0; f1 < fdr_mutex.fact_size; ++f1){
        for (int f2 = f1 + 1; f2 < fdr_mutex.fact_size; ++f2){
            if (pddlMutexPairsIsMutex(&fdr_mutex, f1, f2)){
                /*
                printf("%d: %s | %d | %d %d\n", f1,
                       C.fdr.var.global_id_to_val[f1]->name,
                       C.fdr.var.global_id_to_val[f1]->strips_id,
                       C.fdr.var.global_id_to_val[f1]->var_id,
                       C.fdr.var.global_id_to_val[f1]->val_id);
                printf("%d: %s | %d | %d %d\n", f2, C.fdr.var.global_id_to_val[f2]->name,
                       C.fdr.var.global_id_to_val[f2]->strips_id,
                       C.fdr.var.global_id_to_val[f2]->var_id,
                       C.fdr.var.global_id_to_val[f2]->val_id);
                fflush(stdout);
                */
                assert(C.fdr.var.global_id_to_val[f1]->strips_id >= 0);
                assert(C.fdr.var.global_id_to_val[f2]->strips_id >= 0);
                assert(pddlMutexPairsIsMutex(&C.mutex,
                                             C.fdr.var.global_id_to_val[f1]->strips_id,
                                             C.fdr.var.global_id_to_val[f2]->strips_id));

            }else{
                if (C.fdr.var.global_id_to_val[f1]->strips_id >= 0
                        && C.fdr.var.global_id_to_val[f2]->strips_id >= 0){
                    assert(!pddlMutexPairsIsMutex(&C.mutex,
                                                  C.fdr.var.global_id_to_val[f1]->strips_id,
                                                  C.fdr.var.global_id_to_val[f2]->strips_id));
                }
            }
        }
    }

    for (int f1 = 0; f1 < C.mutex.fact_size; ++f1){
        for (int f2 = f1 + 1; f2 < C.mutex.fact_size; ++f2){
            PDDL_ISET_FOR_EACH(&C.fdr.var.strips_id_to_val[f1], x1){
                PDDL_ISET_FOR_EACH(&C.fdr.var.strips_id_to_val[f2], x2){
                    if (pddlMutexPairsIsMutex(&C.mutex, f1, f2)){
                        assert(pddlMutexPairsIsMutex(&fdr_mutex, x1, x2));

                    }else{
                        assert(!pddlMutexPairsIsMutex(&fdr_mutex, x1, x2));
                    }
                }
            }
        }
    }
    pddlMutexPairsFree(&fdr_mutex);
}
