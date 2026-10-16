/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the fdr_app_op_tree module (pddl/fdr_app_op_tree.h).
 *
 * The per-task tests compare pddlFDRAppOpTreeFind() with brute force and
 * with pddlFDRAppOpFind() (C.fdr_app_op). The hand-built test is TEST_ONCE.
 * Run with:  cd tests && make && ./test -Q -T _ -s fdr_app_op_tree
 */

#include "test.h"
#include "context.h"
#include <assert.h>
#include <string.h>

/** Traverses the tree from the offset N, checks the structure of the
 *  records, stores the offset of the leaf of each operator in LEAF_OF[]
 *  (which must be initialized to -1), counts internal nodes per variable in
 *  VAR_NODES[], and returns the number of ints spanned by the subtree */
static int appOpTreeCheck(const pddl_fdr_app_op_tree_t *tree,
                          const pddl_fdr_vars_t *vars,
                          int n,
                          int *leaf_of,
                          int *var_nodes)
{
    assert(n >= 0 && n + 1 < tree->node_size);
    const int *node = tree->node + n;
    if (node[0] < 0){
        // Leaf: [-len, begin] with sorted operator IDs
        int len = -node[0];
        int begin = node[1];
        assert(begin >= 0 && begin + len <= tree->ops->op_size);
        for (int i = 0; i < len; ++i){
            int op_id = tree->op_id[begin + i];
            assert(i == 0 || tree->op_id[begin + i - 1] < op_id);
            assert(leaf_of[op_id] == -1);
            leaf_of[op_id] = n;
        }
        return 2;
    }

    // Internal node: [var, noval, child[0], ..., child[val_size - 1]]
    int var = node[0];
    assert(var < vars->var_size);
    ++var_nodes[var];
    int val_size = vars->var[var].val_size;
    assert(n + 2 + val_size <= tree->node_size);

    // Children are stored right after the record in preorder, the "no
    // value" child first and then the value children in increasing order,
    // so they tile the rest of the subtree
    int next = n + 2 + val_size;
    int num_children = 0;
    for (int i = 1; i < 2 + val_size; ++i){
        if (node[i] == -1)
            continue;
        assert(node[i] == next);
        next += appOpTreeCheck(tree, vars, next, leaf_of, var_nodes);
        ++num_children;
    }
    assert(num_children > 0);
    return next - n;
}

/** Checks the structure of the whole TREE built from OPS over VARS: every
 *  operator is in exactly one leaf (stored in LEAF_OF[]), the records tile
 *  .node[], and VAR_NODES[] holds the number of internal nodes per
 *  variable */
static void appOpTreeCheckAll(const pddl_fdr_app_op_tree_t *tree,
                              const pddl_fdr_vars_t *vars,
                              const pddl_fdr_ops_t *ops,
                              int *leaf_of,
                              int *var_nodes)
{
    for (int i = 0; i < ops->op_size; ++i)
        leaf_of[i] = -1;
    for (int i = 0; i < vars->var_size; ++i)
        var_nodes[i] = 0;
    if (ops->op_size == 0){
        assert(tree->node_size == 0);
        return;
    }

    int size = appOpTreeCheck(tree, vars, 0, leaf_of, var_nodes);
    assert(size == tree->node_size);
    for (int i = 0; i < ops->op_size; ++i)
        assert(leaf_of[i] >= 0);
}

/** Runs appOpTreeCheckAll() on TREE built from C.fdr */
static void appOpTreeCheckC(const pddl_fdr_app_op_tree_t *tree)
{
    int *leaf_of = PDDL_ALLOC_ARR(int, PDDL_MAX(C.fdr.op.op_size, 1));
    int *var_nodes = PDDL_ALLOC_ARR(int, PDDL_MAX(C.fdr.var.var_size, 1));
    appOpTreeCheckAll(tree, &C.fdr.var, &C.fdr.op, leaf_of, var_nodes);
    PDDL_FREE(leaf_of);
    PDDL_FREE(var_nodes);
}

/** Checks that TREE finds the same operators in STATE as brute force and
 *  C.fdr_app_op, and recursively does the same for all successor states up
 *  to the depth DEPTH */
