/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests for the bottom-up fold of numeric expressions
 * (pddlFmNumExpFold()).
 */

#include "pddl/fm.h"
#include "test.h"
#include "context.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static pddl_num_val_t mk_int(int64_t val)
{
    pddl_num_val_t v;
    pddlNumValInitInt(&v, val);
    return v;
}

static pddl_num_val_t mk_flt(double val)
{
    pddl_num_val_t v;
    pddlNumValInitFlt(&v, val);
    return v;
}

static pddl_fm_num_exp_t *exp_bin(pddl_fm_type_t type,
                                  pddl_fm_num_exp_t *left,
                                  pddl_fm_num_exp_t *right)
{
    switch (type){
    case PDDL_FM_NUM_EXP_PLUS:
        return pddlFmNewNumExpPlus(left, right);
    case PDDL_FM_NUM_EXP_MINUS:
        return pddlFmNewNumExpMinus(left, right);
    case PDDL_FM_NUM_EXP_MULT:
        return pddlFmNewNumExpMult(left, right);
    case PDDL_FM_NUM_EXP_DIV:
        return pddlFmNewNumExpDiv(left, right);
    default:
        assert(0);
        return NULL;
    }
}

// (pred) -- 0-ary ground fluent
static pddl_fm_num_exp_t *exp_fluent0(int pred)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(0);
    a->pred = pred;
    return pddlFmNewNumExpFluent(a);
}


// --- Fold as an exact evaluator over pddl_num_val_t values ---

/** Values of 0-ary fluents indexed by the predicate ID */
struct fluent_vals {
    const pddl_num_val_t *val;
    int size;
};

static int eval_leaf(const pddl_fm_num_exp_t *leaf, void *ud, void *val)
{
    const struct fluent_vals *fv = ud;
    pddl_num_val_t *out = val;
    if (leaf->fm.type == PDDL_FM_NUM_EXP_NUM){
        pddlNumValInitCopy(out, &leaf->e.num);
        return 0;
    }
    assert(leaf->fm.type == PDDL_FM_NUM_EXP_FLUENT);
    assert(leaf->e.fluent->pred < fv->size);
    pddlNumValInitCopy(out, fv->val + leaf->e.fluent->pred);
    return 0;
}

#define EVAL_DIV_BY_ZERO 42

static int eval_bin_op(const pddl_fm_num_exp_t *e, const void *left,
                       const void *right, void *ud, void *val)
{
    const pddl_num_val_t *l = left;
    const pddl_num_val_t *r = right;
    pddl_num_val_t *out = val;
    switch (e->fm.type){
    case PDDL_FM_NUM_EXP_PLUS:
        pddlNumValAddTo(out, l, r);
        return 0;
    case PDDL_FM_NUM_EXP_MINUS:
        pddlNumValSubTo(out, l, r);
        return 0;
    case PDDL_FM_NUM_EXP_MULT:
        pddlNumValMulTo(out, l, r);
        return 0;
    default:
        assert(e->fm.type == PDDL_FM_NUM_EXP_DIV);
        if (pddlNumValDivTo(out, l, r) != 0)
            return EVAL_DIV_BY_ZERO;
        return 0;
    }
}

static void eval_free(void *val, void *ud)
{
    pddlNumValFree(val);
}

static pddl_fm_num_eval_status_t eval_fluent_fn(const pddl_fm_atom_t *fluent,
                                                const int *args,
                                                void *ud,
                                                pddl_num_val_t *val)
{
    const struct fluent_vals *fv = ud;
    assert(fluent->pred < fv->size);
    pddlNumValInitCopy(val, fv->val + fluent->pred);
    return PDDL_FM_NUM_EVAL_OK;
}

/** Folds E as an evaluator and asserts the result matches
 *  pddlFmNumExpEval(); deletes E */
static void assert_fold_eval_eq(pddl_fm_num_exp_t *e,
                                const struct fluent_vals *fv)
{
    pddl_num_val_t fold_val, eval_val;
    int st = pddlFmNumExpFold(e, sizeof(pddl_num_val_t),
                              eval_leaf, eval_bin_op, eval_free,
                              (void *)fv, &fold_val);
    pddl_fm_num_eval_status_t est;
    est = pddlFmNumExpEval(e, NULL, eval_fluent_fn, (void *)fv, &eval_val);
    if (st == 0){
        assert(est == PDDL_FM_NUM_EVAL_OK);
        assert(pddlNumValEq(&fold_val, &eval_val));
    }else{
        assert(st == EVAL_DIV_BY_ZERO);
        assert(est == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);
    }
    pddlFmDel(&e->fm);
}


