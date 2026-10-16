/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the fdr_app_op_bitset module (pddl/fdr_app_op_bitset.h).
 *
 * The per-task tests compare pddlFDRAppOpBitsetFind() with brute force and
 * with pddlFDRAppOpFind() (C.fdr_app_op). The hand-built test is TEST_ONCE.
 * Run with:  cd tests && make && ./test -Q -T _ -s fdr_app_op_bitset
 */

#include "test.h"
#include "context.h"
#include <assert.h>
#include <string.h>

/** Checks that every operator is either in .op_empty_pre or in exactly one
 *  partition with .var[] equal to its precondition variables, that every
 *  bit B_{v,x}[k] equals (pre(o_k)[v] == x), that the padding bits are 0,
 *  and that .op_empty_pre and all .op_id[] are sorted */
static void appOpBitsetCheck(const pddl_fdr_app_op_bitset_t *app,
                             const pddl_fdr_vars_t *vars,
                             const pddl_fdr_ops_t *ops)
{
    int *seen = PDDL_ZALLOC_ARR(int, PDDL_MAX(ops->op_size, 1));
    for (int i = 0; i < app->op_empty_pre_size; ++i){
        int op_id = app->op_empty_pre[i];
        assert(i == 0 || app->op_empty_pre[i - 1] < op_id);
        assert(ops->op[op_id]->pre.fact_size == 0);
        assert(seen[op_id] == 0);
        seen[op_id] = 1;
    }

    for (int pi = 0; pi < app->part_size; ++pi){
        const pddl_fdr_app_op_bitset_part_t *part = app->part + pi;
        assert(part->var_size > 0);
        assert(part->op_size > 0);
        assert(part->word_size == (part->op_size + 63) / 64);

        int block_size = 0;
        for (int i = 0; i < part->var_size; ++i){
            assert(i == 0 || part->var[i - 1] < part->var[i]);
            assert(part->val_offset[i] == block_size);
            block_size += vars->var[part->var[i]].val_size;
        }
        assert(part->block_size == block_size);

        for (int j = 0; j < part->op_size; ++j){
            int op_id = part->op_id[j];
            assert(j == 0 || part->op_id[j - 1] < op_id);
            const pddl_fdr_part_state_t *pre = &ops->op[op_id]->pre;
            assert(pre->fact_size == part->var_size);
            for (int i = 0; i < part->var_size; ++i)
                assert(pre->fact[i].var == part->var[i]);
            assert(seen[op_id] == 0);
            seen[op_id] = 1;
        }

        // All bits including the padding ones
        for (int k = 0; k < part->word_size; ++k){
            const uint64_t *block = part->bits + k * part->block_size;
            for (int i = 0; i < part->var_size; ++i){
                int var = part->var[i];
                for (int x = 0; x < vars->var[var].val_size; ++x){
                    uint64_t word = block[part->val_offset[i] + x];
                    for (int b = 0; b < 64; ++b){
                        int j = 64 * k + b;
                        int exp = 0;
                        if (j < part->op_size){
                            const pddl_fdr_op_t *op = ops->op[part->op_id[j]];
                            exp = (pddlFDRPartStateGet(&op->pre, var) == x);
                        }
                        assert(((word >> b) & 1u) == (uint64_t)exp);
                    }
                }
            }
        }
    }

    for (int op_id = 0; op_id < ops->op_size; ++op_id)
        assert(seen[op_id] == 1);
    PDDL_FREE(seen);
}

/** Checks that APP finds the same operators in STATE as brute force and
 *  C.fdr_app_op, and recursively does the same for all successor states up
 *  to the depth DEPTH */
static void findOps(const pddl_fdr_app_op_bitset_t *app,
                    const int *state,
                    int depth)
{
    const pddl_fdr_ops_t *ops = &C.fdr.op;

    PDDL_ISET(found);
    int ret = pddlFDRAppOpBitsetFind(app, state, &found);
    assert(ret == pddlISetSize(&found));

    PDDL_ISET(app_brute);
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        const pddl_fdr_part_state_t *pre = &ops->op[op_id]->pre;
        if (pddlFDRPartStateIsConsistentWithState(pre, state))
            pddlISetAdd(&app_brute, op_id);
    }
    assert(pddlISetEq(&found, &app_brute));

    PDDL_ISET(app_op);
    pddlFDRAppOpFind(&C.fdr_app_op, state, &app_op);
    assert(pddlISetEq(&found, &app_op));

    if (depth > 0){
        int var_size = C.fdr.var.var_size;
        int *next_state = PDDL_ALLOC_ARR(int, var_size);
        PDDL_ISET_FOR_EACH(&found, op_id){
            memcpy(next_state, state, sizeof(int) * var_size);
            pddlFDRPartStateApplyToState(&ops->op[op_id]->eff, next_state);
            findOps(app, next_state, depth - 1);
        }
        PDDL_FREE(next_state);
    }

    pddlISetFree(&app_op);
    pddlISetFree(&app_brute);
    pddlISetFree(&found);
}

