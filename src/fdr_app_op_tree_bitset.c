/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the fdr_app_op_tree_bitset module
 * (pddl/fdr_app_op_tree_bitset.h).
 *
 * The per-task tests compare pddlFDRAppOpTreeBitsetFind() with brute force
 * and with pddlFDRAppOpFind() (C.fdr_app_op). The hand-built test is
 * TEST_ONCE.
 * Run with:  cd tests && make && ./test -Q -T _ -s fdr_app_op_tree_bitset
 */

#include "test.h"
#include "context.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

#define KIND_MASK 0xC0000000u
#define KIND_INTERNAL 0x00000000u
#define KIND_LEAF 0x80000000u
#define KIND_BITSET 0xC0000000u
#define PAYLOAD_MASK 0x3FFFFFFFu

/** Counters and state of appOpTreeBitsetCheck() */
struct check {
    const pddl_fdr_app_op_tree_bitset_t *tree;
    const pddl_fdr_vars_t *vars;
    /** Offset of the leaf of each operator, or -1 */
    int *leaf_of;
    /** Number of internal nodes per variable */
    int *var_nodes;
    /** Next expected start of words of a bitset leaf in .bits[] */
    int next_bits;
    int num_internal;
    int num_leaves;
    int num_bitset_leaves;
};
typedef struct check check_t;

/** Checks the operators op_id[BEGIN:BEGIN + LEN) of the leaf at the offset
 *  N: they are sorted and not in any other leaf */
static void checkLeafOps(check_t *c, int n, int begin, int len)
{
    const pddl_fdr_app_op_tree_bitset_t *tree = c->tree;
    assert(len > 0);
    assert(begin >= 0 && begin + len <= tree->ops->op_size);
    for (int i = 0; i < len; ++i){
        int op_id = tree->op_id[begin + i];
        assert(i == 0 || tree->op_id[begin + i - 1] < op_id);
        assert(c->leaf_of[op_id] == -1);
        c->leaf_of[op_id] = n;
    }
}

/** Checks the bitset leaf at the offset N and returns the number of its
 *  words in .node[] */
static int checkBitsetLeaf(check_t *c, int n)
{
    const pddl_fdr_app_op_tree_bitset_t *tree = c->tree;
    const uint32_t *node = tree->node + n;
    int len = node[0] & PAYLOAD_MASK;
    int begin = node[1];
    int var_size = node[2];
    int bits_begin = node[3];
    int block_size = node[4];
    assert(n + 5 + 2 * var_size <= tree->node_size);
    assert(len <= tree->max_bitset_leaf_ops);
    assert(block_size <= tree->max_bitset_leaf_vals);
    assert(var_size > 0);
    checkLeafOps(c, n, begin, len);

    // Variables are distinct and their values are laid out consecutively
    int off = 0;
    for (int i = 0; i < var_size; ++i){
        int var = node[5 + 2 * i];
        assert(var >= 0 && var < c->vars->var_size);
        for (int i2 = 0; i2 < i; ++i2)
            assert((int)node[5 + 2 * i2] != var);
        assert((int)node[5 + 2 * i + 1] == off);
        off += c->vars->var[var].val_size;
    }
    assert(off == block_size);

    // Words of the leaves are stored consecutively in preorder
    int word_size = (len + 63) / 64;
    assert(bits_begin == c->next_bits);
    c->next_bits += word_size * block_size;
    assert(c->next_bits <= tree->bits_size);

    // Every bit including the padding ones
    for (int k = 0; k < word_size; ++k){
        const uint64_t *block = tree->bits + bits_begin + k * block_size;
        for (int i = 0; i < var_size; ++i){
            int var = node[5 + 2 * i];
            for (int x = 0; x < c->vars->var[var].val_size; ++x){
                uint64_t word = block[node[5 + 2 * i + 1] + x];
                for (int bit = 0; bit < 64; ++bit){
                    int j = 64 * k + bit;
                    int exp = 0;
                    if (j < len){
                        int op_id = tree->op_id[begin + j];
                        const pddl_fdr_op_t *op = tree->ops->op[op_id];
                        int val = pddlFDRPartStateGet(&op->pre, var);
                        exp = (val == -1 || val == x);
                    }
                    assert(((word >> bit) & 1u) == (uint64_t)exp);
                }
            }
        }
    }

    ++c->num_bitset_leaves;
    return 5 + 2 * var_size;
}

