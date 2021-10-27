#ifndef __TEST_CONTEXT_H__
#define __TEST_CONTEXT_H__

#include <pddl/pddl.h>

struct context {
    bor_err_t err;
    pddl_files_t files;
    pddl_t pddl;
    int pddl_set;
    pddl_lifted_mgroups_t lmg;
    int lmg_set;
    pddl_strips_t strips;
    int strips_set;
    pddl_mgroups_t mg;
    int mg_set;
    pddl_strips_sym_t strips_sym;
    int strips_sym_set;
    pddl_mutex_pairs_t mutex;
    int mutex_set;
    bor_iset_t mutex_unreachable_op;
    bor_iset_t mutex_unreachable_fact;
    pddl_mutex_pairs_t mutex3;
    int mutex3_set;
    pddl_mg_strips_t mg_strips;
    int mg_strips_set;
    pddl_trans_systems_t trans_systems;
    int trans_systems_set;
};
typedef struct context context_t;

extern context_t C;

#endif /* __TEST_CONTEXT_H__ */