// --- Operand-order fold over int trace values ---

// Leaf value is the integer constant itself; a binary operation encodes
// (left * 100 + right), so the final value uniquely determines both the
// post-order structure and the left/right roles of the operands.
static int trace_leaf(const pddl_fm_num_exp_t *leaf, void *ud, void *val)
{
    assert(leaf->fm.type == PDDL_FM_NUM_EXP_NUM);
    *(int *)val = (int)leaf->e.num.v.i;
    return 0;
}

static int trace_bin_op(const pddl_fm_num_exp_t *e, const void *left,
                        const void *right, void *ud, void *val)
{
    *(int *)val = *(const int *)left * 100 + *(const int *)right;
    return 0;
}


// --- Stack-growth fold over a struct value ---

struct sum_count {
    int64_t sum;
    int count;
};

static int sum_count_leaf(const pddl_fm_num_exp_t *leaf, void *ud, void *val)
{
    struct sum_count *out = val;
    assert(leaf->fm.type == PDDL_FM_NUM_EXP_NUM);
    out->sum = leaf->e.num.v.i;
    out->count = 1;
    return 0;
}

static int sum_count_bin_op(const pddl_fm_num_exp_t *e, const void *left,
                            const void *right, void *ud, void *val)
{
    const struct sum_count *l = left;
    const struct sum_count *r = right;
    struct sum_count *out = val;
    assert(e->fm.type == PDDL_FM_NUM_EXP_PLUS);
    out->sum = l->sum + r->sum;
    out->count = l->count + r->count;
    return 0;
}

/** Builds 1 + 2 + ... + N, leaning to the right if RIGHT is true */
static pddl_fm_num_exp_t *deep_sum(int n, int right)
{
    pddl_fm_num_exp_t *e;
    if (right){
        e = pddlFmNewNumExpNumInt(n);
        for (int i = n - 1; i >= 1; --i)
            e = pddlFmNewNumExpPlus(pddlFmNewNumExpNumInt(i), e);
    }else{
        e = pddlFmNewNumExpNumInt(1);
        for (int i = 2; i <= n; ++i)
            e = pddlFmNewNumExpPlus(e, pddlFmNewNumExpNumInt(i));
    }
    return e;
}


TEST_ONCE(fm_num_exp_fold)
{
    const pddl_num_val_t fvals[3] = { mk_int(7), mk_flt(2.5), mk_int(0) };
    const struct fluent_vals fv = { fvals, 3 };

    // 1. Single-node expressions: no bin_op callback involved
    assert_fold_eval_eq(pddlFmNewNumExpNumInt(42), &fv);
    assert_fold_eval_eq(pddlFmNewNumExpNumFlt(-0.5), &fv);
    assert_fold_eval_eq(exp_fluent0(1), &fv);

    // 2. Equivalence with pddlFmNumExpEval() on all four operators,
    // nesting, and mixed int/float constants and fluents
    assert_fold_eval_eq(exp_bin(PDDL_FM_NUM_EXP_PLUS,
                                pddlFmNewNumExpNumInt(1),
                                pddlFmNewNumExpNumInt(2)), &fv);
    assert_fold_eval_eq(exp_bin(PDDL_FM_NUM_EXP_MINUS,
                                pddlFmNewNumExpNumInt(1),
                                pddlFmNewNumExpNumFlt(0.25)), &fv);
    assert_fold_eval_eq(exp_bin(PDDL_FM_NUM_EXP_MULT,
                                exp_fluent0(0),
                                exp_fluent0(1)), &fv);
    assert_fold_eval_eq(exp_bin(PDDL_FM_NUM_EXP_DIV,
                                pddlFmNewNumExpNumInt(9),
                                pddlFmNewNumExpNumInt(3)), &fv);
    assert_fold_eval_eq(
            exp_bin(PDDL_FM_NUM_EXP_MULT,
                    exp_bin(PDDL_FM_NUM_EXP_PLUS,
                            exp_fluent0(0),
                            pddlFmNewNumExpNumInt(-3)),
                    exp_bin(PDDL_FM_NUM_EXP_DIV,
                            exp_fluent0(1),
                            pddlFmNewNumExpNumFlt(0.5))), &fv);

    // 3. Operand order: (- (+ 1 2) 3) must fold to 102 * 100 + 3
    pddl_fm_num_exp_t *e;
    e = exp_bin(PDDL_FM_NUM_EXP_MINUS,
                exp_bin(PDDL_FM_NUM_EXP_PLUS,
                        pddlFmNewNumExpNumInt(1),
                        pddlFmNewNumExpNumInt(2)),
                pddlFmNewNumExpNumInt(3));
    int trace = 0;
    assert(pddlFmNumExpFold(e, sizeof(int), trace_leaf, trace_bin_op,
                            NULL, NULL, &trace) == 0);
    assert(trace == 10203);
    pddlFmDel(&e->fm);

    // 4. Stack growth on deep trees with a struct-typed value: the
    // right-leaning tree pushes all leaves before any operation collapses
    for (int right = 0; right <= 1; ++right){
        e = deep_sum(150, right);
        struct sum_count sc = { 0, 0 };
        assert(pddlFmNumExpFold(e, sizeof(sc), sum_count_leaf,
                                sum_count_bin_op, NULL, NULL, &sc) == 0);
        assert(sc.sum == 150 * 151 / 2);
        assert(sc.count == 150);
        pddlFmDel(&e->fm);
    }
}


