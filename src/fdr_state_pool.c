/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the representations of the FDR state pool
 * (pddl/fdr_state_pool.h): the tree compression, the fallbacks to the array
 * representation, and the switch of PDDL_FDR_STATE_POOL_TREE_FFD from the
 * tree to the array of FFD-packed states.
 *
 * All tests except fdr_state_pool_walk are TEST_ONCE (not per task).
 * Run with:  cd tests && make && ./test -T _ -s fdr_state_pool_
 */

#include "pddl/fdr_state_pool.h"
#include "pddl/rand.h"
#include "test.h"
#include "context.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Number of values of the variables of varsInitWords(); 8 such variables
 *  fill one 32-bit word */
#define VAL_SIZE 16
/** Number of variables of varsInitWords() per word */
#define VARS_PER_WORD 8

/* Initializes VARS with VARS_PER_WORD * NUM_WORDS variables of VAL_SIZE
 * values. With the FFD layout, the word k stores the variables
 * VARS_PER_WORD * k, ..., VARS_PER_WORD * (k + 1) - 1 (all variables have
 * the same width, so they are placed in the order of their IDs). */
static void varsInitWords(pddl_fdr_vars_t *vars, int num_words)
{
    memset(vars, 0, sizeof(*vars));
    for (int i = 0; i < VARS_PER_WORD * num_words; ++i)
        pddlFDRVarsAdd(vars, VAL_SIZE);
}

/* Sets all variables of the word K of STATE (see varsInitWords()) to VAL,
 * so that the word is determined by VAL. */
static void setWord(int *state, int k, int val)
{
    for (int i = 0; i < VARS_PER_WORD; ++i)
        state[VARS_PER_WORD * k + i] = val;
}

/* Initializes POOL over VARS with the type TYPE, the FFD layout, the
 * minimum state size MIN_SIZE, the number of states of the check
 * CHECK_STATES, and the minimum ratio MIN_RATIO. */
static void poolInit(pddl_fdr_state_pool_t *pool,
                     const pddl_fdr_vars_t *vars,
                     pddl_fdr_state_pool_type_t type,
                     int min_size,
                     int check_states,
                     float min_ratio)
{
    pddl_fdr_state_pool_config_t cfg = PDDL_FDR_STATE_POOL_CONFIG_INIT;
    cfg.type = type;
    cfg.packer_cfg.vars = vars;
    cfg.tree_min_state_size = min_size;
    cfg.tree_ffd_check_states = check_states;
    cfg.tree_ffd_min_ratio = min_ratio;
    int ret = pddlFDRStatePoolInit(pool, &cfg, NULL);
    assert(ret == 0);
}

/* Returns the statistics of POOL. */
static pddl_fdr_state_pool_stat_t poolStat(const pddl_fdr_state_pool_t *pool)
{
    pddl_fdr_state_pool_stat_t stat;
    pddlFDRStatePoolStat(pool, &stat);
    return stat;
}

/* Asserts that the state STATE_ID of POOL is STATE of NUM_VARS variables. */
static void assertGet(const pddl_fdr_state_pool_t *pool,
                      pddl_state_id_t state_id,
                      const int *state,
                      int num_vars)
{
    int *state2 = calloc(num_vars, sizeof(int));
    pddlFDRStatePoolGet(pool, state_id, state2);
    assert(memcmp(state, state2, sizeof(int) * num_vars) == 0);
    free(state2);
}

/* Fills STATE of NUM_VARS variables (see varsInitWords()) with random
 * values smaller than MAX_VAL, so that a small MAX_VAL gives many
 * duplicates and shared subtrees. */
static void randState(int *state, int num_vars, int max_val,
                      pddl_rand_t *rnd)
{
    for (int i = 0; i < num_vars; ++i)
        state[i] = pddlRandInt(rnd) % (uint32_t)max_val;
}

/*
 * The default configuration is the array representation with the default
 * packer, the minimum FFD state size 16 bytes, the check at 1000000 states,
 * and the minimum compression ratio 1.2.
 */