static void findOps(const pddl_fdr_app_op_tree_t *tree,
                    const int *state,
                    int depth)
{
    const pddl_fdr_ops_t *ops = &C.fdr.op;

    PDDL_ISET(app);
    int ret = pddlFDRAppOpTreeFind(tree, state, &app);
    assert(ret == pddlISetSize(&app));

    PDDL_ISET(app_brute);
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        const pddl_fdr_part_state_t *pre = &ops->op[op_id]->pre;
        if (pddlFDRPartStateIsConsistentWithState(pre, state))
            pddlISetAdd(&app_brute, op_id);
    }
    assert(pddlISetEq(&app, &app_brute));

    PDDL_ISET(app_op);
    pddlFDRAppOpFind(&C.fdr_app_op, state, &app_op);
    assert(pddlISetEq(&app, &app_op));

    if (depth > 0){
        int var_size = C.fdr.var.var_size;
        int *next_state = PDDL_ALLOC_ARR(int, var_size);
        PDDL_ISET_FOR_EACH(&app, op_id){
            memcpy(next_state, state, sizeof(int) * var_size);
            pddlFDRPartStateApplyToState(&ops->op[op_id]->eff, next_state);
            findOps(tree, next_state, depth - 1);
        }
        PDDL_FREE(next_state);
    }

    pddlISetFree(&app_op);
    pddlISetFree(&app_brute);
    pddlISetFree(&app);
}

/** Runs findOps() from the initial state to the depth 2 */
static void findOpsSearch(void)
{
    pddl_fdr_app_op_tree_t tree;
    pddlFDRAppOpTreeInit(&tree, &C.fdr.var, &C.fdr.op, &C.fdr.goal);
    appOpTreeCheckC(&tree);
    findOps(&tree, C.fdr.init, 2);
    pddlFDRAppOpTreeFree(&tree);
}

/** Runs findOps() on NUM_SAMPLES random states */
static void findOpsRand(int num_samples)
{
    const pddl_fdr_vars_t *vars = &C.fdr.var;
    pddl_fdr_app_op_tree_t tree;
    pddlFDRAppOpTreeInit(&tree, vars, &C.fdr.op, &C.fdr.goal);
    appOpTreeCheckC(&tree);

    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);
    int *state = PDDL_ALLOC_ARR(int, vars->var_size);
    for (int sample = 0; sample < num_samples; ++sample){
        for (int var = 0; var < vars->var_size; ++var){
            int val = pddlRand(&rnd, 0, vars->var[var].val_size);
            val = PDDL_MIN(val, vars->var[var].val_size - 1);
            state[var] = val;
        }
        findOps(&tree, state, 0);
    }
    PDDL_FREE(state);
    pddlFDRAppOpTreeFree(&tree);
}

/*
 * pddl_fdr_app_op_tree_t finds the same applicable operators as brute force
 * and pddl_fdr_app_op_t in all states reachable in two steps from init.
 */
TEST(fdr_app_op_tree_search, fdr_app_op)
{
    findOpsSearch();
}

/*
 * pddl_fdr_app_op_tree_t finds the same applicable operators as brute force
 * and pddl_fdr_app_op_t in random states.
 */
TEST(fdr_app_op_tree_rand, fdr_app_op)
{
    findOpsRand(5000);
}

/*
 * Same as fdr_app_op_tree_search but with essential-first FDR variables.
 */
TEST(fdr_app_op_tree_search_essential, fdr_app_op_essential)
{
    findOpsSearch();
}

/*
 * Same as fdr_app_op_tree_rand but with essential-first FDR variables.
 */
TEST(fdr_app_op_tree_rand_essential, fdr_app_op_essential)
{
    findOpsRand(5000);
}

#define END -1

/** Adds an operator with the precondition given as the END-terminated
 *  list of (var, val) pairs PRE */
static void appOpTreeAddOp(pddl_fdr_ops_t *ops, const int *pre)
{
    pddl_fdr_op_t *op = pddlFDROpNewEmpty();
    op->name = PDDL_STRDUP("op");
    for (int i = 0; pre[i] != END; i += 2)
        pddlFDRPartStateSet(&op->pre, pre[i], pre[i + 1]);
    pddlFDROpsAddSteal(ops, op);
}

/*
 * pddl_fdr_app_op_tree_t on a small hand-built task: operators with empty
 * and duplicate preconditions end up in shared leaves sorted by ID, every
 * operator is in exactly one leaf, a variable not used in any precondition
 * gets no node, Find agrees with brute force in all states and adds to a
 * non-empty output set, and a task without operators yields an empty tree.
 */
