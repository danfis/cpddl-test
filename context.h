#ifndef __TEST_CONTEXT_H__
#define __TEST_CONTEXT_H__

#include <pddl/pddl.h>

struct context {
    bor_err_t err;
    pddl_files_t files;
    pddl_t pddl;
    pddl_lifted_mgroups_t lmg;
    pddl_strips_t strips;
    pddl_mgroups_t mg;
    pddl_strips_sym_t strips_sym;
    pddl_mutex_pairs_t mutex;
};
typedef struct context context_t;

extern context_t C;

#endif /* __TEST_CONTEXT_H__ */