TEST_ONCE(fdr_state_pool_config_init)
{
    pddl_fdr_state_pool_config_t cfg = PDDL_FDR_STATE_POOL_CONFIG_INIT;
    assert(cfg.type == PDDL_FDR_STATE_POOL_ARRAY);
    assert(cfg.packer_cfg.vars == NULL);
    assert(cfg.packer_cfg.layout == PDDL_FDR_STATE_PACKER_LAYOUT_FFD);
    assert(cfg.tree_min_state_size == 16);
    assert(cfg.tree_ffd_check_states == 1000000);
    assert(cfg.tree_ffd_min_ratio == 1.5f);
}

/*
 * The tree representation with 3, 4, 5, 8, and 13 words (the odd numbers
 * give unbalanced trees) assigns the same IDs as the array representation
 * to the same sequence of random states with many duplicates (values in
 * {0, ..., 2}), returns the stored states, and re-inserting a stored state
 * returns its ID and adds no pair to the tree. More than 2 * 4096 distinct
 * states are inserted, so the hash tables of both indexed sets are
 * resized.
 */
TEST_ONCE(fdr_state_pool_tree_round_trip)
{
    static const int num_words[] = { 3, 4, 5, 8, 13 };
    const int num_cases = sizeof(num_words) / sizeof(num_words[0]);
    const int num = 20000;

    pddl_rand_t rnd;
    pddlRandInit(&rnd, 2026);
    for (int ci = 0; ci < num_cases; ++ci){
        pddl_fdr_vars_t vars;
        varsInitWords(&vars, num_words[ci]);
        const int num_vars = vars.var_size;
        pddl_fdr_state_pool_t tree, array;
        poolInit(&tree, &vars, PDDL_FDR_STATE_POOL_TREE, 0, 1, 1.f);
        poolInit(&array, &vars, PDDL_FDR_STATE_POOL_ARRAY, 0, 1, 1.f);
        assert(tree.is_tree);
        assert(!array.is_tree);
        assert(pddlFDRStatePackerNumWords(&tree.packer) == num_words[ci]);

        int *states = calloc((size_t)num * num_vars, sizeof(int));
        for (int i = 0; i < num; ++i){
            int *state = states + (size_t)i * num_vars;
            randState(state, num_vars, 3, &rnd);
            pddl_state_id_t id_array = pddlFDRStatePoolInsert(&array, state);
            pddl_state_id_t id = pddlFDRStatePoolInsert(&tree, state);
            assert(id == id_array);
            assert(pddlFDRStatePoolSize(&tree)
                    == pddlFDRStatePoolSize(&array));
        }

        pddl_fdr_state_pool_stat_t stat = poolStat(&tree);
        assert(stat.is_tree);
        assert(stat.num_states == pddlFDRStatePoolSize(&array));
        assert(stat.num_states > 2 * 4096);
        assert(stat.packed_state_size == 4u * num_words[ci]);
        assert(stat.ffd_state_size == 4u * num_words[ci]);
        assert(stat.tree_nodes > 0);
        // Every state adds at most W - 2 pairs besides its root pair
        assert(stat.tree_nodes <= stat.num_states * (num_words[ci] - 2));
        assert(stat.htable_buckets > 4096);

        for (int i = 0; i < num; ++i){
            const int *state = states + (size_t)i * num_vars;
            pddl_state_id_t id = pddlFDRStatePoolInsert(&tree, state);
            pddl_state_id_t id_array = pddlFDRStatePoolInsert(&array, state);
            assert(id == id_array);
            assertGet(&tree, id, state, num_vars);
        }
        pddl_fdr_state_pool_stat_t stat2 = poolStat(&tree);
        assert(stat2.num_states == stat.num_states);
        assert(stat2.tree_nodes == stat.tree_nodes);

        free(states);
        pddlFDRStatePoolFree(&tree);
        pddlFDRStatePoolFree(&array);
        pddlFDRVarsFree(&vars);
    }
    pddlRandFree(&rnd);
}

/*
 * The pairs of the internal nodes are shared by all states and all
 * positions in the tree. With 4 words, the tree is ((0 1) (2 3)), the root
 * pair is stored in .states, and the pairs of the nodes (0 1) and (2 3)
 * in .nodes; the words are given by the values a, b, c, d of their
 * variables:
 *   - (1, 2, 1, 2): both nodes are the same pair -> 1 pair;
 *   - (1, 2, 3, 4): (1, 2) is shared, (3, 4) is new -> 2 pairs;
 *   - (3, 4, 1, 2): both pairs are stored (at the other positions), only
 *     the root pair is new -> 2 pairs;
 *   - (1, 2, 1, 2) again: the ID 0, nothing new.
 */
