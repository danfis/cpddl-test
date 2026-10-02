/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the plan module (pddl/plan.h) on small hand-built FDR tasks.
 *
 * All tests are TEST_ONCE (not per task).
 * Run with:  cd tests && make && ./test -T _ -s plan_
 */

#include "pddl/plan.h"
#include "pddl/alloc.h"
#include "pddl/fdr_app_op.h"
#include "pddl/fdr_state_space.h"
#include "test.h"
#include <assert.h>
#include <string.h>

/** Terminator of (var, val) lists passed to taskAddOp() */
#define END -1

/** Maximum number of variables of the test tasks */
#define MAX_VARS 4

/** Hand-built FDR task together with the structures needed for plan
 *  reconstruction */
struct task {
    pddl_err_t err;
    pddl_fdr_t fdr;
    pddl_fdr_app_op_t app_op;
    pddl_fdr_state_space_t space;
    pddl_fdr_state_space_node_t node;
};
typedef struct task task_t;

/** Initializes the task with VAR_SIZE variables each having VAL_SIZE
 *  values; the initial state assigns 0 to all variables. */
static void taskInit(task_t *t, int var_size, int val_size)
{
    assert(var_size <= MAX_VARS);
    memset(t, 0, sizeof(*t));
    for (int var = 0; var < var_size; ++var){
        pddl_fdr_var_t *v = pddlFDRVarsAdd(&t->fdr.var, val_size);
        assert(v->var_id == var);
    }
    t->fdr.init = PDDL_ZALLOC_ARR(int, var_size);
    pddlFDROpsInit(&t->fdr.op);
    pddlFDRPartStateInit(&t->fdr.goal);
}

/** Sets the (var, val) pairs from the END-terminated list VV in PS */
static void setPartState(pddl_fdr_part_state_t *ps, const int *vv)
{
    for (int i = 0; vv[i] != END; i += 2)
        pddlFDRPartStateSet(ps, vv[i], vv[i + 1]);
}

/** Adds a new operator and returns it; PRE and EFF are END-terminated
 *  lists of (var, val) pairs. */
static pddl_fdr_op_t *taskAddOp(task_t *t,
                                const char *name,
                                int cost,
                                const int *pre,
                                const int *eff)
{
    pddl_fdr_op_t *op = pddlFDROpNewEmpty();
    op->name = PDDL_STRDUP(name);
    op->cost = cost;
    setPartState(&op->pre, pre);
    setPartState(&op->eff, eff);
    pddlFDROpsAddSteal(&t->fdr.op, op);
    return op;
}

/** Adds a conditional effect to OP; PRE and EFF are END-terminated lists
 *  of (var, val) pairs. */
static void opAddCondEff(pddl_fdr_op_t *op, const int *pre, const int *eff)
{
    pddl_fdr_op_cond_eff_t *ce = pddlFDROpAddEmptyCondEff(op);
    setPartState(&ce->pre, pre);
    setPartState(&ce->eff, eff);
}

/** Creates the applicable-operator finder and the state space; must be
 *  called after all operators are added. */
static void taskFinalize(task_t *t)
{
    pddlFDRAppOpInit(&t->app_op, &t->fdr.var, &t->fdr.op, &t->fdr.goal);
    pddlFDRStateSpaceInit(&t->space, &t->fdr.var, &t->err);
    pddlFDRStateSpaceNodeInit(&t->node, &t->space);
}

static void taskFree(task_t *t)
{
    pddlFDRStateSpaceNodeFree(&t->node);
    pddlFDRStateSpaceFree(&t->space);
    pddlFDRAppOpFree(&t->app_op);
    pddlFDRFree(&t->fdr);
}

/** Inserts STATE into the state space with the parent PARENT_ID and the
 *  g-value G, and returns its ID. */
static pddl_state_id_t taskAddState(task_t *t,
                                    const int *state,
                                    pddl_state_id_t parent_id,
                                    int g)
{
    pddl_bool_t is_new;
    pddl_state_id_t id = pddlFDRStateSpaceInsert(&t->space, state, &is_new);
    assert(is_new);
    pddlFDRStateSpaceGetNoState(&t->space, id, &t->node);
    t->node.parent_id = parent_id;
    pddlNumSetInt(&t->node.g_value, g);
    pddlFDRStateSpaceSet(&t->space, &t->node);
    return id;
}

