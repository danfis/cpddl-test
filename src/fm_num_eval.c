/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests for the numeric expression/comparator evaluator
 * (pddlFmNumExpEval() / pddlFmNumCmpEval()).
 *
 * All tests are TEST_ONCE (not per task).
 * Run with:  cd tests && make && ./test -T _ -s fm_num_eval
 *
 * Integer overflow during evaluation causes PANIC, which is tested via
 * testPanic() running the offending evaluation in a forked subprocess.
 */

#include "pddl/fm.h"
#include "test.h"
#include "context.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define MAX_FLUENT_ARGS 2

struct fluent_def {
    int pred;
    int arg_size;
    int args[MAX_FLUENT_ARGS];
    pddl_num_val_t val;
};

struct fluent_table {
    const struct fluent_def *def;
    int size;
    /** The args pointer the callback was last called with */
    const int *last_args;
};

static struct fluent_table empty_table = { NULL, 0, NULL };

static pddl_fm_num_eval_status_t fluent_lookup(const pddl_fm_atom_t *fluent,
                                               const int *args,
                                               void *userdata,
                                               pddl_num_val_t *val)
{
    struct fluent_table *tbl = userdata;
    tbl->last_args = args;

    assert(fluent->arg_size <= MAX_FLUENT_ARGS);
    int resolved[MAX_FLUENT_ARGS];
    for (int i = 0; i < fluent->arg_size; ++i){
        if (fluent->arg[i].param >= 0){
            resolved[i] = args[fluent->arg[i].param];
        }else{
            resolved[i] = fluent->arg[i].obj;
        }
    }

    for (int i = 0; i < tbl->size; ++i){
        const struct fluent_def *d = tbl->def + i;
        if (d->pred != fluent->pred || d->arg_size != fluent->arg_size)
            continue;
        if (memcmp(d->args, resolved, sizeof(int) * d->arg_size) == 0){
            pddlNumValInitCopy(val, &d->val);
            return PDDL_FM_NUM_EVAL_OK;
        }
    }
    return PDDL_FM_NUM_EVAL_UNDEF;
}

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

// (pred ?x) -- unary fluent whose argument refers to action parameter PARAM
static pddl_fm_num_exp_t *exp_fluent_param(int pred, int param)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(1);
    a->pred = pred;
    a->arg[0].param = param;
    return pddlFmNewNumExpFluent(a);
}

// (pred obj) -- unary ground fluent with the object OBJ as its argument
static pddl_fm_num_exp_t *exp_fluent_obj(int pred, int obj)
{
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(1);
    a->pred = pred;
    a->arg[0].obj = obj;
    return pddlFmNewNumExpFluent(a);
}

/** Evaluates E and deletes it; returns the status */
static pddl_fm_num_eval_status_t eval_exp_del(pddl_fm_num_exp_t *e,
                                              const int *args,
                                              struct fluent_table *tbl,
                                              pddl_num_val_t *val)
{
    pddl_fm_num_eval_status_t st;
    st = pddlFmNumExpEval(e, args, fluent_lookup, tbl, val);
    pddlFmDel(&e->fm);
    return st;
}

/** Builds (TYPE LEFT RIGHT), evaluates it, and deletes it */
static pddl_fm_num_eval_status_t eval_cmp_del(pddl_fm_type_t type,
                                              pddl_fm_num_exp_t *left,
                                              pddl_fm_num_exp_t *right,
                                              const int *args,
                                              struct fluent_table *tbl)
{
    pddl_fm_num_cmp_t *c = pddlFmNewNumCmp(type, left, right);
    pddl_fm_num_eval_status_t st;
    st = pddlFmNumCmpEval(c, args, fluent_lookup, tbl);
    pddlFmDel(&c->fm);
    return st;
}

/** Evaluates (TYPE L R) on constants and asserts OK; returns the value */
static pddl_num_val_t eval_bin_ok(pddl_fm_type_t type,
                                  pddl_num_val_t l,
                                  pddl_num_val_t r)
{
    pddl_fm_num_exp_t *e = exp_bin(type, pddlFmNewNumExpNum(&l),
                                   pddlFmNewNumExpNum(&r));
    pddl_num_val_t out;
    assert(eval_exp_del(e, NULL, &empty_table, &out) == PDDL_FM_NUM_EVAL_OK);
    return out;
}