TEST_ONCE(fdr_state_pool_tree_nodes)
{
    static const int words[4][4] = {
        { 1, 2, 1, 2 },
        { 1, 2, 3, 4 },
        { 3, 4, 1, 2 },
        { 1, 2, 1, 2 },
    };
    static const int exp_id[4] = { 0, 1, 2, 0 };
    static const int exp_nodes[4] = { 1, 2, 2, 2 };
    static const int exp_states[4] = { 1, 2, 3, 3 };

    pddl_fdr_vars_t vars;
    varsInitWords(&vars, 4);
    pddl_fdr_state_pool_t pool;
    poolInit(&pool, &vars, PDDL_FDR_STATE_POOL_TREE, 0, 1, 1.f);
    assert(pool.is_tree);

    int state[4 * VARS_PER_WORD];
    for (int i = 0; i < 4; ++i){
        for (int k = 0; k < 4; ++k)
            setWord(state, k, words[i][k]);
        pddl_state_id_t id = pddlFDRStatePoolInsert(&pool, state);
        assert(id == (pddl_state_id_t)exp_id[i]);
        pddl_fdr_state_pool_stat_t stat = poolStat(&pool);
        assert(stat.tree_nodes == (size_t)exp_nodes[i]);
        assert(stat.num_states == (size_t)exp_states[i]);
    }

    for (int i = 0; i < 3; ++i){
        for (int k = 0; k < 4; ++k)
            setWord(state, k, words[i][k]);
        assertGet(&pool, i, state, vars.var_size);
    }

    pddlFDRStatePoolFree(&pool);
    pddlFDRVarsFree(&vars);
}

/*
 * The tree is the complete binary tree in the heap order with the words in
 * the order of the packer as its leaves, so with d = floor(log2(W)) and
 * P = 2^d, the words 0, ..., 2P - W - 1 are at the depth d and the other
 * words at the depth d + 1 (e.g., W = 5: (((3 4) 0) (1 2)), W = 6:
 * (((2 3) (4 5)) (0 1))). This is checked for W = 3, ..., 8 by the
 * number of pairs added to .nodes: after a state with all words 0, a
 * state differing only in the word k (set to k + 1, so that no pair is
 * shared with the other such states) adds the pairs of all ancestors of
 * the word except the root, i.e., depth(k) - 1 pairs.
 */
TEST_ONCE(fdr_state_pool_tree_shape)
{
    for (int W = 3; W <= 8; ++W){
        int d = 0;
        while ((2 << d) <= W)
            ++d;
        const int P = 1 << d;

        pddl_fdr_vars_t vars;
        varsInitWords(&vars, W);
        pddl_fdr_state_pool_t pool;
        poolInit(&pool, &vars, PDDL_FDR_STATE_POOL_TREE, 0, 1, 1.f);
        assert(pool.is_tree);
        assert(pddlFDRStatePackerNumWords(&pool.packer) == W);

        int *state = calloc(vars.var_size, sizeof(int));
        pddl_state_id_t id = pddlFDRStatePoolInsert(&pool, state);
        assert(id == 0);
        size_t nodes = poolStat(&pool).tree_nodes;
        for (int k = 0; k < W; ++k){
            setWord(state, k, k + 1);
            id = pddlFDRStatePoolInsert(&pool, state);
            assert(id == (pddl_state_id_t)(k + 1));
            int depth = (k < 2 * P - W ? d : d + 1);
            size_t nodes2 = poolStat(&pool).tree_nodes;
            assert(nodes2 == nodes + depth - 1);
            nodes = nodes2;
            assertGet(&pool, id, state, vars.var_size);
            setWord(state, k, 0);
        }

        free(state);
        pddlFDRStatePoolFree(&pool);
        pddlFDRVarsFree(&vars);
    }
}

/*
 * The tree representation is used only by the tree types, only if the
 * FFD-packed state has at least .tree_min_state_size bytes, and only with
 * at least three words:
 *   - array with 4 words is never a tree;
 *   - tree and tree-ffd with 4 words (16 bytes) are trees with the minimum
 *     size 16, but not with 17 (then tree-ffd checks nothing);
 *   - tree with 2 words is not a tree even with the minimum size 0.
 * The pools in the array representation store and return states.
 */