/** Asserts that PLAN consists of the operators OPS (of size SIZE) */
static void assertPlanOps(const pddl_plan_t *plan, const int *ops, int size)
{
    assert(plan->length == size);
    assert(pddlIArrSize(&plan->op) == size);
    for (int i = 0; i < size; ++i)
        assert(pddlIArrGet(&plan->op, i) == ops[i]);
}

/** Builds the task used by plan_backtrack, plan_copy and plan_print:
 *  a path s0 -> s1 -> s2 -> s3 plus a state s4 off the path. Returns the
 *  ID of the goal state s3. */
static pddl_state_id_t buildPathTask(task_t *t)
{
    taskInit(t, 2, 3);
    taskAddOp(t, "x0-x1", 2, (int[]){ 0, 0, END }, (int[]){ 0, 1, END });
    taskAddOp(t, "y0-y1", 3, (int[]){ 1, 0, END }, (int[]){ 1, 1, END });
    taskAddOp(t, "x1-x2", 1, (int[]){ 0, 1, END }, (int[]){ 0, 2, END });
    taskAddOp(t, "y0-y2", 1, (int[]){ 1, 0, END }, (int[]){ 1, 2, END });
    taskFinalize(t);

    pddl_state_id_t s0 = taskAddState(t, (int[]){ 0, 0 }, PDDL_NO_STATE_ID, 0);
    pddl_state_id_t s1 = taskAddState(t, (int[]){ 1, 0 }, s0, 2);
    pddl_state_id_t s4 = taskAddState(t, (int[]){ 0, 2 }, s0, 1);
    pddl_state_id_t s2 = taskAddState(t, (int[]){ 1, 1 }, s1, 5);
    pddl_state_id_t s3 = taskAddState(t, (int[]){ 2, 1 }, s2, 6);
    assert(s0 == 0 && s1 == 1 && s4 == 2 && s2 == 3 && s3 == 4);
    return s3;
}

/*
 * Backtracking along a path of states yields the right states, operators,
 * length and cost; reloading into the same plan overwrites it.
 */
TEST_ONCE(plan_backtrack)
{
    task_t t;
    pddl_state_id_t goal = buildPathTask(&t);

    pddl_plan_t plan;
    pddlPlanInit(&plan);
    pddlPlanLoadBacktrack(&plan, goal, &t.space, &t.app_op);

    assert(plan.state_size == 4);
    assert(plan.state[0] == 0);
    assert(plan.state[1] == 1);
    assert(plan.state[2] == 3);
    assert(plan.state[3] == 4);
    assertPlanOps(&plan, (int[]){ 0, 1, 2 }, 3);
    assert(plan.cost == 6);

    // Loading again into the same plan overwrites the previous content
    pddlPlanLoadBacktrack(&plan, 1, &t.space, &t.app_op);
    assert(plan.state_size == 2);
    assert(plan.state[0] == 0);
    assert(plan.state[1] == 1);
    assertPlanOps(&plan, (int[]){ 0 }, 1);
    assert(plan.cost == 2);

    pddlPlanFree(&plan);
    taskFree(&t);
}

/*
 * Backtracking chooses the cheapest applicable operator leading to the
 * next state (the first one on ties), ignoring the stored g-values.
 */
