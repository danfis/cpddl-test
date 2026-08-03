#include "test.h"
#include "context.h"

TEST(pred_func_props, pddl)
{
    pddl_pred_func_props_t fp;
    pddlPredFuncPropsInit(&fp, &C.pddl);
    for (int pi = 0; pi < fp.pred_size; ++pi){
        const pddl_pred_prop_t *p = fp.pred_prop + pi;
        printf("(%s)\n", C.pddl.pred.pred[pi].name);
        printf("  in-init: %d\n", p->in_init);
        printf("  in-pre: %d\n", p->in_pre);
        printf("  in-pre-neg: %d\n", p->in_pre_neg);
        printf("  in-goal: %d\n", p->in_goal);
        printf("  in-goal-neg: %d\n", p->in_goal_neg);
        printf("  in-eff: %d\n", p->in_eff);
    }
    for (int f = 0; f < fp.func_size; ++f){
        const pddl_func_prop_t *p = fp.func_prop + f;
        printf("(%s)\n", C.pddl.func.pred[f].name);
        printf("  in-init: %d\n", p->in_init);
        printf("  init-all-nonneg: %d\n", p->init_all_nonneg);
        printf("  init-all-nonpos: %d\n", p->init_all_nonpos);
        printf("  init-all-int: %d\n", p->init_all_int);
        printf("  in-pre: %d\n", p->in_pre);
        printf("  in-goal: %d\n", p->in_goal);
        printf("  in-metric: %d\n", p->in_metric);
        printf("  is-metric-alone: %d\n", p->is_metric_alone);
        printf("  on-num-op-rhs: %d\n", p->on_num_op_rhs);
        printf("  in-eff: %d\n", p->in_eff);
        printf("  in-eff-by-assign-or-scale: %d\n",
               p->in_eff_by_assign_or_scale);
        printf("  in-eff-in-cond-eff: %d\n", p->in_eff_in_cond_eff);
        printf("  write-delta-nonneg: %d\n", p->write_delta_nonneg);
        printf("  write-delta-nonneg-int: %d\n", p->write_delta_nonneg_int);
        printf("  rhs-funcs:");
        PDDL_ISET_FOR_EACH(&p->rhs_funcs, g)
            printf(" (%s)", C.pddl.func.pred[g].name);
        printf("\n");
    }
    pddlPredFuncPropsFree(&fp);
}