/** Traverses the tree from the offset N, checks the structure of the
 *  records and returns the number of words spanned by the subtree */
static int checkNode(check_t *c, int n)
{
    const pddl_fdr_app_op_tree_bitset_t *tree = c->tree;
    assert(n >= 0 && n + 1 < tree->node_size);
    const uint32_t *node = tree->node + n;
    uint32_t kind = node[0] & KIND_MASK;
    if (kind == KIND_LEAF){
        checkLeafOps(c, n, node[1], node[0] & PAYLOAD_MASK);
        ++c->num_leaves;
        return 2;

    }else if (kind == KIND_BITSET){
        return checkBitsetLeaf(c, n);
    }

    // Internal node: [var, noval, child[0], ..., child[val_size - 1]]
    assert(kind == KIND_INTERNAL);
    int var = node[0];
    assert(var < c->vars->var_size);
    ++c->var_nodes[var];
    ++c->num_internal;
    int val_size = c->vars->var[var].val_size;
    assert(n + 2 + val_size <= tree->node_size);

    // Children are stored right after the record in preorder, the "no
    // value" child first and then the value children in increasing order,
    // so they tile the rest of the subtree; 0 means "no child"
    int next = n + 2 + val_size;
    int num_children = 0;
    for (int i = 1; i < 2 + val_size; ++i){
        if (node[i] == 0)
            continue;
        assert((int)node[i] == next);
        next += checkNode(c, next);
        ++num_children;
    }
    assert(num_children > 0);
    return next - n;
}

/** Checks the structure of the whole TREE built from OPS over VARS: the
 *  records tile .node[], the words of bitset leaves tile .bits[], every
 *  operator is in exactly one leaf (stored in LEAF_OF[]), and VAR_NODES[]
 *  holds the number of internal nodes per variable */
static void appOpTreeBitsetCheck(const pddl_fdr_app_op_tree_bitset_t *tree,
                                 const pddl_fdr_vars_t *vars,
                                 const pddl_fdr_ops_t *ops,
                                 int *leaf_of,
                                 int *var_nodes,
                                 check_t *c)
{
    for (int i = 0; i < ops->op_size; ++i)
        leaf_of[i] = -1;
    for (int i = 0; i < vars->var_size; ++i)
        var_nodes[i] = 0;
    memset(c, 0, sizeof(*c));
    c->tree = tree;
    c->vars = vars;
    c->leaf_of = leaf_of;
    c->var_nodes = var_nodes;
    if (ops->op_size == 0){
        assert(tree->node_size == 0);
        assert(tree->bits_size == 0);
        return;
    }

    int size = checkNode(c, 0);
    assert(size == tree->node_size);
    assert(c->next_bits == tree->bits_size);
    for (int i = 0; i < ops->op_size; ++i)
        assert(leaf_of[i] >= 0);
    if (tree->max_bitset_leaf_ops <= 0)
        assert(c->num_bitset_leaves == 0);
}

/** Runs appOpTreeBitsetCheck() on TREE built from C.fdr */
static void appOpTreeBitsetCheckC(const pddl_fdr_app_op_tree_bitset_t *tree)
{
    int *leaf_of = PDDL_ALLOC_ARR(int, PDDL_MAX(C.fdr.op.op_size, 1));
    int *var_nodes = PDDL_ALLOC_ARR(int, PDDL_MAX(C.fdr.var.var_size, 1));
    check_t c;
    appOpTreeBitsetCheck(tree, &C.fdr.var, &C.fdr.op, leaf_of, var_nodes,
                         &c);
    PDDL_FREE(leaf_of);
    PDDL_FREE(var_nodes);
}

/** Checks that TREE finds the same operators in STATE as brute force and
 *  C.fdr_app_op, and recursively does the same for all successor states up
 *  to the depth DEPTH */