TEST_ONCE(plan_backtrack_cheapest)
{
    task_t t;
    taskInit(&t, 2, 3);
    // Operators leading from (0, 0) to (1, 0)
    taskAddOp(&t, "expensive", 5, (int[]){ 0, 0, END }, (int[]){ 0, 1, END });
    taskAddOp(&t, "cheap", 1, (int[]){ 0, 0, END }, (int[]){ 0, 1, END });
    taskAddOp(&t, "no-pre", 3, (int[]){ END }, (int[]){ 0, 1, END });
    // Cheapest applicable, but leads to a different state
    taskAddOp(&t, "other-eff", 0, (int[]){ 0, 0, END }, (int[]){ 0, 2, END });
    // Cheapest with the right effect, but not applicable
    taskAddOp(&t, "not-app", 0, (int[]){ 1, 1, END }, (int[]){ 0, 1, END });
    // Operators leading from (1, 0) to (2, 0) with the same cost
    taskAddOp(&t, "tie-a", 2, (int[]){ 0, 1, END }, (int[]){ 0, 2, END });
    taskAddOp(&t, "tie-b", 2, (int[]){ 0, 1, END }, (int[]){ 0, 2, END });
    taskFinalize(&t);

    // The g-values are deliberately inconsistent with the operator costs
    // as they must not be used for choosing operators
    pddl_state_id_t s0 = taskAddState(&t, (int[]){ 0, 0 }, PDDL_NO_STATE_ID, 0);
    pddl_state_id_t s1 = taskAddState(&t, (int[]){ 1, 0 }, s0, 5);
    pddl_state_id_t s2 = taskAddState(&t, (int[]){ 2, 0 }, s1, 100);

    pddl_plan_t plan;
    pddlPlanInit(&plan);
    pddlPlanLoadBacktrack(&plan, s2, &t.space, &t.app_op);

    assert(plan.state_size == 3);
    // "cheap" and then "tie-a" (the first of the equally cheap operators)
    assertPlanOps(&plan, (int[]){ 1, 5 }, 2);
    assert(plan.cost == 3);

    pddlPlanFree(&plan);
    taskFree(&t);
}

/*
 * The plan cost is the sum of the costs of the plan's operators, not the
 * g-value stored for the goal state.
 */
TEST_ONCE(plan_backtrack_cost_from_ops)
{
    task_t t;
    taskInit(&t, 2, 2);
    taskAddOp(&t, "x0-x1", 3, (int[]){ 0, 0, END }, (int[]){ 0, 1, END });
    taskAddOp(&t, "y0-y1", 4, (int[]){ 1, 0, END }, (int[]){ 1, 1, END });
    taskFinalize(&t);

    // Only one operator leads to each state, and the goal g-value differs
    // from the sum of their costs (3 + 4)
    pddl_state_id_t s0 = taskAddState(&t, (int[]){ 0, 0 }, PDDL_NO_STATE_ID, 0);
    pddl_state_id_t s1 = taskAddState(&t, (int[]){ 1, 0 }, s0, 1);
    pddl_state_id_t s2 = taskAddState(&t, (int[]){ 1, 1 }, s1, 42);

    pddl_plan_t plan;
    pddlPlanInit(&plan);
    pddlPlanLoadBacktrack(&plan, s2, &t.space, &t.app_op);
    assertPlanOps(&plan, (int[]){ 0, 1 }, 2);
    assert(plan.cost == 7);

    // The same with a negative goal g-value
    pddlFDRStateSpaceGetNoState(&t.space, s2, &t.node);
    pddlNumSetInt(&t.node.g_value, -1);
    pddlFDRStateSpaceSet(&t.space, &t.node);
    pddlPlanLoadBacktrack(&plan, s2, &t.space, &t.app_op);
    assertPlanOps(&plan, (int[]){ 0, 1 }, 2);
    assert(plan.cost == 7);

    pddlPlanFree(&plan);
    taskFree(&t);
}

/*
 * Backtracking from the initial state gives an empty plan.
 */
TEST_ONCE(plan_backtrack_init_goal)
{
    task_t t;
    taskInit(&t, 2, 2);
    taskAddOp(&t, "x0-x1", 1, (int[]){ 0, 0, END }, (int[]){ 0, 1, END });
    taskFinalize(&t);

    pddl_state_id_t s0 = taskAddState(&t, (int[]){ 0, 0 }, PDDL_NO_STATE_ID, 0);

    pddl_plan_t plan;
    pddlPlanInit(&plan);
    pddlPlanLoadBacktrack(&plan, s0, &t.space, &t.app_op);

    assert(plan.state_size == 1);
    assert(plan.state[0] == s0);
    assertPlanOps(&plan, NULL, 0);
    assert(plan.cost == 0);

    pddlPlanFree(&plan);
    taskFree(&t);
}

/*
 * Backtracking takes conditional effects into account when matching
 * operators to the next state.
 */
