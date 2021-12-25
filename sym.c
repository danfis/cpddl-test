#include <assert.h>
#include "test.h"
#include "context.h"

TEST_COND(symmetry, strips_pruned, BLISS)
{
    int *fact_used = BOR_CALLOC_ARR(int, C.strips.fact.fact_size);
    int *op_used = BOR_CALLOC_ARR(int, C.strips.op.op_size);
    pddl_strips_sym_t sym;
    pddlStripsSymInitPDG(&sym, &C.strips);

    fprintf(stdout, "Symmetric facts:\n");
    for (int fact_id = 0; fact_id < C.strips.fact.fact_size; ++fact_id){
        if (fact_used[fact_id])
            continue;
        BOR_ISET(set);
        borISetAdd(&set, fact_id);

        pddl_set_iset_t fset;
        pddlSetISetInit(&fset);
        pddlSetISetAdd(&fset, &set);
        pddlStripsSymAllFactSetSymmetries(&sym, &fset);

        BOR_ISET(sym_set);
        const bor_iset_t *s;
        PDDL_SET_ISET_FOR_EACH(&fset, s){
            assert(borISetSize(s) == 1);
            borISetUnion(&sym_set, s);
        }
        if (borISetSize(&sym_set) > 1){
            int fact_id;
            BOR_ISET_FOR_EACH(&sym_set, fact_id){
                fact_used[fact_id] = 1;
                fprintf(stdout, " (%s)", C.strips.fact.fact[fact_id]->name);
            }
            fprintf(stdout, "\n");
        }
        borISetFree(&sym_set);

        pddlSetISetFree(&fset);
        borISetFree(&set);
    }

    fprintf(stdout, "Symmetric ops:\n");
    for (int op_id = 0; op_id < C.strips.op.op_size; ++op_id){
        if (op_used[op_id])
            continue;
        BOR_ISET(set);
        borISetAdd(&set, op_id);

        pddl_set_iset_t fset;
        pddlSetISetInit(&fset);
        pddlSetISetAdd(&fset, &set);
        pddlStripsSymAllOpSetSymmetries(&sym, &fset);

        BOR_ISET(sym_set);
        const bor_iset_t *s;
        PDDL_SET_ISET_FOR_EACH(&fset, s){
            assert(borISetSize(s) == 1);
            borISetUnion(&sym_set, s);
        }
        if (borISetSize(&sym_set) > 1){
            int op_id;
            BOR_ISET_FOR_EACH(&sym_set, op_id){
                op_used[op_id] = 1;
                fprintf(stdout, " (%s)", C.strips.op.op[op_id]->name);
            }
            fprintf(stdout, "\n");
        }
        borISetFree(&sym_set);

        pddlSetISetFree(&fset);
        borISetFree(&set);
    }

    pddlStripsSymFree(&sym);
    if (fact_used != NULL)
        BOR_FREE(fact_used);
    if (op_used != NULL)
        BOR_FREE(op_used);
}