/** Runs findOps() from the initial state to the depth 2 */
static void findOpsSearch(void)
{
    pddl_fdr_app_op_bitset_t app;
    pddlFDRAppOpBitsetInit(&app, &C.fdr.var, &C.fdr.op);
    appOpBitsetCheck(&app, &C.fdr.var, &C.fdr.op);
    findOps(&app, C.fdr.init, 2);
    pddlFDRAppOpBitsetFree(&app);
}

/** Runs findOps() on NUM_SAMPLES random states */
static void findOpsRand(int num_samples)
{
    const pddl_fdr_vars_t *vars = &C.fdr.var;
    pddl_fdr_app_op_bitset_t app;
    pddlFDRAppOpBitsetInit(&app, vars, &C.fdr.op);
    appOpBitsetCheck(&app, vars, &C.fdr.op);

    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);
    int *state = PDDL_ALLOC_ARR(int, vars->var_size);
    for (int sample = 0; sample < num_samples; ++sample){
        for (int var = 0; var < vars->var_size; ++var){
            int val = pddlRand(&rnd, 0, vars->var[var].val_size);
            val = PDDL_MIN(val, vars->var[var].val_size - 1);
            state[var] = val;
        }
        findOps(&app, state, 0);
    }
    PDDL_FREE(state);
    pddlFDRAppOpBitsetFree(&app);
}

/*
 * pddl_fdr_app_op_bitset_t has a consistent structure and finds the same
 * applicable operators as brute force and pddl_fdr_app_op_t in all states
 * reachable in two steps from init.
 */
TEST(fdr_app_op_bitset_search, fdr_app_op)
{
    findOpsSearch();
}

/*
 * pddl_fdr_app_op_bitset_t has a consistent structure and finds the same
 * applicable operators as brute force and pddl_fdr_app_op_t in random
 * states.
 */
TEST(fdr_app_op_bitset_rand, fdr_app_op)
{
    findOpsRand(5000);
}

/*
 * Same as fdr_app_op_bitset_search but with essential-first FDR variables.
 */
TEST(fdr_app_op_bitset_search_essential, fdr_app_op_essential)
{
    findOpsSearch();
}

/*
 * Same as fdr_app_op_bitset_rand but with essential-first FDR variables.
 */
TEST(fdr_app_op_bitset_rand_essential, fdr_app_op_essential)
{
    findOpsRand(5000);
}

#define END -1

/** Adds an operator with the precondition given as the END-terminated
 *  array of (var, val) pairs PRE */
static void appOpBitsetAddOp(pddl_fdr_ops_t *ops, const int *pre)
{
    pddl_fdr_op_t *op = pddlFDROpNewEmpty();
    op->name = PDDL_STRDUP("op");
    for (int i = 0; pre[i] != END; i += 2)
        pddlFDRPartStateSet(&op->pre, pre[i], pre[i + 1]);
    pddlFDROpsAddSteal(ops, op);
}

/** Returns the partition of APP containing OP_ID, or NULL */
static const pddl_fdr_app_op_bitset_part_t *
appOpBitsetPartOf(const pddl_fdr_app_op_bitset_t *app, int op_id)
{
    for (int pi = 0; pi < app->part_size; ++pi){
        const pddl_fdr_app_op_bitset_part_t *part = app->part + pi;
        for (int j = 0; j < part->op_size; ++j){
            if (part->op_id[j] == op_id)
                return part;
        }
    }
    return NULL;
}

/*
 * pddl_fdr_app_op_bitset_t on a small hand-built task: operators with empty
 * preconditions are kept aside, operators are partitioned by the sets of
 * their precondition variables regardless of the values, a partition of 70
 * operators spans two words, Find agrees with brute force in all states
 * and adds to a non-empty output set, and a task without operators yields
 * no partitions.
 */
