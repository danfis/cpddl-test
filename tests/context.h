#ifndef __TEST_CONTEXT_H__
#define __TEST_CONTEXT_H__

#include <pddl/pddl.h>

struct context {
    pddl_err_t err;
    pddl_files_t files;
    int optimal_cost;

    pddl_t pddl;
    int pddl_set;

    pddl_lifted_mgroups_t lmg;
    int lmg_set;
    pddl_lifted_mgroups_t lmg_fd;
    int lmg_fd_set;
    pddl_lifted_mgroups_t lmg_mono;
    int lmg_mono_set;

    pddl_strips_t strips;
    int strips_set;
    pddl_mgroups_t mg;
    int mg_set;
    pddl_mutex_pairs_t mutex;
    int mutex_set;

    pddl_fdr_t fdr;
    int fdr_set;
    pddl_fdr_app_op_t fdr_app_op;
    int fdr_app_op_set;
};
typedef struct context context_t;

extern context_t C;

int validateLiftedPlan(const pddl_lifted_plan_t *plan);
int validateGroundPlan(const pddl_fdr_t *fdr, const pddl_plan_t *plan);

#endif /* __TEST_CONTEXT_H__ */