TEST_ONCE(plan_backtrack_cond_eff)
{
    task_t t;
    taskInit(&t, 2, 2);
    // Leads to (1, 0) instead of (1, 1)
    taskAddOp(&t, "plain", 0, (int[]){ 0, 0, END }, (int[]){ 0, 1, END });
    // Conditional effect is not triggered, leads to (1, 0)
    pddl_fdr_op_t *op;
    op = taskAddOp(&t, "ce-inactive", 0, (int[]){ END }, (int[]){ 0, 1, END });
    opAddCondEff(op, (int[]){ 1, 1, END }, (int[]){ 1, 1, END });
    // Conditional effect is triggered, leads to (1, 1)
    op = taskAddOp(&t, "ce-active", 4, (int[]){ 0, 0, END }, (int[]){ 0, 1, END });
    opAddCondEff(op, (int[]){ 0, 0, END }, (int[]){ 1, 1, END });
    taskFinalize(&t);

    pddl_state_id_t s0 = taskAddState(&t, (int[]){ 0, 0 }, PDDL_NO_STATE_ID, 0);
    pddl_state_id_t s1 = taskAddState(&t, (int[]){ 1, 1 }, s0, 4);

    pddl_plan_t plan;
    pddlPlanInit(&plan);
    pddlPlanLoadBacktrack(&plan, s1, &t.space, &t.app_op);

    assert(plan.state_size == 2);
    assertPlanOps(&plan, (int[]){ 2 }, 1);
    assert(plan.cost == 4);

    pddlPlanFree(&plan);
    taskFree(&t);
}

/*
 * pddlPlanCopy() creates a deep copy independent of the source.
 */
TEST_ONCE(plan_copy)
{
    task_t t;
    pddl_state_id_t goal = buildPathTask(&t);

    pddl_plan_t plan;
    pddlPlanInit(&plan);
    pddlPlanLoadBacktrack(&plan, goal, &t.space, &t.app_op);

    pddl_plan_t copy;
    pddlPlanCopy(&copy, &plan);
    assert(copy.state != plan.state);
    assert(copy.state_size == plan.state_size);
    for (int i = 0; i < plan.state_size; ++i)
        assert(copy.state[i] == plan.state[i]);
    assert(copy.op.arr != plan.op.arr);
    assert(copy.cost == plan.cost);
    assertPlanOps(&copy, (int[]){ 0, 1, 2 }, 3);

    // The copy is independent of the source
    pddlPlanFree(&plan);
    assert(copy.state[3] == goal);
    assertPlanOps(&copy, (int[]){ 0, 1, 2 }, 3);

    pddlPlanFree(&copy);
    taskFree(&t);
}

/*
 * pddlPlanUpdateAuxiliaryOps() removes auxiliary operators, recomputes
 * the cost and length, and drops the states.
 */
TEST_ONCE(plan_update_aux_ops)
{
    task_t t;
    pddl_state_id_t goal = buildPathTask(&t);
    t.fdr.op.op[1]->is_aux_remove_from_plan = pddl_true;

    pddl_plan_t plan;
    pddlPlanInit(&plan);
    pddlPlanLoadBacktrack(&plan, goal, &t.space, &t.app_op);
    assertPlanOps(&plan, (int[]){ 0, 1, 2 }, 3);
    assert(plan.cost == 6);

    pddlPlanUpdateAuxiliaryOps(&plan, &t.fdr);
    assert(plan.state == NULL);
    assert(plan.state_size == 0);
    assert(plan.state_alloc == 0);
    assertPlanOps(&plan, (int[]){ 0, 2 }, 2);
    assert(plan.cost == 3);

    pddlPlanFree(&plan);
    taskFree(&t);
}

/*
 * pddlPlanPrint() writes the expected text.
 */
TEST_ONCE(plan_print)
{
    task_t t;
    pddl_state_id_t goal = buildPathTask(&t);

    pddl_plan_t plan;
    pddlPlanInit(&plan);
    pddlPlanLoadBacktrack(&plan, goal, &t.space, &t.app_op);

    // Print into a temporary file and compare with the expected output
    FILE *fout = tmpfile();
    assert(fout != NULL);
    pddlPlanPrint(&plan, &t.fdr.op, fout);
    rewind(fout);
    char buf[256];
    size_t size = fread(buf, 1, sizeof(buf) - 1, fout);
    buf[size] = '\0';
    fclose(fout);
    assert(strcmp(buf, ";; Cost: 6\n"
                       ";; Length: 3\n"
                       "(x0-x1) ;; cost: 2\n"
                       "(y0-y1) ;; cost: 3\n"
                       "(x1-x2) ;; cost: 1\n") == 0);

    pddlPlanFree(&plan);
    taskFree(&t);
}