static void findOps(const pddl_fdr_app_op_tree_bitset_t *tree,
                    const int *state,
                    int depth)
{
    const pddl_fdr_ops_t *ops = &C.fdr.op;

    PDDL_ISET(app);
    int ret = pddlFDRAppOpTreeBitsetFind(tree, state, &app);
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

/** Runs findOps() from the initial state to the depth 2 with the default
 *  configuration */
static void findOpsSearch(void)
{
    pddl_fdr_app_op_tree_bitset_config_t cfg
            = PDDL_FDR_APP_OP_TREE_BITSET_CONFIG_INIT;
    cfg.vars = &C.fdr.var;
    cfg.ops = &C.fdr.op;
    cfg.goal = &C.fdr.goal;
    pddl_fdr_app_op_tree_bitset_t tree;
    pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
    appOpTreeBitsetCheckC(&tree);
    findOps(&tree, C.fdr.init, 2);
    pddlFDRAppOpTreeBitsetFree(&tree);
}

/** Runs findOps() on NUM_SAMPLES random states for several values of
 *  .max_bitset_leaf_ops and .max_bitset_leaf_vals */
static void findOpsRand(int num_samples)
{
    static const int max_ops_vals[][2] = {
        { 0, INT_MAX },
        { 1, INT_MAX },
        { 64, INT_MAX },
        { 200, INT_MAX },
        { INT_MAX, INT_MAX },
        { 64, 8 },
        { INT_MAX, 32 },
    };
    int num_cfgs = sizeof(max_ops_vals) / sizeof(max_ops_vals[0]);
    const pddl_fdr_vars_t *vars = &C.fdr.var;
    int *state = PDDL_ALLOC_ARR(int, vars->var_size);
    for (int ci = 0; ci < num_cfgs; ++ci){
        pddl_fdr_app_op_tree_bitset_config_t cfg
                = PDDL_FDR_APP_OP_TREE_BITSET_CONFIG_INIT;
        cfg.vars = vars;
        cfg.ops = &C.fdr.op;
        cfg.goal = &C.fdr.goal;
        cfg.max_bitset_leaf_ops = max_ops_vals[ci][0];
        cfg.max_bitset_leaf_vals = max_ops_vals[ci][1];
        pddl_fdr_app_op_tree_bitset_t tree;
        pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
        appOpTreeBitsetCheckC(&tree);

        pddl_rand_t rnd;
        pddlRandInitAuto(&rnd);
        for (int sample = 0; sample < num_samples; ++sample){
            for (int var = 0; var < vars->var_size; ++var){
                int val = pddlRand(&rnd, 0, vars->var[var].val_size);
                val = PDDL_MIN(val, vars->var[var].val_size - 1);
                state[var] = val;
            }
            findOps(&tree, state, 0);
        }
        pddlFDRAppOpTreeBitsetFree(&tree);
    }
    PDDL_FREE(state);
}

/*
 * pddl_fdr_app_op_tree_bitset_t with the default configuration has a
 * consistent structure and finds the same applicable operators as brute
 * force and pddl_fdr_app_op_t in all states reachable in two steps from
 * init.
 */
TEST(fdr_app_op_tree_bitset_search, fdr_app_op)
{
    findOpsSearch();
}

/*
 * pddl_fdr_app_op_tree_bitset_t has a consistent structure and finds the
 * same applicable operators as brute force and pddl_fdr_app_op_t in random
 * states, from the plain decision tree (.max_bitset_leaf_ops = 0) up to a
 * single bitset leaf (INT_MAX), also with limited numbers of values in
 * bitset leaves (.max_bitset_leaf_vals).
 */
TEST(fdr_app_op_tree_bitset_rand, fdr_app_op)
{
    findOpsRand(1000);
}

/*
 * Same as fdr_app_op_tree_bitset_search but with essential-first FDR
 * variables.
 */
TEST(fdr_app_op_tree_bitset_search_essential, fdr_app_op_essential)
{
    findOpsSearch();
}

/*
 * Same as fdr_app_op_tree_bitset_rand but with essential-first FDR
 * variables.
 */
TEST(fdr_app_op_tree_bitset_rand_essential, fdr_app_op_essential)
{
    findOpsRand(1000);
}

#define END -1

/** Adds an operator with the precondition given as the END-terminated
 *  array of (var, val) pairs PRE */
static void appOpTreeBitsetAddOp(pddl_fdr_ops_t *ops, const int *pre)
{
    pddl_fdr_op_t *op = pddlFDROpNewEmpty();
    op->name = PDDL_STRDUP("op");
    for (int i = 0; pre[i] != END; i += 2)
        pddlFDRPartStateSet(&op->pre, pre[i], pre[i + 1]);
    pddlFDROpsAddSteal(ops, op);
}

/** Creates an empty task with the variables of the sizes 3, 2, 2, 2 */
static void appOpTreeBitsetTaskInit(pddl_fdr_t *fdr)
{
    memset(fdr, 0, sizeof(*fdr));
    pddlFDRVarsAdd(&fdr->var, 3);
    pddlFDRVarsAdd(&fdr->var, 2);
    pddlFDRVarsAdd(&fdr->var, 2);
    pddlFDRVarsAdd(&fdr->var, 2);
    fdr->init = PDDL_ZALLOC_ARR(int, fdr->var.var_size);
    pddlFDROpsInit(&fdr->op);
    pddlFDRPartStateInit(&fdr->goal);
    pddlFDRPartStateSet(&fdr->goal, 0, 2);
}

/** Compares Find on TREE with brute force in all states of the task FDR
 *  created by appOpTreeBitsetTaskInit() */
static void appOpTreeBitsetCmpBrute(const pddl_fdr_app_op_tree_bitset_t *tree,
                                    const pddl_fdr_t *fdr)
{
    PDDL_ISET(found);
    int state[4];
    for (int s = 0; s < 3 * 2 * 2 * 2; ++s){
        state[0] = s % 3;
        state[1] = (s / 3) % 2;
        state[2] = (s / 6) % 2;
        state[3] = (s / 12) % 2;

        PDDL_ISET(exp);
        for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
            const pddl_fdr_part_state_t *pre = &fdr->op.op[op_id]->pre;
            if (pddlFDRPartStateIsConsistentWithState(pre, state))
                pddlISetAdd(&exp, op_id);
        }

        pddlISetEmpty(&found);
        int ret = pddlFDRAppOpTreeBitsetFind(tree, state, &found);
        assert(ret == pddlISetSize(&exp));
        assert(pddlISetEq(&found, &exp));

        // Find adds to the existing content of the output set
        pddlISetEmpty(&found);
        pddlISetAdd(&found, 1000);
        ret = pddlFDRAppOpTreeBitsetFind(tree, state, &found);
        assert(ret == pddlISetSize(&exp));
        pddlISetAdd(&exp, 1000);
        assert(pddlISetEq(&found, &exp));
        pddlISetFree(&exp);
    }
    pddlISetFree(&found);
}

