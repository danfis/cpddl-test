#include "test.h"
#include "context.h"
#include <assert.h>

TEST(pddl_props, pddl)
{
    pddl_props_t props;
    pddlPropsInit(&props, &C.pddl);
    for (int pi = 0; pi < props.pred_size; ++pi){
        const pddl_pred_prop_t *p = props.pred_prop + pi;
        printf("(%s)\n", C.pddl.pred.pred[pi].name);
        printf("  in-init: %d\n", p->in_init);
        printf("  in-pre: %d\n", p->in_pre);
        printf("  in-pre-neg: %d\n", p->in_pre_neg);
        printf("  in-goal: %d\n", p->in_goal);
        printf("  in-goal-neg: %d\n", p->in_goal_neg);
        printf("  in-eff: %d\n", p->in_eff);
    }
    for (int f = 0; f < props.func_size; ++f){
        const pddl_func_prop_t *p = props.func_prop + f;
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
    printf("global\n");
    printf("  has-cond-eff: %d\n", props.has_cond_eff);
    printf("  has-num-cmp: %d\n", props.has_num_cmp);
    printf("  has-eq-pred: %d\n", props.has_eq_pred);
    printf("  has-non-static-neg-cond: %d\n", props.has_non_static_neg_cond);
    if (props.action_cost_func >= 0){
        printf("  action-cost-func: (%s)\n",
               C.pddl.func.pred[props.action_cost_func].name);
    }else{
        printf("  action-cost-func: -1\n");
    }
    printf("  has-int-action-cost-func: %d\n", props.has_int_action_cost_func);
    printf("  is-unit-cost: %d\n", props.is_unit_cost);
    printf("  is-numeric: %d\n", props.is_numeric);

    // The parent test normalizes the task and leaves C.pddl.props valid,
    // so the query functions must agree with the freshly computed props
    assert(props.has_cond_eff == pddlHasCondEff(&C.pddl));
    assert(props.has_eq_pred == pddlHasEqPred(&C.pddl));
    assert(props.has_non_static_neg_cond
            == pddlHasNonStaticNegativeConditions(&C.pddl));
    assert(props.is_numeric == pddlIsNumeric(&C.pddl));
    if (props.has_int_action_cost_func)
        assert(pddlIsMetricExpressibleAsNonNegIntActionCosts(&C.pddl, NULL));
    pddlPropsFree(&props);
}