TEST_ONCE(fdr_state_pool_tree_fallback)
{
    static const struct {
        int num_words;
        pddl_fdr_state_pool_type_t type;
        int min_size;
        pddl_bool_t is_tree;
    } cases[] = {
        { 4, PDDL_FDR_STATE_POOL_ARRAY, 0, pddl_false },
        { 4, PDDL_FDR_STATE_POOL_TREE, 16, pddl_true },
        { 4, PDDL_FDR_STATE_POOL_TREE, 17, pddl_false },
        { 4, PDDL_FDR_STATE_POOL_TREE_FFD, 16, pddl_true },
        { 4, PDDL_FDR_STATE_POOL_TREE_FFD, 17, pddl_false },
        { 2, PDDL_FDR_STATE_POOL_TREE, 0, pddl_false },
        { 2, PDDL_FDR_STATE_POOL_TREE_FFD, 0, pddl_false },
    };
    const int num_cases = sizeof(cases) / sizeof(cases[0]);

    pddl_rand_t rnd;
    pddlRandInit(&rnd, 3);
    for (int ci = 0; ci < num_cases; ++ci){
        pddl_fdr_vars_t vars;
        varsInitWords(&vars, cases[ci].num_words);
        pddl_fdr_state_pool_t pool;
        poolInit(&pool, &vars, cases[ci].type, cases[ci].min_size, 10, 1.f);
        assert(pool.is_tree == cases[ci].is_tree);
        assert(pool.has_ffd_packer
                == (cases[ci].is_tree
                        && cases[ci].type == PDDL_FDR_STATE_POOL_TREE_FFD));
        assert(pool.tree_check_num_states == (pool.has_ffd_packer ? 10 : 0));

        pddl_fdr_state_pool_stat_t stat = poolStat(&pool);
        assert(stat.is_tree == cases[ci].is_tree);
        if (cases[ci].type == PDDL_FDR_STATE_POOL_ARRAY){
            assert(stat.ffd_state_size == 0);
        }else{
            assert(stat.ffd_state_size == 4u * cases[ci].num_words);
        }
        if (!cases[ci].is_tree){
            assert(stat.packed_state_size == 4u * cases[ci].num_words);
            assert(stat.compression_ratio == 1.);
        }

        int *state = calloc(vars.var_size, sizeof(int));
        for (int i = 0; i < 100; ++i){
            randState(state, vars.var_size, VAL_SIZE, &rnd);
            pddl_state_id_t id = pddlFDRStatePoolInsert(&pool, state);
            assertGet(&pool, id, state, vars.var_size);
        }
        free(state);
        pddlFDRStatePoolFree(&pool);
        pddlFDRVarsFree(&vars);
    }
    pddlRandFree(&rnd);
}

/*
 * Invalid configurations of the pool fail with an error: an unknown type,
 * and a non-positive number of states of the check of tree-ffd.
 */
TEST_ONCE(fdr_state_pool_err_config)
{
    pddl_fdr_vars_t vars;
    varsInitWords(&vars, 4);
    pddl_fdr_state_pool_t pool;
    pddl_err_t err;

    pddl_fdr_state_pool_config_t cfg = PDDL_FDR_STATE_POOL_CONFIG_INIT;
    cfg.packer_cfg.vars = &vars;
    cfg.type = (pddl_fdr_state_pool_type_t)-100;
    pddlErrInit(&err);
    int ret = pddlFDRStatePoolInit(&pool, &cfg, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));

    cfg.type = PDDL_FDR_STATE_POOL_TREE_FFD;
    cfg.tree_ffd_check_states = 0;
    pddlErrInit(&err);
    ret = pddlFDRStatePoolInit(&pool, &cfg, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));

    pddlFDRVarsFree(&vars);
}

/* Fills STATE of the redundant states of the tree-ffd tests: 64 words, all
 * words zero except the last one, which is given by I (the last eight
 * variables encode I in base VAL_SIZE). */
static void redundantState(int *state, int num_vars, int i)
{
    memset(state, 0, sizeof(int) * num_vars);
    for (int v = num_vars - 1; v >= num_vars - VARS_PER_WORD; --v){
        state[v] = i % VAL_SIZE;
        i /= VAL_SIZE;
    }
}