/** Checks that the subtrees at the offset N of TREE and REF have the same
 *  shape, i.e., they differ only in "no child" being 0 instead of -1 and
 *  in the tags of leaves */
static void appOpTreeBitsetCmpShape(const pddl_fdr_app_op_tree_bitset_t *tree,
                                    const pddl_fdr_app_op_tree_t *ref,
                                    const pddl_fdr_vars_t *vars,
                                    int n)
{
    if (ref->node[n] < 0){
        assert(tree->node[n] == (KIND_LEAF | (uint32_t)-ref->node[n]));
        assert((int)tree->node[n + 1] == ref->node[n + 1]);
        return;
    }

    int var = ref->node[n];
    assert(tree->node[n] == (uint32_t)var);
    for (int i = 1; i < 2 + vars->var[var].val_size; ++i){
        if (ref->node[n + i] == -1){
            assert(tree->node[n + i] == 0);
        }else{
            assert((int)tree->node[n + i] == ref->node[n + i]);
            appOpTreeBitsetCmpShape(tree, ref, vars, ref->node[n + i]);
        }
    }
}

/*
 * pddl_fdr_app_op_tree_bitset_t on small hand-built tasks: a task without
 * operators yields an empty tree; with .max_bitset_leaf_ops = 0 the tree
 * has the same shape as pddl_fdr_app_op_tree_t; INT_MAX gives a single
 * multi-word bitset leaf; 64 gives all three kinds of nodes; the number of
 * values in bitset leaves is bounded by .max_bitset_leaf_vals; and Find
 * agrees with brute force in all states for various configurations.
 */