// --- Early termination ---

struct abort_ctx {
    /** Predicate ID of the fluent whose resolution fails */
    int fail_pred;
    int leaf_calls;
    int bin_op_calls;
};

#define ABORT_CODE -7

static int abort_leaf(const pddl_fm_num_exp_t *leaf, void *ud, void *val)
{
    struct abort_ctx *ctx = ud;
    ++ctx->leaf_calls;
    if (leaf->fm.type == PDDL_FM_NUM_EXP_FLUENT
            && leaf->e.fluent->pred == ctx->fail_pred){
        return ABORT_CODE;
    }
    *(int *)val = 1;
    return 0;
}

static int abort_bin_op(const pddl_fm_num_exp_t *e, const void *left,
                        const void *right, void *ud, void *val)
{
    struct abort_ctx *ctx = ud;
    ++ctx->bin_op_calls;
    *(int *)val = 1;
    return 0;
}

TEST_ONCE(fm_num_exp_fold_abort)
{
    // (+ (* (f0) (f1)) (f2)) with (f1) failing to resolve: the fold must
    // stop right there -- no operation is combined, the failing code is
    // propagated, and the output value stays untouched
    pddl_fm_num_exp_t *e;
    e = exp_bin(PDDL_FM_NUM_EXP_PLUS,
                exp_bin(PDDL_FM_NUM_EXP_MULT,
                        exp_fluent0(0),
                        exp_fluent0(1)),
                exp_fluent0(2));
    struct abort_ctx ctx = { 1, 0, 0 };
    int out = -123;
    assert(pddlFmNumExpFold(e, sizeof(int), abort_leaf, abort_bin_op,
                            NULL, &ctx, &out) == ABORT_CODE);
    assert(ctx.leaf_calls == 2);
    assert(ctx.bin_op_calls == 0);
    assert(out == -123);
    pddlFmDel(&e->fm);

    // A failure in the bin_op callback (division by zero in the exact
    // evaluator) is propagated too
    const struct fluent_vals fv = { NULL, 0 };
    pddl_num_val_t val = mk_int(-1);
    e = exp_bin(PDDL_FM_NUM_EXP_DIV,
                pddlFmNewNumExpNumInt(1),
                pddlFmNewNumExpNumInt(0));
    assert(pddlFmNumExpFold(e, sizeof(val), eval_leaf, eval_bin_op,
                            eval_free, (void *)&fv, &val) == EVAL_DIV_BY_ZERO);
    pddl_num_val_t expect = mk_int(-1);
    assert(pddlNumValEq(&val, &expect));
    pddlFmDel(&e->fm);
}


// --- Destructor callback: values owning external resources ---

#define RES_MAX 64

struct res_pool {
    /** True for every resource slot currently owned by some value */
    pddl_bool_t alive[RES_MAX];
    int allocs;
    int frees;
    /** The leaf callback fails when acquiring this slot, -1 to disable */
    int fail_leaf;
};