/* Inserts the states FROM, ..., FROM + NUM - 1 into POOL, asserts that
 * they get consecutive IDs (so they must be new), and stores them in
 * STATES (indexed by the IDs). The states are given by redundantState() if
 * RND is NULL, and they are random states generated by RND otherwise. */
static void insertStates(pddl_fdr_state_pool_t *pool,
                         int *states,
                         int num_vars,
                         int from,
                         int num,
                         pddl_rand_t *rnd)
{
    for (int i = from; i < from + num; ++i){
        int *state = states + (size_t)i * num_vars;
        if (rnd == NULL){
            redundantState(state, num_vars, i);
        }else{
            randState(state, num_vars, VAL_SIZE, rnd);
        }
        pddl_state_id_t id = pddlFDRStatePoolInsert(pool, state);
        assert(id == (pddl_state_id_t)i);
    }
}

/* Returns the compression ratio of the tree representation (without any
 * switch) after NUM redundant (SEED < 0) or random (with the seed SEED)
 * states over VARS (see insertStates()). */
static double treeRatio(const pddl_fdr_vars_t *vars, int num, int seed)
{
    pddl_rand_t rnd;
    pddlRandInit(&rnd, seed);
    int *states = calloc((size_t)num * vars->var_size, sizeof(int));
    pddl_fdr_state_pool_t pool;
    poolInit(&pool, vars, PDDL_FDR_STATE_POOL_TREE, 0, 1, 1.f);
    insertStates(&pool, states, vars->var_size, 0, num,
                 (seed < 0 ? NULL : &rnd));
    double r = poolStat(&pool).compression_ratio;
    pddlFDRStatePoolFree(&pool);
    free(states);
    pddlRandFree(&rnd);
    return r;
}

/* Runs tree-ffd over VARS with the check at CHECK states and the minimum
 * ratio MIN_RATIO on 2 * CHECK redundant (SEED < 0) or random (with the
 * seed SEED) states, and returns true if the pool switched to the array.
 * Asserts that the pool keeps the tree until the check, that it switches
 * or keeps the tree exactly at the check, and that after the switch:
 *   - the pool is the array of FFD-packed states without the tree;
 *   - all states stored before the switch are returned by their IDs;
 *   - re-inserting them returns their IDs;
 *   - new states get consecutive IDs and are returned. */
static pddl_bool_t runTreeFFD(const pddl_fdr_vars_t *vars,
                              int check,
                              float min_ratio,
                              int seed)
{
    const int num_vars = vars->var_size;
    pddl_rand_t rnd;
    pddlRandInit(&rnd, seed);
    pddl_rand_t *r = (seed < 0 ? NULL : &rnd);
    int *states = calloc((size_t)2 * check * num_vars, sizeof(int));

    pddl_fdr_state_pool_t pool;
    poolInit(&pool, vars, PDDL_FDR_STATE_POOL_TREE_FFD, 0, check, min_ratio);
    assert(pool.is_tree);
    insertStates(&pool, states, num_vars, 0, check - 1, r);
    assert(pool.is_tree);
    assert(pool.tree_check_num_states == check);
    insertStates(&pool, states, num_vars, check - 1, 1, r);
    assert(pool.tree_check_num_states == 0);
    pddl_bool_t switched = !pool.is_tree;

    pddl_fdr_state_pool_stat_t stat = poolStat(&pool);
    assert(stat.num_states == (size_t)check);
    if (switched){
        assert(!pool.has_ffd_packer);
        assert(stat.packed_state_size == stat.ffd_state_size);
        assert(stat.tree_nodes == 0);
        assert(stat.compression_ratio == 1.);
    }else{
        assert(pool.has_ffd_packer);
        assert(stat.tree_nodes > 0);
    }

    for (int i = 0; i < check; ++i){
        const int *state = states + (size_t)i * num_vars;
        assertGet(&pool, i, state, num_vars);
        pddl_state_id_t id = pddlFDRStatePoolInsert(&pool, state);
        assert(id == (pddl_state_id_t)i);
    }
    insertStates(&pool, states, num_vars, check, check, r);
    assert(pool.is_tree == !switched);
    for (int i = 0; i < 2 * check; ++i)
        assertGet(&pool, i, states + (size_t)i * num_vars, num_vars);

    pddlFDRStatePoolFree(&pool);
    free(states);
    pddlRandFree(&rnd);
    return switched;
}

