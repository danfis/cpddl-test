#include "test.h"
#include "context.h"

TEST(func_props, pddl)
{
    pddl_func_props_t fp;
    pddlFuncPropsInit(&fp, &C.pddl);
    for (int f = 0; f < fp.func_size; ++f){
        const pddl_func_prop_t *p = fp.prop + f;
        printf("(%s)\n", C.pddl.func.pred[f].name);
        printf("  init-count: %d\n", p->init_count);
        printf("  init-all-nonneg: %d\n", p->init_all_nonneg);
        printf("  init-all-nonpos: %d\n", p->init_all_nonpos);
        printf("  init-all-int: %d\n", p->init_all_int);
        printf("  in-pre: %d\n", p->in_pre);
        printf("  in-goal: %d\n", p->in_goal);
        printf("  in-metric: %d\n", p->in_metric);
        printf("  is-metric-alone: %d\n", p->is_metric_alone);
        printf("  on-num-op-rhs: %d\n", p->on_num_op_rhs);
        printf("  written: %d\n", p->written);
        printf("  written-by-assign-or-scale: %d\n",
               p->written_by_assign_or_scale);
        printf("  written-in-cond-eff: %d\n", p->written_in_cond_eff);
        printf("  write-size: %d\n", p->write_size);
        printf("  write-delta-nonneg: %d\n", p->write_delta_nonneg);
        printf("  write-delta-nonneg-int: %d\n", p->write_delta_nonneg_int);
        printf("  rhs-funcs:");
        PDDL_ISET_FOR_EACH(&p->rhs_funcs, g)
            printf(" (%s)", C.pddl.func.pred[g].name);
        printf("\n");
    }
    pddlFuncPropsFree(&fp);
}