TEST_ONCE(fdr_app_op_tree_handmade)
{
    pddl_fdr_t fdr;
    memset(&fdr, 0, sizeof(fdr));
    pddlFDRVarsAdd(&fdr.var, 3);
    pddlFDRVarsAdd(&fdr.var, 2);
    pddlFDRVarsAdd(&fdr.var, 2); // not used in any precondition
    pddlFDRVarsAdd(&fdr.var, 2);
    fdr.init = PDDL_ZALLOC_ARR(int, fdr.var.var_size);
    pddlFDROpsInit(&fdr.op);
    pddlFDRPartStateInit(&fdr.goal);
    pddlFDRPartStateSet(&fdr.goal, 0, 2);

    // Without operators
    pddl_fdr_app_op_tree_t tree;
    pddlFDRAppOpTreeInit(&tree, &fdr.var, &fdr.op, &fdr.goal);
    assert(tree.node_size == 0);
    PDDL_ISET(app);
    pddlISetAdd(&app, 7);
    int ret = pddlFDRAppOpTreeFind(&tree, fdr.init, &app);
    assert(ret == 0);
    assert(pddlISetSize(&app) == 1 && pddlISetIn(7, &app));
    pddlFDRAppOpTreeFree(&tree);

    appOpTreeAddOp(&fdr.op, (int []){ END });                       // 0
    appOpTreeAddOp(&fdr.op, (int []){ 0, 1, END });                 // 1
    appOpTreeAddOp(&fdr.op, (int []){ 0, 1, 1, 0, END });           // 2
    appOpTreeAddOp(&fdr.op, (int []){ 1, 0, END });                 // 3
    appOpTreeAddOp(&fdr.op, (int []){ END });                       // 4
    appOpTreeAddOp(&fdr.op, (int []){ 1, 0, 0, 1, END });           // 5
    appOpTreeAddOp(&fdr.op, (int []){ 3, 1, 0, 2, END });           // 6
    appOpTreeAddOp(&fdr.op, (int []){ 0, 2, END });                 // 7
    pddlFDRAppOpTreeInit(&tree, &fdr.var, &fdr.op, &fdr.goal);

    // Leaves correspond to distinct preconditions and hold operator IDs
    // sorted in ascending order
    assert(fdr.op.op_size == 8);
    int leaf_of[8];
    int var_nodes[4];
    appOpTreeCheckAll(&tree, &fdr.var, &fdr.op, leaf_of, var_nodes);
    int num_leaves = 0;
    for (int op_id = 0; op_id < fdr.op.op_size; ++op_id){
        int first = 1;
        for (int i = 0; i < op_id; ++i)
            first &= (leaf_of[i] != leaf_of[op_id]);
        num_leaves += first;
    }
    assert(num_leaves == 6);
    assert(leaf_of[0] == leaf_of[4]);
    assert(leaf_of[2] == leaf_of[5]);
    assert(var_nodes[2] == 0);

    // Compare with brute force in all states
    int state[4];
    for (int s = 0; s < 3 * 2 * 2 * 2; ++s){
        state[0] = s % 3;
        state[1] = (s / 3) % 2;
        state[2] = (s / 6) % 2;
        state[3] = (s / 12) % 2;

        PDDL_ISET(exp);
        for (int op_id = 0; op_id < fdr.op.op_size; ++op_id){
            const pddl_fdr_part_state_t *pre = &fdr.op.op[op_id]->pre;
            if (pddlFDRPartStateIsConsistentWithState(pre, state))
                pddlISetAdd(&exp, op_id);
        }

        pddlISetEmpty(&app);
        ret = pddlFDRAppOpTreeFind(&tree, state, &app);
        assert(ret == pddlISetSize(&exp));
        assert(pddlISetEq(&app, &exp));

        // Find adds to the existing content of the output set
        pddlISetEmpty(&app);
        pddlISetAdd(&app, 100);
        ret = pddlFDRAppOpTreeFind(&tree, state, &app);
        assert(ret == pddlISetSize(&exp));
        pddlISetAdd(&exp, 100);
        assert(pddlISetEq(&app, &exp));
        pddlISetFree(&exp);
    }

    // State 0=1, 1=0, 3=1 (so op 6 is not applicable)
    state[0] = 1;
    state[1] = 0;
    state[2] = 0;
    state[3] = 1;
    pddlISetEmpty(&app);
    ret = pddlFDRAppOpTreeFind(&tree, state, &app);
    assert(ret == 6);
    int expected[] = { 0, 1, 2, 3, 4, 5 };
    for (int i = 0; i < 6; ++i)
        assert(pddlISetGet(&app, i) == expected[i]);

    pddlISetFree(&app);
    pddlFDRAppOpTreeFree(&tree);
    pddlFDRFree(&fdr);
}