TEST_ONCE(fdr_app_op_bitset_handmade)
{
    pddl_fdr_t fdr;
    memset(&fdr, 0, sizeof(fdr));
    pddlFDRVarsAdd(&fdr.var, 3);
    pddlFDRVarsAdd(&fdr.var, 2);
    pddlFDRVarsAdd(&fdr.var, 2);
    pddlFDRVarsAdd(&fdr.var, 2);
    fdr.init = PDDL_ZALLOC_ARR(int, fdr.var.var_size);
    pddlFDROpsInit(&fdr.op);
    pddlFDRPartStateInit(&fdr.goal);

    // Without operators
    pddl_fdr_app_op_bitset_t app;
    pddlFDRAppOpBitsetInit(&app, &fdr.var, &fdr.op);
    assert(app.part_size == 0);
    assert(app.op_empty_pre_size == 0);
    PDDL_ISET(found);
    pddlISetAdd(&found, 7);
    int ret = pddlFDRAppOpBitsetFind(&app, fdr.init, &found);
    assert(ret == 0);
    assert(pddlISetSize(&found) == 1 && pddlISetIn(7, &found));
    pddlFDRAppOpBitsetFree(&app);

    appOpBitsetAddOp(&fdr.op, (int []){ END });                     // 0
    appOpBitsetAddOp(&fdr.op, (int []){ 0, 1, END });               // 1
    appOpBitsetAddOp(&fdr.op, (int []){ 0, 2, END });               // 2
    appOpBitsetAddOp(&fdr.op, (int []){ 0, 0, 1, 1, END });         // 3
    appOpBitsetAddOp(&fdr.op, (int []){ END });                     // 4
    appOpBitsetAddOp(&fdr.op, (int []){ 1, 0, 3, 1, END });         // 5
    appOpBitsetAddOp(&fdr.op, (int []){ 1, 0, 0, 1, END });         // 6
    // 7, ..., 76: all four combinations of values of the variables 2, 3
    for (int i = 0; i < 70; ++i)
        appOpBitsetAddOp(&fdr.op, (int []){ 2, i % 2, 3, (i / 2) % 2, END });
    assert(fdr.op.op_size == 77);

    pddlFDRAppOpBitsetInit(&app, &fdr.var, &fdr.op);
    appOpBitsetCheck(&app, &fdr.var, &fdr.op);
    assert(app.op_empty_pre_size == 2);
    assert(app.op_empty_pre[0] == 0 && app.op_empty_pre[1] == 4);
    assert(app.part_size == 4);

    const pddl_fdr_app_op_bitset_part_t *part = appOpBitsetPartOf(&app, 1);
    assert(part == appOpBitsetPartOf(&app, 2));
    assert(part->var_size == 1 && part->op_size == 2);
    part = appOpBitsetPartOf(&app, 3);
    assert(part == appOpBitsetPartOf(&app, 6));
    assert(part->var_size == 2 && part->op_size == 2);
    part = appOpBitsetPartOf(&app, 5);
    assert(part->var_size == 2 && part->op_size == 1);
    part = appOpBitsetPartOf(&app, 7);
    assert(part == appOpBitsetPartOf(&app, 76));
    assert(part->op_size == 70 && part->word_size == 2);

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

        pddlISetEmpty(&found);
        ret = pddlFDRAppOpBitsetFind(&app, state, &found);
        assert(ret == pddlISetSize(&exp));
        assert(pddlISetEq(&found, &exp));

        // Find adds to the existing content of the output set
        pddlISetEmpty(&found);
        pddlISetAdd(&found, 100);
        ret = pddlFDRAppOpBitsetFind(&app, state, &found);
        assert(ret == pddlISetSize(&exp));
        pddlISetAdd(&exp, 100);
        assert(pddlISetEq(&found, &exp));
        pddlISetFree(&exp);
    }

    // State 0=1, 1=0, 2=1, 3=1: operators from all partitions interleave
    state[0] = 1;
    state[1] = 0;
    state[2] = 1;
    state[3] = 1;
    pddlISetEmpty(&found);
    ret = pddlFDRAppOpBitsetFind(&app, state, &found);
    // 0, 1, 4, 5, 6 + every fourth of 7..76 starting from 7 + 3 = 10
    assert(ret == 5 + 17);
    int expected[] = { 0, 1, 4, 5, 6, 10, 14 };
    for (int i = 0; i < 7; ++i)
        assert(pddlISetGet(&found, i) == expected[i]);

    pddlISetFree(&found);
    pddlFDRAppOpBitsetFree(&app);
    pddlFDRFree(&fdr);
}