static void assert_bin(pddl_fm_type_t type,
                       pddl_num_val_t l,
                       pddl_num_val_t r,
                       pddl_num_val_t expect)
{
    pddl_num_val_t out = eval_bin_ok(type, l, r);
    assert(pddlNumValEq(&out, &expect));
}

TEST_ONCE(fm_num_eval)
{
    pddl_num_val_t out, expect;

    // 1. Constants: evaluate to exactly the stored value (type included)
    pddl_fm_num_exp_t *e = pddlFmNewNumExpNumInt(42);
    assert(eval_exp_del(e, NULL, &empty_table, &out) == PDDL_FM_NUM_EVAL_OK);
    expect = mk_int(42);
    assert(pddlNumValEq(&out, &expect));
    e = pddlFmNewNumExpNumFlt(0.5);
    assert(eval_exp_del(e, NULL, &empty_table, &out) == PDDL_FM_NUM_EVAL_OK);
    expect = mk_flt(0.5);
    assert(pddlNumValEq(&out, &expect));

    // 2. Fluent resolution
    struct fluent_def defs[3] = {
        { 0, 0, { 0, 0 }, mk_int(7) },
        { 1, 1, { 3, 0 }, mk_flt(2.5) },
        { 1, 1, { 4, 0 }, mk_int(-2) },
    };
    struct fluent_table tbl = { defs, 3, NULL };

    // ground 0-ary fluent with args == NULL
    e = exp_fluent0(0);
    assert(eval_exp_del(e, NULL, &tbl, &out) == PDDL_FM_NUM_EVAL_OK);
    expect = mk_int(7);
    assert(pddlNumValEq(&out, &expect));
    assert(tbl.last_args == NULL);

    // parametrized fluent: parameter 1 resolves to 3 through args
    int args[2] = { 9, 3 };
    e = exp_fluent_param(1, 1);
    assert(eval_exp_del(e, args, &tbl, &out) == PDDL_FM_NUM_EVAL_OK);
    expect = mk_flt(2.5);
    assert(pddlNumValEq(&out, &expect));
    // the callback must have received exactly the caller's args pointer
    assert(tbl.last_args == args);

    // ground unary fluent with an object argument
    e = exp_fluent_obj(1, 4);
    assert(eval_exp_del(e, NULL, &tbl, &out) == PDDL_FM_NUM_EVAL_OK);
    expect = mk_int(-2);
    assert(pddlNumValEq(&out, &expect));

    // fluent missing from the table -> UNDEF, output value untouched
    pddl_num_val_t sentinel = mk_int(-12345);
    pddlNumValInitCopy(&out, &sentinel);
    e = exp_fluent0(5);
    assert(eval_exp_del(e, NULL, &tbl, &out) == PDDL_FM_NUM_EVAL_UNDEF);
    assert(pddlNumValEq(&out, &sentinel));

    // 3. Arithmetic: value and sticky-float type discipline
    // INT op INT
    assert_bin(PDDL_FM_NUM_EXP_PLUS, mk_int(2), mk_int(3), mk_int(5));
    assert_bin(PDDL_FM_NUM_EXP_MINUS, mk_int(2), mk_int(3), mk_int(-1));
    assert_bin(PDDL_FM_NUM_EXP_MULT, mk_int(2), mk_int(3), mk_int(6));
    // INT/INT with zero remainder stays INT, with a remainder becomes FLT
    assert_bin(PDDL_FM_NUM_EXP_DIV, mk_int(6), mk_int(3), mk_int(2));
    assert_bin(PDDL_FM_NUM_EXP_DIV, mk_int(2), mk_int(3), mk_flt(2. / 3.));
    // INT op FLT
    assert_bin(PDDL_FM_NUM_EXP_PLUS, mk_int(2), mk_flt(0.5), mk_flt(2.5));
    assert_bin(PDDL_FM_NUM_EXP_MINUS, mk_int(2), mk_flt(0.5), mk_flt(1.5));
    assert_bin(PDDL_FM_NUM_EXP_MULT, mk_int(2), mk_flt(0.5), mk_flt(1.));
    assert_bin(PDDL_FM_NUM_EXP_DIV, mk_int(2), mk_flt(0.5), mk_flt(4.));
    // FLT op INT
    assert_bin(PDDL_FM_NUM_EXP_PLUS, mk_flt(0.5), mk_int(2), mk_flt(2.5));
    assert_bin(PDDL_FM_NUM_EXP_MINUS, mk_flt(0.5), mk_int(2), mk_flt(-1.5));
    assert_bin(PDDL_FM_NUM_EXP_MULT, mk_flt(0.5), mk_int(2), mk_flt(1.));
    assert_bin(PDDL_FM_NUM_EXP_DIV, mk_flt(0.5), mk_int(2), mk_flt(0.25));
    // FLT op FLT
    assert_bin(PDDL_FM_NUM_EXP_PLUS, mk_flt(0.5), mk_flt(0.25), mk_flt(0.75));
    assert_bin(PDDL_FM_NUM_EXP_MINUS, mk_flt(0.5), mk_flt(0.25), mk_flt(0.25));
    assert_bin(PDDL_FM_NUM_EXP_MULT, mk_flt(0.5), mk_flt(0.5), mk_flt(0.25));
    assert_bin(PDDL_FM_NUM_EXP_DIV, mk_flt(0.5), mk_flt(0.25), mk_flt(2.));

    // Nested expression ((a + 2) * b - 6) / (c - 1) with fluent leaves
    // a = (0), b = (1), c = (2)
    pddl_fm_num_exp_t *nested =
        pddlFmNewNumExpDiv(
            pddlFmNewNumExpMinus(
                pddlFmNewNumExpMult(
                    pddlFmNewNumExpPlus(exp_fluent0(0),
                                        pddlFmNewNumExpNumInt(2)),
                    exp_fluent0(1)),
                pddlFmNewNumExpNumInt(6)),
            pddlFmNewNumExpMinus(exp_fluent0(2), pddlFmNewNumExpNumInt(1)));

    // a = 4, b = 2, c = 3: ((4 + 2) * 2 - 6) / (3 - 1) = 3 (INT)
    struct fluent_def nested_defs1[3] = {
        { 0, 0, { 0, 0 }, mk_int(4) },
        { 1, 0, { 0, 0 }, mk_int(2) },
        { 2, 0, { 0, 0 }, mk_int(3) },
    };
    struct fluent_table nested_tbl1 = { nested_defs1, 3, NULL };
    assert(pddlFmNumExpEval(nested, NULL, fluent_lookup, &nested_tbl1, &out)
                == PDDL_FM_NUM_EVAL_OK);
    expect = mk_int(3);
    assert(pddlNumValEq(&out, &expect));

    // a = 2.5, b = 2, c = 5: ((2.5 + 2) * 2 - 6) / (5 - 1) = 0.75 (FLT,
    // the float propagates through the whole expression)
    struct fluent_def nested_defs2[3] = {
        { 0, 0, { 0, 0 }, mk_flt(2.5) },
        { 1, 0, { 0, 0 }, mk_int(2) },
        { 2, 0, { 0, 0 }, mk_int(5) },
    };
    struct fluent_table nested_tbl2 = { nested_defs2, 3, NULL };
    assert(pddlFmNumExpEval(nested, NULL, fluent_lookup, &nested_tbl2, &out)
                == PDDL_FM_NUM_EVAL_OK);
    expect = mk_flt(0.75);
    assert(pddlNumValEq(&out, &expect));

    // 8. Purity: re-evaluating the same tree yields the same result
    assert(pddlFmNumExpEval(nested, NULL, fluent_lookup, &nested_tbl1, &out)
                == PDDL_FM_NUM_EVAL_OK);
    expect = mk_int(3);
    assert(pddlNumValEq(&out, &expect));
    pddlFmDel(&nested->fm);

    // 4. UNDEF propagation from an undefined fluent (u = (9))
    e = pddlFmNewNumExpPlus(exp_fluent0(9), pddlFmNewNumExpNumInt(1));
    assert(eval_exp_del(e, NULL, &tbl, &out) == PDDL_FM_NUM_EVAL_UNDEF);
    e = pddlFmNewNumExpPlus(pddlFmNewNumExpNumInt(1), exp_fluent0(9));
    assert(eval_exp_del(e, NULL, &tbl, &out) == PDDL_FM_NUM_EVAL_UNDEF);
    // buried two levels deep: (1 + (2 * (u - 1)))
    e = pddlFmNewNumExpPlus(
            pddlFmNewNumExpNumInt(1),
            pddlFmNewNumExpMult(
                pddlFmNewNumExpNumInt(2),
                pddlFmNewNumExpMinus(exp_fluent0(9),
                                     pddlFmNewNumExpNumInt(1))));
    assert(eval_exp_del(e, NULL, &tbl, &out) == PDDL_FM_NUM_EVAL_UNDEF);

    // 5. Division by zero
    e = pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1),
                           pddlFmNewNumExpNumInt(0));
    assert(eval_exp_del(e, NULL, &empty_table, &out)
                == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);
    e = pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1),
                           pddlFmNewNumExpNumFlt(0.));
    assert(eval_exp_del(e, NULL, &empty_table, &out)
                == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);
    e = pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(0),
                           pddlFmNewNumExpNumInt(0));
    assert(eval_exp_del(e, NULL, &empty_table, &out)
                == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);
    // fluent-valued zero denominator (z = (3) with value 0)
    struct fluent_def zero_defs[1] = {
        { 3, 0, { 0, 0 }, mk_int(0) },
    };
    struct fluent_table zero_tbl = { zero_defs, 1, NULL };
    e = pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1), exp_fluent0(3));
    assert(eval_exp_del(e, NULL, &zero_tbl, &out)
                == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);
    // nested inside a larger expression: (2 + (3 / (1 - 1)))
    e = pddlFmNewNumExpPlus(
            pddlFmNewNumExpNumInt(2),
            pddlFmNewNumExpDiv(
                pddlFmNewNumExpNumInt(3),
                pddlFmNewNumExpMinus(pddlFmNewNumExpNumInt(1),
                                     pddlFmNewNumExpNumInt(1))));
    assert(eval_exp_del(e, NULL, &empty_table, &out)
                == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);

    // 6. Comparators: full SAT/UNSAT matrix over left <, ==, > right
    struct {
        pddl_fm_type_t type;
        int sat_lt, sat_eq, sat_gt;
    } cmp_matrix[6] = {
        { PDDL_FM_NUM_CMP_EQ,  0, 1, 0 },
        { PDDL_FM_NUM_CMP_NEQ, 1, 0, 1 },
        { PDDL_FM_NUM_CMP_GE,  0, 1, 1 },
        { PDDL_FM_NUM_CMP_LE,  1, 1, 0 },
        { PDDL_FM_NUM_CMP_GT,  0, 0, 1 },
        { PDDL_FM_NUM_CMP_LT,  1, 0, 0 },
    };
    int64_t left_val[3] = { 1, 2, 3 };
    for (int ci = 0; ci < 6; ++ci){
        int sat[3] = { cmp_matrix[ci].sat_lt, cmp_matrix[ci].sat_eq,
                       cmp_matrix[ci].sat_gt };
        for (int pi = 0; pi < 3; ++pi){
            pddl_fm_num_eval_status_t st;
            st = eval_cmp_del(cmp_matrix[ci].type,
                              pddlFmNewNumExpNumInt(left_val[pi]),
                              pddlFmNewNumExpNumInt(2),
                              NULL, &empty_table);
            assert(st == (sat[pi] ? PDDL_FM_NUM_EVAL_SAT
                                  : PDDL_FM_NUM_EVAL_UNSAT));
        }
    }
    // Mixed-type equality: INT 2 compares equal to FLT 2.0 (value-based)
    for (int ci = 0; ci < 6; ++ci){
        pddl_fm_num_eval_status_t st;
        st = eval_cmp_del(cmp_matrix[ci].type,
                          pddlFmNewNumExpNumInt(2),
                          pddlFmNewNumExpNumFlt(2.),
                          NULL, &empty_table);
        assert(st == (cmp_matrix[ci].sat_eq ? PDDL_FM_NUM_EVAL_SAT
                                            : PDDL_FM_NUM_EVAL_UNSAT));
    }

    // 7. Comparator status precedence
    // undefined fluent on either side -> UNSAT for every comparator type
    for (int ci = 0; ci < 6; ++ci){
        assert(eval_cmp_del(cmp_matrix[ci].type,
                            exp_fluent0(9), pddlFmNewNumExpNumInt(1),
                            NULL, &tbl) == PDDL_FM_NUM_EVAL_UNSAT);
        assert(eval_cmp_del(cmp_matrix[ci].type,
                            pddlFmNewNumExpNumInt(1), exp_fluent0(9),
                            NULL, &tbl) == PDDL_FM_NUM_EVAL_UNSAT);
    }
    // division by zero on either side -> DIV_BY_ZERO
    assert(eval_cmp_del(PDDL_FM_NUM_CMP_EQ,
                        pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1),
                                           pddlFmNewNumExpNumInt(0)),
                        pddlFmNewNumExpNumInt(1),
                        NULL, &tbl) == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);
    assert(eval_cmp_del(PDDL_FM_NUM_CMP_EQ,
                        pddlFmNewNumExpNumInt(1),
                        pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1),
                                           pddlFmNewNumExpNumInt(0)),
                        NULL, &tbl) == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);
    // division by zero takes precedence over an undefined fluent on the
    // other side (both orders)
    assert(eval_cmp_del(PDDL_FM_NUM_CMP_EQ,
                        pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1),
                                           pddlFmNewNumExpNumInt(0)),
                        exp_fluent0(9),
                        NULL, &tbl) == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);
    assert(eval_cmp_del(PDDL_FM_NUM_CMP_EQ,
                        exp_fluent0(9),
                        pddlFmNewNumExpDiv(pddlFmNewNumExpNumInt(1),
                                           pddlFmNewNumExpNumInt(0)),
                        NULL, &tbl) == PDDL_FM_NUM_EVAL_DIV_BY_ZERO);

    // 8. Purity of comparators: same comparator evaluated twice
    pddl_fm_num_cmp_t *cmp = pddlFmNewNumCmpLE(exp_fluent0(0),
                                               pddlFmNewNumExpNumInt(7));
    assert(pddlFmNumCmpEval(cmp, NULL, fluent_lookup, &tbl)
                == PDDL_FM_NUM_EVAL_SAT);
    assert(pddlFmNumCmpEval(cmp, NULL, fluent_lookup, &tbl)
                == PDDL_FM_NUM_EVAL_SAT);
    pddlFmDel(&cmp->fm);
}