TEST_ONCE(fdr_app_op_tree_bitset_handmade)
{
    pddl_fdr_t fdr;
    appOpTreeBitsetTaskInit(&fdr);
    pddl_fdr_app_op_tree_bitset_config_t cfg
            = PDDL_FDR_APP_OP_TREE_BITSET_CONFIG_INIT;
    assert(cfg.max_bitset_leaf_ops == 64);
    assert(cfg.max_bitset_leaf_vals == INT_MAX);
    cfg.vars = &fdr.var;
    cfg.ops = &fdr.op;
    cfg.goal = &fdr.goal;

    // Without operators
    pddl_fdr_app_op_tree_bitset_t tree;
    pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
    assert(tree.node_size == 0 && tree.bits_size == 0);
    PDDL_ISET(found);
    pddlISetAdd(&found, 7);
    int ret = pddlFDRAppOpTreeBitsetFind(&tree, fdr.init, &found);
    assert(ret == 0);
    assert(pddlISetSize(&found) == 1 && pddlISetIn(7, &found));
    pddlISetFree(&found);
    pddlFDRAppOpTreeBitsetFree(&tree);

    // The task of fdr_app_op_tree_handmade
    appOpTreeBitsetAddOp(&fdr.op, (int []){ END });                 // 0
    appOpTreeBitsetAddOp(&fdr.op, (int []){ 0, 1, END });           // 1
    appOpTreeBitsetAddOp(&fdr.op, (int []){ 0, 1, 1, 0, END });     // 2
    appOpTreeBitsetAddOp(&fdr.op, (int []){ 1, 0, END });           // 3
    appOpTreeBitsetAddOp(&fdr.op, (int []){ END });                 // 4
    appOpTreeBitsetAddOp(&fdr.op, (int []){ 1, 0, 0, 1, END });     // 5
    appOpTreeBitsetAddOp(&fdr.op, (int []){ 3, 1, 0, 2, END });     // 6
    appOpTreeBitsetAddOp(&fdr.op, (int []){ 0, 2, END });           // 7

    // Without bitset leaves, the tree has the same shape as
    // pddl_fdr_app_op_tree_t, only "no child" is 0 instead of -1 and leaves
    // are tagged differently
    pddl_fdr_app_op_tree_t ref;
    pddlFDRAppOpTreeInit(&ref, &fdr.var, &fdr.op, &fdr.goal);
    cfg.max_bitset_leaf_ops = 0;
    pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
    assert(tree.node_size == ref.node_size);
    assert(tree.bits_size == 0);
    appOpTreeBitsetCmpShape(&tree, &ref, &fdr.var, 0);
    for (int i = 0; i < fdr.op.op_size; ++i)
        assert(tree.op_id[i] == ref.op_id[i]);
    appOpTreeBitsetCmpBrute(&tree, &fdr);
    pddlFDRAppOpTreeBitsetFree(&tree);
    pddlFDRAppOpTreeFree(&ref);

    // Extend the task to 150 operators: the odd ones from 9, ..., 149 have
    // empty preconditions (so that there are more than 64 of them and they
    // end up in a standard leaf), the even ones from 8, ..., 148 cycle
    // through different partial preconditions with many duplicates
    for (int i = 8; i < 150; ++i){
        int pre[9];
        int ins = 0;
        if (i % 2 == 1){
            pre[ins] = END;
            appOpTreeBitsetAddOp(&fdr.op, pre);
            continue;
        }
        if (i % 4 != 0){
            pre[ins++] = 0;
            pre[ins++] = i % 3;
        }
        if (i % 5 != 0){
            pre[ins++] = 1;
            pre[ins++] = (i / 3) % 2;
        }
        if (i % 2 == 0){
            pre[ins++] = 2;
            pre[ins++] = (i / 2) % 2;
        }
        if (i % 7 != 0){
            pre[ins++] = 3;
            pre[ins++] = (i / 7) % 2;
        }
        pre[ins] = END;
        appOpTreeBitsetAddOp(&fdr.op, pre);
    }
    assert(fdr.op.op_size == 150);

    int leaf_of[150];
    int var_nodes[4];
    check_t c;

    // A single bitset leaf spanning three words
    cfg.max_bitset_leaf_ops = INT_MAX;
    pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
    appOpTreeBitsetCheck(&tree, &fdr.var, &fdr.op, leaf_of, var_nodes, &c);
    assert(c.num_internal == 0);
    assert(c.num_leaves == 0);
    assert(c.num_bitset_leaves == 1);
    assert((tree.node[0] & KIND_MASK) == KIND_BITSET);
    assert((tree.node[0] & PAYLOAD_MASK) == 150);
    assert(tree.node[2] == 4);
    assert(tree.bits_size == 3 * (3 + 2 + 2 + 2));
    appOpTreeBitsetCmpBrute(&tree, &fdr);
    pddlFDRAppOpTreeBitsetFree(&tree);

    // All kinds of nodes
    cfg.max_bitset_leaf_ops = 64;
    pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
    appOpTreeBitsetCheck(&tree, &fdr.var, &fdr.op, leaf_of, var_nodes, &c);
    assert(c.num_internal > 0);
    assert(c.num_leaves > 0);
    assert(c.num_bitset_leaves > 0);
    appOpTreeBitsetCmpBrute(&tree, &fdr);
    pddlFDRAppOpTreeBitsetFree(&tree);

    // Find agrees with brute force for various configurations
    static const int max_ops[] = { 0, 1, 2, 3, 200 };
    for (int mi = 0; mi < (int)(sizeof(max_ops) / sizeof(int)); ++mi){
        cfg.max_bitset_leaf_ops = max_ops[mi];
        pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
        appOpTreeBitsetCheck(&tree, &fdr.var, &fdr.op, leaf_of, var_nodes,
                             &c);
        // Bitset leaves are guaranteed only for 0 (none) and for a value
        // covering all operators (the root)
        if (max_ops[mi] == 0){
            assert(c.num_bitset_leaves == 0);
        }else if (max_ops[mi] >= fdr.op.op_size){
            assert(c.num_bitset_leaves == 1 && c.num_internal == 0);
        }
        appOpTreeBitsetCmpBrute(&tree, &fdr);
        pddlFDRAppOpTreeBitsetFree(&tree);
    }

    // The limit on the number of values in bitset leaves: all 3 + 2 + 2 + 2
    // values fit, one value less splits the root, and no value at all
    // gives the plain decision tree
    cfg.max_bitset_leaf_ops = INT_MAX;
    cfg.max_bitset_leaf_vals = 9;
    pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
    appOpTreeBitsetCheck(&tree, &fdr.var, &fdr.op, leaf_of, var_nodes, &c);
    assert(c.num_bitset_leaves == 1 && c.num_internal == 0);
    appOpTreeBitsetCmpBrute(&tree, &fdr);
    pddlFDRAppOpTreeBitsetFree(&tree);

    static const int max_vals[] = { 8, 6, 4, 3, 2, 1 };
    for (int vi = 0; vi < (int)(sizeof(max_vals) / sizeof(int)); ++vi){
        cfg.max_bitset_leaf_vals = max_vals[vi];
        pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
        appOpTreeBitsetCheck(&tree, &fdr.var, &fdr.op, leaf_of, var_nodes,
                             &c);
        assert(c.num_internal > 0);
        appOpTreeBitsetCmpBrute(&tree, &fdr);
        pddlFDRAppOpTreeBitsetFree(&tree);
    }

    cfg.max_bitset_leaf_vals = 0;
    pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
    appOpTreeBitsetCheck(&tree, &fdr.var, &fdr.op, leaf_of, var_nodes, &c);
    assert(c.num_bitset_leaves == 0);
    appOpTreeBitsetCmpBrute(&tree, &fdr);
    pddlFDRAppOpTreeBitsetFree(&tree);

    pddlFDRFree(&fdr);
}

/*
 * pddlFDRAppOpTreeBitsetInit() panics if the configuration does not set
 * the task.
 */
TEST_PANIC_ONCE(fdr_app_op_tree_bitset_no_task)
{
    pddl_fdr_app_op_tree_bitset_config_t cfg
            = PDDL_FDR_APP_OP_TREE_BITSET_CONFIG_INIT;
    pddl_fdr_app_op_tree_bitset_t tree;
    pddlFDRAppOpTreeBitsetInit(&tree, &cfg);
}