/*
 * tree-ffd with 64 words and the check at 1000 states: redundant states
 * (only the last word differs) compress well, so with the minimum ratio
 * 1.2 the tree is kept; random states do not compress at all (the ratio is
 * below 1), so the pool switches to the array. The minimum ratio decides:
 * the redundant states switch with the minimum ratio slightly above their
 * ratio at the check, and keep the tree slightly below it.
 */
TEST_ONCE(fdr_state_pool_tree_ffd)
{
    const int check = 1000;
    pddl_fdr_vars_t vars;
    varsInitWords(&vars, 64);

    double redundant = treeRatio(&vars, check, -1);
    double random = treeRatio(&vars, check, 7);
    assert(redundant > 1.2);
    assert(random < 1.);

    assert(!runTreeFFD(&vars, check, 1.2f, -1));
    assert(runTreeFFD(&vars, check, 1.2f, 7));
    assert(runTreeFFD(&vars, check, redundant * 1.01, -1));
    assert(!runTreeFFD(&vars, check, redundant * 0.99, -1));
    // The check at the first state
    assert(runTreeFFD(&vars, 1, 1e9f, 7));

    pddlFDRVarsFree(&vars);
}

/*
 * The array of tree-ffd uses the FFD layout, whereas the array of tree uses
 * the configured layout: the variables of the widths 20, 12, 12, 20, with
 * one operator changing both 12-bit variables, are packed into 2 words by
 * FFD and into 3 words by the effect affinity layout ({1, 2}, {0}, {3}).
 * So:
 *   - tree uses the effect affinity packer (12 bytes) both as a tree and
 *     as an array (minimum size 9 > 8 bytes of FFD);
 *   - tree-ffd that is not a tree uses the FFD packer (8 bytes);
 *   - tree-ffd that switches converts the states from the effect affinity
 *     packer (12 bytes) to the FFD packer (8 bytes).
 */
TEST_ONCE(fdr_state_pool_tree_ffd_packer)
{
    static const int val_size[4] = { 1 << 20, 1 << 12, 1 << 12, 1 << 20 };
    pddl_fdr_vars_t vars;
    memset(&vars, 0, sizeof(vars));
    for (int i = 0; i < 4; ++i)
        pddlFDRVarsAdd(&vars, val_size[i]);
    pddl_fdr_ops_t ops;
    pddlFDROpsInit(&ops);
    pddl_fdr_op_t *op = pddlFDROpNewEmpty();
    pddlFDRPartStateSet(&op->eff, 1, 1);
    pddlFDRPartStateSet(&op->eff, 2, 1);
    pddlFDROpsAddSteal(&ops, op);

    pddl_fdr_state_pool_config_t cfg = PDDL_FDR_STATE_POOL_CONFIG_INIT;
    cfg.packer_cfg.vars = &vars;
    cfg.packer_cfg.ops = &ops;
    cfg.packer_cfg.layout = PDDL_FDR_STATE_PACKER_LAYOUT_EFF_AFFINITY;

    static const struct {
        pddl_fdr_state_pool_type_t type;
        int min_size;
        pddl_bool_t is_tree;
        int bufsize;
        int bufsize_after;
    } cases[] = {
        { PDDL_FDR_STATE_POOL_TREE, 0, pddl_true, 12, 12 },
        { PDDL_FDR_STATE_POOL_TREE, 9, pddl_false, 12, 12 },
        { PDDL_FDR_STATE_POOL_TREE_FFD, 9, pddl_false, 8, 8 },
        { PDDL_FDR_STATE_POOL_TREE_FFD, 0, pddl_true, 12, 8 },
    };
    const int num_cases = sizeof(cases) / sizeof(cases[0]);

    pddl_rand_t rnd;
    pddlRandInit(&rnd, 11);
    const int num = 50;
    int states[50][4];
    for (int ci = 0; ci < num_cases; ++ci){
        cfg.type = cases[ci].type;
        cfg.tree_min_state_size = cases[ci].min_size;
        // The ratio is never reached, so tree-ffd switches at the check
        cfg.tree_ffd_check_states = num / 2;
        cfg.tree_ffd_min_ratio = 1e9f;
        pddl_fdr_state_pool_t pool;
        int ret = pddlFDRStatePoolInit(&pool, &cfg, NULL);
        assert(ret == 0);
        assert(pool.is_tree == cases[ci].is_tree);
        assert(pddlFDRStatePackerBufSize(&pool.packer) == cases[ci].bufsize);
        assert(poolStat(&pool).ffd_state_size == 8);

        for (int i = 0; i < num; ++i){
            for (int v = 0; v < 4; ++v)
                states[i][v] = pddlRandInt(&rnd) % (unsigned)val_size[v];
            pddl_state_id_t id = pddlFDRStatePoolInsert(&pool, states[i]);
            assert(id == (pddl_state_id_t)i);
        }
        assert(pddlFDRStatePackerBufSize(&pool.packer)
                == cases[ci].bufsize_after);
        for (int i = 0; i < num; ++i)
            assertGet(&pool, i, states[i], 4);
        pddlFDRStatePoolFree(&pool);
    }
    pddlRandFree(&rnd);
    pddlFDROpsFree(&ops);
    pddlFDRVarsFree(&vars);
}