static void panic_eval_int_overflow(void *userdata)
{
    pddl_fm_num_exp_t *e;
    e = pddlFmNewNumExpPlus(pddlFmNewNumExpNumInt(INT64_MAX),
                            pddlFmNewNumExpNumInt(1));
    pddl_num_val_t out;
    pddlFmNumExpEval(e, NULL, fluent_lookup, &empty_table, &out);
}

static void panic_eval_non_num_exp(void *userdata)
{
    // an atom is not a numeric expression
    pddl_fm_atom_t *a = pddlFmNewEmptyAtom(0);
    pddl_num_val_t out;
    pddlFmNumExpEval((const pddl_fm_num_exp_t *)a, NULL,
                     fluent_lookup, &empty_table, &out);
}

static void panic_eval_non_num_cmp(void *userdata)
{
    pddl_fm_num_cmp_t *c = pddlFmNewNumCmpEq(pddlFmNewNumExpNumInt(1),
                                             pddlFmNewNumExpNumInt(1));
    c->fm.type = PDDL_FM_AND;
    pddlFmNumCmpEval(c, NULL, fluent_lookup, &empty_table);
}

static void no_panic(void *userdata)
{
    pddl_fm_num_exp_t *e;
    e = pddlFmNewNumExpPlus(pddlFmNewNumExpNumInt(1),
                            pddlFmNewNumExpNumInt(2));
    pddl_num_val_t out;
    pddlFmNumExpEval(e, NULL, fluent_lookup, &empty_table, &out);
    pddlFmDel(&e->fm);
}

TEST_ONCE(fm_num_eval_panic)
{
    assert(testPanic(panic_eval_int_overflow, NULL));
    assert(testPanic(panic_eval_non_num_exp, NULL));
    assert(testPanic(panic_eval_non_num_cmp, NULL));
    assert(!testPanic(no_panic, NULL));
}