/** A value owning one resource slot of the pool */
struct res_val {
    int slot;
};

#define RES_FAIL_CODE -3

static int res_acquire(struct res_pool *pool)
{
    assert(pool->allocs < RES_MAX);
    int slot = pool->allocs++;
    pool->alive[slot] = pddl_true;
    return slot;
}

static int res_leaf(const pddl_fm_num_exp_t *leaf, void *ud, void *val)
{
    struct res_pool *pool = ud;
    if (pool->allocs == pool->fail_leaf)
        return RES_FAIL_CODE;
    ((struct res_val *)val)->slot = res_acquire(pool);
    return 0;
}

static int res_bin_op(const pddl_fm_num_exp_t *e, const void *left,
                      const void *right, void *ud, void *val)
{
    struct res_pool *pool = ud;
    // The operands must still be alive when they are combined
    assert(pool->alive[((const struct res_val *)left)->slot]);
    assert(pool->alive[((const struct res_val *)right)->slot]);
    ((struct res_val *)val)->slot = res_acquire(pool);
    return 0;
}

static int res_bin_op_fail(const pddl_fm_num_exp_t *e, const void *left,
                           const void *right, void *ud, void *val)
{
    return RES_FAIL_CODE;
}

static void res_free(void *val, void *ud)
{
    struct res_pool *pool = ud;
    int slot = ((struct res_val *)val)->slot;
    // A slot that is not alive means a double free
    assert(pool->alive[slot]);
    pool->alive[slot] = pddl_false;
    ++pool->frees;
}

static void res_pool_init(struct res_pool *pool)
{
    memset(pool, 0, sizeof(*pool));
    pool->fail_leaf = -1;
}

/** Asserts that no resource slot of the pool is alive */
static void res_pool_assert_all_dead(const struct res_pool *pool)
{
    for (int i = 0; i < RES_MAX; ++i)
        assert(!pool->alive[i]);
}

// (+ (* 1 2) 3) -- 3 leaves and 2 binary operations
static pddl_fm_num_exp_t *res_exp(void)
{
    return exp_bin(PDDL_FM_NUM_EXP_PLUS,
                   exp_bin(PDDL_FM_NUM_EXP_MULT,
                           pddlFmNewNumExpNumInt(1),
                           pddlFmNewNumExpNumInt(2)),
                   pddlFmNewNumExpNumInt(3));
}

TEST_ONCE(fm_num_exp_fold_free)
{
    pddl_fm_num_exp_t *e = res_exp();
    struct res_pool pool;
    struct res_val out;

    // Successful fold: every intermediate value is destroyed exactly
    // once and the resulting value is handed over to the caller, who is
    // responsible for destroying it
    res_pool_init(&pool);
    out.slot = -1;
    assert(pddlFmNumExpFold(e, sizeof(out), res_leaf, res_bin_op,
                            res_free, &pool, &out) == 0);
    assert(pool.allocs == 5);
    assert(pool.frees == 4);
    assert(pool.alive[out.slot]);
    res_free(&out, &pool);
    assert(pool.frees == 5);
    res_pool_assert_all_dead(&pool);

    // Fold terminated by the bin_op callback: both pending operand
    // values are destroyed by the fold
    res_pool_init(&pool);
    assert(pddlFmNumExpFold(e, sizeof(out), res_leaf, res_bin_op_fail,
                            res_free, &pool, &out) == RES_FAIL_CODE);
    assert(pool.allocs == 2);
    assert(pool.frees == 2);
    res_pool_assert_all_dead(&pool);

    // Fold terminated by the leaf callback: the values created so far
    // are destroyed by the fold, the failed value was never created.
    // When the last leaf is visited, (* 1 2) has already consumed the
    // first two values and created the third one, so the failure
    // triggers at three allocations.
    res_pool_init(&pool);
    pool.fail_leaf = 3;
    assert(pddlFmNumExpFold(e, sizeof(out), res_leaf, res_bin_op,
                            res_free, &pool, &out) == RES_FAIL_CODE);
    assert(pool.allocs == 3);
    assert(pool.frees == 3);
    res_pool_assert_all_dead(&pool);

    pddlFmDel(&e->fm);
}