/*
 * States of the task reached by random walks from the initial state (with
 * a fixed seed) get the same IDs in the array, in the tree (used if the
 * packed state of the task has at least three words), and in tree-ffd
 * switching to the array at 1000 states, and all pools return the stored
 * states.
 */
TEST(fdr_state_pool_walk, fdr)
{
    const int num_vars = C.fdr.var.var_size;
    const int num_walks = 100;
    const int walk_len = 200;

    pddl_fdr_state_pool_config_t cfg = PDDL_FDR_STATE_POOL_CONFIG_INIT;
    cfg.packer_cfg.vars = &C.fdr.var;
    cfg.packer_cfg.ops = &C.fdr.op;
    cfg.tree_min_state_size = 0;
    cfg.tree_ffd_check_states = 1000;
    cfg.tree_ffd_min_ratio = 1e9f;
    pddl_fdr_state_pool_t pool[3];
    static const pddl_fdr_state_pool_type_t types[3] = {
        PDDL_FDR_STATE_POOL_ARRAY,
        PDDL_FDR_STATE_POOL_TREE,
        PDDL_FDR_STATE_POOL_TREE_FFD,
    };
    for (int pi = 0; pi < 3; ++pi){
        cfg.type = types[pi];
        int ret = pddlFDRStatePoolInit(pool + pi, &cfg, &C.err);
        assert(ret == 0);
    }
    pddl_bool_t is_tree = pool[1].is_tree;
    assert(is_tree == (pddlFDRStatePackerNumWords(&pool[0].packer) >= 3));

    pddl_rand_t rnd;
    pddlRandInit(&rnd, 1);
    int *state = calloc(num_vars + 1, sizeof(int));
    pddl_iset_t app;
    pddlISetInit(&app);
    for (int walk = 0; walk < num_walks; ++walk){
        memcpy(state, C.fdr.init, sizeof(int) * num_vars);
        for (int step = 0; step < walk_len; ++step){
            pddl_state_id_t id = pddlFDRStatePoolInsert(pool + 0, state);
            for (int pi = 1; pi < 3; ++pi){
                pddl_state_id_t id2 = pddlFDRStatePoolInsert(pool + pi, state);
                assert(id2 == id);
            }

            pddlISetEmpty(&app);
            pddlFDRAppOpFind(&C.fdr_app_op, state, &app);
            if (pddlISetSize(&app) == 0)
                break;
            int k = pddlRandInt(&rnd) % (uint32_t)pddlISetSize(&app);
            int op_id = pddlISetGet(&app, k);
            pddlFDROpApplyOnStateInPlace(C.fdr.op.op[op_id], num_vars, state);
        }
    }

    int num_states = pddlFDRStatePoolSize(pool + 0);
    for (int id = 0; id < num_states; ++id){
        pddlFDRStatePoolGet(pool + 0, id, state);
        for (int pi = 1; pi < 3; ++pi)
            assertGet(pool + pi, id, state, num_vars);
    }
    // tree-ffd switches only if it reached the check in the tree
    assert(!pool[2].is_tree || num_states < 1000);
    assert(pool[1].is_tree == is_tree);

    pddlISetFree(&app);
    free(state);
    pddlRandFree(&rnd);
    for (int pi = 0; pi < 3; ++pi)
        pddlFDRStatePoolFree(pool + pi);
}
