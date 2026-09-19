/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests for the pddl_num_t numeric value (val + delta * eps).
 *
 * All tests are TEST_ONCE (not per task).
 * Run with:  cd tests && make && ./test -T _ -s num_ -a
 */

#include "pddl/num.h"
#include "test.h"
#include "context.h"
#include <assert.h>
#include <string.h>

static pddl_num_t mk_int(int val)
{
    pddl_num_t v;
    pddlNumSetInt(&v, val);
    return v;
}

static pddl_num_t mk_int_d(int val, int delta)
{
    pddl_num_t v;
    pddlNumSetInt(&v, val);
    pddlNumSetDelta(&v, delta);
    return v;
}

static pddl_num_t mk_flt(float val)
{
    pddl_num_t v;
    pddlNumSetFlt(&v, val);
    return v;
}

static pddl_num_t mk_flt_d(float val, int delta)
{
    pddl_num_t v;
    pddlNumSetFlt(&v, val);
    pddlNumSetDelta(&v, delta);
    return v;
}

/** Creates a huge number with the given sign and delta via saturation */
static pddl_num_t mk_huge(int sign, int delta)
{
    pddl_num_t a = mk_int(sign > 0 ? INT_MAX : INT_MIN);
    pddl_num_t b = mk_int_d(sign > 0 ? 1 : -1, delta);
    pddl_num_t h;
    pddlNumAddSatTo(&h, &a, &b);
    assert(pddlNumIsHuge(&h));
    return h;
}

/** Name of the status, the switch is exhaustive and has no default case so
 *  that the compiler's -Wswitch warning points here when the enum gains a
 *  value. */
static const char *st_name(pddl_num_status_t st)
{
    switch (st){
    case PDDL_NUM_OK:
        return "ok";
    case PDDL_NUM_DIV_BY_ZERO:
        return "div-by-zero";
    case PDDL_NUM_OVERFLOW:
        return "overflow";
    case PDDL_NUM_DELTA_OVERFLOW:
        return "delta-overflow";
    case PDDL_NUM_HUGE_OPERAND:
        return "huge-operand";
    case PDDL_NUM_NONZERO_DELTA:
        return "nonzero-delta";
    case PDDL_NUM_UNDEFINED:
        return "undefined";
    }
    return NULL;
}

static void assert_st(pddl_num_status_t st, pddl_num_status_t expected)
{
    if (st != expected){
        fprintf(stderr, "Status %s, expected %s\n",
                st_name(st), st_name(expected));
    }
    assert(st == expected);
}

static void assert_ok(pddl_num_status_t st)
{
    assert_st(st, PDDL_NUM_OK);
}

typedef pddl_num_status_t (*op_fn)(pddl_num_t *,
                                   const pddl_num_t *,
                                   const pddl_num_t *);

/** Checks that DST = A op B fails with EXPECTED and DST is untouched */
static void assert_fail(op_fn op, pddl_num_t a, pddl_num_t b,
                        pddl_num_status_t expected)
{
    pddl_num_t dst = mk_int_d(1234, 5);
    pddl_num_t orig = dst;
    assert_st(op(&dst, &a, &b), expected);
    assert(pddlNumExactEq(&dst, &orig));
}

/** Checks that A op B == EXPECTED (bytewise), also with DST aliasing A */
static void assert_res(op_fn op, pddl_num_t a, pddl_num_t b,
                       pddl_num_t expected)
{
    pddl_num_t dst;
    assert_ok(op(&dst, &a, &b));
    if (!pddlNumExactEq(&dst, &expected)){
        char s1[64], s2[64];
        fprintf(stderr, "Result %s, expected %s\n",
                pddlNumFmt(&dst, s1, sizeof(s1)),
                pddlNumFmt(&expected, s2, sizeof(s2)));
    }
    assert(pddlNumExactEq(&dst, &expected));

    pddl_num_t x = a;
    assert_ok(op(&x, &x, &b));
    assert(pddlNumExactEq(&x, &expected));
}

static pddl_num_status_t add_to(pddl_num_t *d, const pddl_num_t *a,
                                const pddl_num_t *b)
{
    return pddlNumAddTo(d, a, b);
}

static pddl_num_status_t sub_to(pddl_num_t *d, const pddl_num_t *a,
                                const pddl_num_t *b)
{
    return pddlNumSubTo(d, a, b);
}

static pddl_num_status_t mul_to(pddl_num_t *d, const pddl_num_t *a,
                                const pddl_num_t *b)
{
    return pddlNumMulTo(d, a, b);
}

static pddl_num_status_t div_to(pddl_num_t *d, const pddl_num_t *a,
                                const pddl_num_t *b)
{
    return pddlNumDivTo(d, a, b);
}

static pddl_num_status_t add_sat_to(pddl_num_t *d, const pddl_num_t *a,
                                    const pddl_num_t *b)
{
    pddlNumAddSatTo(d, a, b);
    return PDDL_NUM_OK;
}

TEST_ONCE(num_basic)
{
    assert(sizeof(pddl_num_t) == 8);

    pddl_num_t v = mk_int(42);
    assert(pddlNumIsInt(&v));
    assert(!pddlNumIsFlt(&v));
    assert(!pddlNumIsHuge(&v));
    assert(!pddlNumIsInf(&v));
    assert(pddlNumIsFinite(&v));
    assert(pddlNumIsPlainInt(&v));
    assert(v.val.i == 42 && v.delta == 0);

    pddl_num_t f = mk_flt(0.5f);
    assert(pddlNumIsFlt(&f));
    assert(!pddlNumIsInt(&f));
    assert(!pddlNumIsPlainInt(&f));
    assert(pddlNumIsFinite(&f));
    assert(f.val.f == 0.5f && f.delta == 0);

    // An integral float stays a float
    pddl_num_t f2 = mk_flt(2.f);
    assert(pddlNumIsFlt(&f2));

    // Negative zero is normalized to positive zero
    pddl_num_t nz = mk_flt(-0.f);
    pddl_num_t pz = mk_flt(0.f);
    assert(pddlNumExactEq(&nz, &pz));
    assert(pddlNumIsZero(&nz));

    // Infinities, canonical forms
    pddl_num_t inf, ninf;
    pddlNumSetInf(&inf);
    pddlNumSetNegInf(&ninf);
    assert(pddlNumExactEq(&inf, &pddl_num_inf));
    assert(pddlNumExactEq(&ninf, &pddl_num_neg_inf));
    assert(pddlNumIsInf(&inf) && pddlNumIsPosInf(&inf));
    assert(!pddlNumIsNegInf(&inf));
    assert(pddlNumIsInf(&ninf) && pddlNumIsNegInf(&ninf));
    assert(!pddlNumIsPosInf(&ninf));
    assert(!pddlNumIsFinite(&inf));
    assert(inf.val.i == INT_MAX && ninf.val.i == INT_MIN);
    pddl_num_t finf = mk_flt(1.f / 0.f);
    pddl_num_t fninf = mk_flt(-1.f / 0.f);
    assert(pddlNumExactEq(&finf, &pddl_num_inf));
    assert(pddlNumExactEq(&fninf, &pddl_num_neg_inf));

    // Huge values
    pddl_num_t h = mk_huge(1, 0);
    assert(pddlNumIsHuge(&h) && !pddlNumIsInf(&h) && !pddlNumIsFinite(&h));
    assert(h.val.i == INT_MAX);
    pddl_num_t nh = mk_huge(-1, 0);
    assert(pddlNumIsHuge(&nh) && nh.val.i == INT_MIN);

    // Constants
    pddl_num_t zero = mk_int(0);
    pddl_num_t one = mk_int(1);
    pddl_num_t eps = mk_int_d(0, 1);
    assert(pddlNumExactEq(&zero, &pddl_num_zero));
    assert(pddlNumExactEq(&one, &pddl_num_one));
    assert(pddlNumExactEq(&eps, &pddl_num_eps));

    // Zero/one predicates
    pddl_num_t of = mk_flt(1.f);
    assert(pddlNumIsZero(&zero) && pddlNumIsZero(&pz));
    assert(!pddlNumIsZero(&eps));
    assert(!pddlNumIsZero(&one) && !pddlNumIsZero(&f));
    assert(!pddlNumIsZero(&inf) && !pddlNumIsZero(&h));
    assert(pddlNumIsOne(&one) && pddlNumIsOne(&of));
    pddl_num_t one_d = mk_int_d(1, 1);
    assert(!pddlNumIsOne(&one_d));
    assert(!pddlNumIsOne(&zero) && !pddlNumIsOne(&f) && !pddlNumIsOne(&inf));

    // Sign predicates, lexicographic on (val, delta)
    pddl_num_t neg_eps = mk_int_d(0, -1);
    pddl_num_t fneg_eps = mk_flt_d(0.f, -1);
    pddl_num_t m1 = mk_int_d(-1, 5);
    pddl_num_t fm = mk_flt(-0.5f);
    assert(pddlNumIsPos(&eps) && !pddlNumIsNeg(&eps));
    assert(pddlNumIsNeg(&neg_eps) && !pddlNumIsPos(&neg_eps));
    assert(pddlNumIsNeg(&fneg_eps));
    assert(pddlNumIsNeg(&m1));
    assert(pddlNumIsNeg(&fm) && pddlNumIsPos(&f));
    assert(!pddlNumIsPos(&zero) && !pddlNumIsNeg(&zero));
    assert(pddlNumIsNonNeg(&zero) && pddlNumIsNonPos(&zero));
    assert(pddlNumIsNonNeg(&eps) && !pddlNumIsNonPos(&eps));
    assert(pddlNumIsPos(&inf) && pddlNumIsNeg(&ninf));
    assert(pddlNumIsPos(&h) && pddlNumIsNeg(&nh));

    // Integer-in-range predicate
    assert(pddlNumIsIntInRange(&v, 0, 42));
    assert(pddlNumIsIntInRange(&v, 42, 100));
    assert(!pddlNumIsIntInRange(&v, 0, 41));
    assert(!pddlNumIsIntInRange(&v, 43, 100));
    assert(!pddlNumIsIntInRange(&of, 0, 100));
    assert(!pddlNumIsIntInRange(&h, INT_MIN, INT_MAX));
    assert(!pddlNumIsIntInRange(&inf, INT_MIN, INT_MAX));

    // Recast of an integral float value to an integer value
    pddl_num_t rc = mk_flt_d(2.f, 3);
    assert(pddlNumCanRecastFltToInt(&rc));
    pddlNumRecastFltToInt(&rc);
    assert(pddlNumIsPlainInt(&rc) && rc.val.i == 2 && rc.delta == 3);
    rc = mk_flt(-3.f);
    assert(pddlNumCanRecastFltToInt(&rc));
    pddlNumRecastFltToInt(&rc);
    assert(pddlNumIsPlainInt(&rc) && rc.val.i == -3);
    rc = mk_flt(-2147483648.f);
    assert(pddlNumCanRecastFltToInt(&rc));
    pddlNumRecastFltToInt(&rc);
    assert(rc.val.i == INT_MIN);
    rc = mk_flt(0.5f);
    assert(!pddlNumCanRecastFltToInt(&rc));
    rc = mk_flt(2147483648.f);
    assert(!pddlNumCanRecastFltToInt(&rc));
    rc = mk_flt(1e30f);
    assert(!pddlNumCanRecastFltToInt(&rc));
    assert(!pddlNumCanRecastFltToInt(&v));
    assert(!pddlNumCanRecastFltToInt(&inf));
}

TEST_ONCE(num_arith)
{
    // int op int
    assert_res(add_to, mk_int(2), mk_int(3), mk_int(5));
    assert_res(add_to, mk_int(-7), mk_int(3), mk_int(-4));
    assert_res(sub_to, mk_int(2), mk_int(3), mk_int(-1));
    assert_res(mul_to, mk_int(-4), mk_int(3), mk_int(-12));
    assert_res(mul_to, mk_int(0), mk_int(3), mk_int(0));
    assert_res(div_to, mk_int(12), mk_int(-4), mk_int(-3));
    assert_res(div_to, mk_int(0), mk_int(5), mk_int(0));
    // Division with a remainder results in a float
    assert_res(div_to, mk_int(1), mk_int(2), mk_flt(0.5f));
    assert_res(div_to, mk_int(-3), mk_int(2), mk_flt(-1.5f));
    assert_res(div_to, mk_int(INT_MAX), mk_int(INT_MIN), mk_flt(
                (float)((double)INT_MAX / (double)INT_MIN)));
    assert_res(div_to, mk_int(INT_MIN), mk_int(1), mk_int(INT_MIN));

    // int op float and float op float
    assert_res(add_to, mk_int(2), mk_flt(0.5f), mk_flt(2.5f));
    assert_res(add_to, mk_flt(0.5f), mk_int(2), mk_flt(2.5f));
    assert_res(add_to, mk_flt(0.25f), mk_flt(0.5f), mk_flt(0.75f));
    assert_res(sub_to, mk_int(2), mk_flt(0.5f), mk_flt(1.5f));
    assert_res(sub_to, mk_flt(0.5f), mk_flt(0.5f), mk_flt(0.f));
    assert_res(mul_to, mk_int(3), mk_flt(0.5f), mk_flt(1.5f));
    assert_res(mul_to, mk_flt(-2.f), mk_flt(0.5f), mk_flt(-1.f));
    assert_res(div_to, mk_flt(3.f), mk_int(2), mk_flt(1.5f));
    assert_res(div_to, mk_int(1), mk_flt(0.5f), mk_flt(2.f));

    // A float result that is integral stays a float
    assert_res(add_to, mk_flt(1.5f), mk_flt(0.5f), mk_flt(2.f));

    // -0.0 is normalized in the results
    pddl_num_t r;
    pddl_num_t a = mk_flt(-1.f);
    pddl_num_t b = mk_int(0);
    assert_ok(pddlNumMulTo(&r, &a, &b));
    assert(pddlNumExactEq(&r, &pddl_num_zero) == 0);
    pddl_num_t fz = mk_flt(0.f);
    assert(pddlNumExactEq(&r, &fz));

    // In-place variants
    pddl_num_t x = mk_int(10);
    pddl_num_t y = mk_int(4);
    assert_ok(pddlNumAdd(&x, &y));
    assert(x.val.i == 14);
    assert_ok(pddlNumSub(&x, &y));
    assert(x.val.i == 10);
    assert_ok(pddlNumMul(&x, &y));
    assert(x.val.i == 40);
    assert_ok(pddlNumDiv(&x, &y));
    assert(x.val.i == 10);
    assert_ok(pddlNumDiv(&x, &y));
    assert(pddlNumIsFlt(&x) && x.val.f == 2.5f);

    // Aliasing of the both operands
    x = mk_int(7);
    assert_ok(pddlNumAddTo(&x, &x, &x));
    assert(x.val.i == 14);
    assert_ok(pddlNumSubTo(&x, &x, &x));
    assert(pddlNumIsZero(&x));
}

TEST_ONCE(num_delta)
{
    // Element-wise addition/subtraction
    assert_res(add_to, mk_int_d(2, 1), mk_int_d(3, 4), mk_int_d(5, 5));
    assert_res(add_to, mk_int_d(2, 1), mk_int_d(3, -4), mk_int_d(5, -3));
    assert_res(sub_to, mk_int_d(2, 1), mk_int_d(3, 4), mk_int_d(-1, -3));
    assert_res(add_to, mk_flt_d(0.5f, 1), mk_int_d(1, 2), mk_flt_d(1.5f, 3));
    assert_res(sub_to, mk_flt_d(0.5f, 1), mk_flt_d(0.5f, 1), mk_flt(0.f));
    assert_res(add_to, pddl_num_eps, pddl_num_eps, mk_int_d(0, 2));

    // Delta bounds
    assert_res(add_to, mk_int_d(0, PDDL_NUM_DELTA_MAX - 1), pddl_num_eps,
               mk_int_d(0, PDDL_NUM_DELTA_MAX));
    assert_res(sub_to, mk_int_d(0, PDDL_NUM_DELTA_MIN + 1), pddl_num_eps,
               mk_int_d(0, PDDL_NUM_DELTA_MIN));
    assert_fail(add_to, mk_int_d(0, PDDL_NUM_DELTA_MAX), pddl_num_eps,
                PDDL_NUM_DELTA_OVERFLOW);
    assert_fail(sub_to, mk_int_d(0, PDDL_NUM_DELTA_MIN), pddl_num_eps,
                PDDL_NUM_DELTA_OVERFLOW);
    assert_fail(add_to, mk_int_d(0, PDDL_NUM_DELTA_MIN),
                mk_int_d(0, PDDL_NUM_DELTA_MIN), PDDL_NUM_DELTA_OVERFLOW);
    assert_fail(sub_to, mk_int_d(0, PDDL_NUM_DELTA_MAX),
                mk_int_d(0, PDDL_NUM_DELTA_MIN), PDDL_NUM_DELTA_OVERFLOW);
    assert_fail(add_to, mk_flt_d(0.5f, PDDL_NUM_DELTA_MAX), pddl_num_eps,
                PDDL_NUM_DELTA_OVERFLOW);

    // Multiplication/division is not allowed with delta != 0
    assert_fail(mul_to, mk_int_d(2, 1), mk_int(3), PDDL_NUM_NONZERO_DELTA);
    assert_fail(mul_to, mk_int(2), mk_int_d(3, -1), PDDL_NUM_NONZERO_DELTA);
    assert_fail(mul_to, mk_flt_d(0.5f, 1), mk_int(0),
                PDDL_NUM_NONZERO_DELTA);
    assert_fail(div_to, mk_int_d(2, 1), mk_int(3), PDDL_NUM_NONZERO_DELTA);
    assert_fail(div_to, mk_int(2), mk_int_d(3, 1), PDDL_NUM_NONZERO_DELTA);
    assert_fail(div_to, pddl_num_one, pddl_num_eps, PDDL_NUM_NONZERO_DELTA);
}

TEST_ONCE(num_overflow)
{
    // Integer overflow
    assert_fail(add_to, mk_int(INT_MAX), mk_int(1), PDDL_NUM_OVERFLOW);
    assert_fail(add_to, mk_int(INT_MIN), mk_int(-1), PDDL_NUM_OVERFLOW);
    assert_fail(sub_to, mk_int(INT_MIN), mk_int(1), PDDL_NUM_OVERFLOW);
    assert_fail(sub_to, mk_int(0), mk_int(INT_MIN), PDDL_NUM_OVERFLOW);
    assert_fail(mul_to, mk_int(INT_MAX), mk_int(2), PDDL_NUM_OVERFLOW);
    assert_fail(mul_to, mk_int(INT_MIN), mk_int(-1), PDDL_NUM_OVERFLOW);
    assert_fail(mul_to, mk_int(65536), mk_int(-65536), PDDL_NUM_OVERFLOW);
    assert_fail(div_to, mk_int(INT_MIN), mk_int(-1), PDDL_NUM_OVERFLOW);

    // The bounds themselves are fine
    assert_res(add_to, mk_int(INT_MAX - 1), mk_int(1), mk_int(INT_MAX));
    assert_res(sub_to, mk_int(INT_MIN + 1), mk_int(1), mk_int(INT_MIN));
    assert_res(sub_to, mk_int(-1), mk_int(INT_MAX), mk_int(INT_MIN));
    assert_res(mul_to, mk_int(INT_MIN / 2), mk_int(2), mk_int(INT_MIN));
    assert_res(mul_to, mk_int(-1), mk_int(INT_MAX), mk_int(-INT_MAX));

    // Float overflow
    assert_fail(add_to, mk_flt(FLT_MAX), mk_flt(FLT_MAX), PDDL_NUM_OVERFLOW);
    assert_fail(sub_to, mk_flt(-FLT_MAX), mk_flt(FLT_MAX),
                PDDL_NUM_OVERFLOW);
    assert_fail(mul_to, mk_flt(1e30f), mk_flt(1e30f), PDDL_NUM_OVERFLOW);
    assert_fail(mul_to, mk_flt(1e30f), mk_int(-1000000000),
                PDDL_NUM_OVERFLOW);
    assert_fail(div_to, mk_flt(1e30f), mk_flt(1e-30f), PDDL_NUM_OVERFLOW);
    assert_res(mul_to, mk_flt(FLT_MAX), mk_int(1), mk_flt(FLT_MAX));

    // Division by zero
    assert_fail(div_to, mk_int(1), mk_int(0), PDDL_NUM_DIV_BY_ZERO);
    assert_fail(div_to, mk_int(0), mk_int(0), PDDL_NUM_DIV_BY_ZERO);
    assert_fail(div_to, mk_flt(1.5f), mk_int(0), PDDL_NUM_DIV_BY_ZERO);
    assert_fail(div_to, mk_int(1), mk_flt(0.f), PDDL_NUM_DIV_BY_ZERO);
    assert_fail(div_to, pddl_num_inf, mk_int(0), PDDL_NUM_DIV_BY_ZERO);
}

TEST_ONCE(num_inf)
{
    pddl_num_t inf = pddl_num_inf;
    pddl_num_t ninf = pddl_num_neg_inf;

    // Addition/subtraction; delta of the finite operand is dropped
    assert_res(add_to, inf, mk_int_d(5, 3), inf);
    assert_res(add_to, mk_flt(-1.5f), inf, inf);
    assert_res(add_to, ninf, mk_int(INT_MAX), ninf);
    assert_res(add_to, inf, inf, inf);
    assert_res(add_to, ninf, ninf, ninf);
    assert_res(sub_to, inf, mk_int(5), inf);
    assert_res(sub_to, mk_int(5), inf, ninf);
    assert_res(sub_to, mk_int(5), ninf, inf);
    assert_res(sub_to, inf, ninf, inf);
    assert_res(sub_to, ninf, inf, ninf);
    assert_fail(add_to, inf, ninf, PDDL_NUM_UNDEFINED);
    assert_fail(add_to, ninf, inf, PDDL_NUM_UNDEFINED);
    assert_fail(sub_to, inf, inf, PDDL_NUM_UNDEFINED);
    assert_fail(sub_to, ninf, ninf, PDDL_NUM_UNDEFINED);

    // Multiplication
    assert_res(mul_to, inf, mk_int(2), inf);
    assert_res(mul_to, inf, mk_int(-2), ninf);
    assert_res(mul_to, mk_flt(-0.5f), ninf, inf);
    assert_res(mul_to, inf, ninf, ninf);
    assert_res(mul_to, ninf, ninf, inf);
    assert_fail(mul_to, inf, mk_int(0), PDDL_NUM_UNDEFINED);
    assert_fail(mul_to, mk_flt(0.f), ninf, PDDL_NUM_UNDEFINED);

    // Division
    assert_res(div_to, mk_int(5), inf, mk_int(0));
    assert_res(div_to, mk_flt(-5.f), ninf, mk_int(0));
    assert_res(div_to, inf, mk_int(-3), ninf);
    assert_res(div_to, ninf, mk_flt(-0.5f), inf);
    assert_fail(div_to, inf, inf, PDDL_NUM_UNDEFINED);
    assert_fail(div_to, inf, ninf, PDDL_NUM_UNDEFINED);

    // Saturated addition
    assert_res(add_sat_to, inf, mk_int_d(-5, 3), inf);
    assert_res(add_sat_to, mk_huge(-1, 2), inf, inf);
    assert_res(add_sat_to, ninf, mk_huge(1, 0), ninf);
    assert_res(add_sat_to, ninf, ninf, ninf);
    assert_res(add_sat_to, inf, mk_int_d(0, PDDL_NUM_DELTA_MAX), inf);
}

TEST_ONCE(num_huge)
{
    pddl_num_t h = mk_huge(1, 0);
    pddl_num_t nh = mk_huge(-1, 0);

    // Non-saturating operations fail with a huge operand
    assert_fail(add_to, h, mk_int(0), PDDL_NUM_HUGE_OPERAND);
    assert_fail(add_to, mk_int(1), nh, PDDL_NUM_HUGE_OPERAND);
    assert_fail(add_to, h, pddl_num_inf, PDDL_NUM_HUGE_OPERAND);
    assert_fail(sub_to, h, h, PDDL_NUM_HUGE_OPERAND);
    assert_fail(sub_to, mk_flt(0.5f), nh, PDDL_NUM_HUGE_OPERAND);
    assert_fail(mul_to, h, mk_int(1), PDDL_NUM_HUGE_OPERAND);
    assert_fail(mul_to, mk_int(0), nh, PDDL_NUM_HUGE_OPERAND);
    assert_fail(div_to, h, mk_int(1), PDDL_NUM_HUGE_OPERAND);
    assert_fail(div_to, mk_int(1), nh, PDDL_NUM_HUGE_OPERAND);
    assert_fail(div_to, h, mk_int(0), PDDL_NUM_HUGE_OPERAND);

    // Saturation in both directions, delta is summed
    assert_res(add_sat_to, mk_int_d(INT_MAX, 1), mk_int_d(1, 2), mk_huge(1, 3));
    assert_res(add_sat_to, mk_int(INT_MIN), mk_int_d(-1, -2),
               mk_huge(-1, -2));
    assert_res(add_sat_to, mk_int(INT_MAX), mk_int(INT_MAX), mk_huge(1, 0));
    assert_res(add_sat_to, mk_int(INT_MIN), mk_int(INT_MIN), mk_huge(-1, 0));

    // No saturation if the result fits
    assert_res(add_sat_to, mk_int_d(INT_MAX, 1), mk_int_d(-1, 1),
               mk_int_d(INT_MAX - 1, 2));
    assert_res(add_sat_to, mk_int(INT_MAX - 1), mk_int(1), mk_int(INT_MAX));
    assert_res(add_sat_to, mk_flt_d(0.5f, 1), mk_int(2), mk_flt_d(2.5f, 1));

    // Huge + value of the same sign or zero value stays huge
    assert_res(add_sat_to, h, mk_int_d(5, 2), mk_huge(1, 2));
    assert_res(add_sat_to, mk_flt(0.5f), h, mk_huge(1, 0));
    assert_res(add_sat_to, h, mk_int_d(0, -3), mk_huge(1, -3));
    assert_res(add_sat_to, h, mk_flt(0.f), h);
    assert_res(add_sat_to, h, h, h);
    assert_res(add_sat_to, mk_huge(1, 1), mk_huge(1, 2), mk_huge(1, 3));
    assert_res(add_sat_to, nh, mk_int(-5), nh);
    assert_res(add_sat_to, nh, mk_flt(-5.f), nh);
    assert_res(add_sat_to, nh, nh, nh);
    assert_res(add_sat_to, pddl_num_zero, nh, nh);

    // In-place variant
    pddl_num_t x = mk_int(INT_MAX);
    pddlNumAddSat(&x, &pddl_num_one);
    assert(pddlNumExactEq(&x, &h));
    pddlNumAddSat(&x, &pddl_num_eps);
    assert(pddlNumIsHuge(&x) && x.delta == 1);
}

/** Checks all comparison functions on the pair A, B against the expected
 *  result of the comparison EXP (-1/0/1) */
static void check_cmp(const pddl_num_t *a, const pddl_num_t *b, int exp)
{
    int cmp = pddlNumCmp(a, b);
    if (cmp != exp){
        char s1[64], s2[64];
        fprintf(stderr, "Cmp(%s, %s) = %d, expected %d\n",
                pddlNumFmt(a, s1, sizeof(s1)),
                pddlNumFmt(b, s2, sizeof(s2)), cmp, exp);
    }
    assert(cmp == exp);
    assert(__pddlNumCmpGen(a, b) == exp);
    assert(pddlNumLT(a, b) == (exp < 0));
    assert(pddlNumLE(a, b) == (exp <= 0));
    assert(pddlNumGT(a, b) == (exp > 0));
    assert(pddlNumGE(a, b) == (exp >= 0));
    assert(pddlNumEq(a, b) == (exp == 0));
}

TEST_ONCE(num_cmp)
{
    // Strictly increasing sequence; equal values are in the same group
    pddl_num_t seq[][2] = {
        { pddl_num_neg_inf, pddl_num_neg_inf },
        { mk_huge(-1, -1), mk_huge(-1, -1) },
        { mk_huge(-1, 0), mk_huge(-1, 0) },
        { mk_huge(-1, 5), mk_huge(-1, 5) },
        { mk_flt(-1e20f), mk_flt(-1e20f) },
        { mk_int(INT_MIN), mk_int(INT_MIN) },
        { mk_int_d(INT_MIN, 1), mk_int_d(INT_MIN, 1) },
        { mk_int(-5), mk_flt(-5.f) },
        { mk_int_d(-5, 1), mk_flt_d(-5.f, 1) },
        { mk_flt(-0.5f), mk_flt(-0.5f) },
        { mk_int_d(0, -1), mk_flt_d(0.f, -1) },
        { mk_int(0), mk_flt(-0.f) },
        { pddl_num_eps, mk_flt_d(0.f, 1) },
        { mk_flt(1e-30f), mk_flt(1e-30f) },
        { mk_int(2), mk_flt(2.f) },
        { mk_flt(2.5f), mk_flt(2.5f) },
        { mk_int(16777217), mk_int(16777217) },
        { mk_flt(16777218.f), mk_flt(16777218.f) },
        { mk_int_d(INT_MAX, -1), mk_int_d(INT_MAX, -1) },
        { mk_int(INT_MAX), mk_int(INT_MAX) },
        { mk_flt(1e20f), mk_flt(1e20f) },
        { mk_huge(1, -3), mk_huge(1, -3) },
        { mk_huge(1, 0), mk_huge(1, 0) },
        { pddl_num_inf, pddl_num_inf },
    };
    int size = sizeof(seq) / sizeof(seq[0]);
    for (int i = 0; i < size; ++i){
        for (int j = 0; j < size; ++j){
            int exp = (i > j) - (i < j);
            for (int k = 0; k < 2; ++k){
                for (int l = 0; l < 2; ++l)
                    check_cmp(&seq[i][k], &seq[j][l], exp);
            }
        }
    }

    // Numeric vs exact equality
    pddl_num_t i2 = mk_int(2);
    pddl_num_t f2 = mk_flt(2.f);
    assert(pddlNumEq(&i2, &f2));
    assert(!pddlNumExactEq(&i2, &f2));
    assert(pddlNumExactEq(&i2, &i2));

    // Huge values of the same sign with the same delta are equal
    pddl_num_t h1 = mk_huge(1, 2);
    pddl_num_t h2 = mk_int_d(INT_MAX, 1);
    pddlNumAddSat(&h2, &h1);
    pddlNumAddSat(&h2, &pddl_num_neg_inf);
    assert(pddlNumIsNegInf(&h2));
    h2 = mk_huge(1, 1);
    pddlNumAddSat(&h2, &pddl_num_eps);
    assert(pddlNumEq(&h1, &h2) && pddlNumCmp(&h1, &h2) == 0);

    // INT_MAX is not +inf and not huge
    pddl_num_t imax = mk_int(INT_MAX);
    assert(!pddlNumEq(&imax, &pddl_num_inf));
    assert(!pddlNumEq(&imax, &h1));
}

TEST_ONCE(num_hash_fmt)
{
    char s[64];
    struct {
        pddl_num_t v;
        const char *str;
    } fmt[] = {
        { mk_int(5), "5" },
        { mk_int(-5), "-5" },
        { mk_flt(2.5f), "2.5" },
        { mk_int_d(5, 3), "5+3e" },
        { mk_int_d(5, -2), "5-2e" },
        { mk_flt_d(-0.5f, 1), "-0.5+1e" },
        { pddl_num_eps, "0+1e" },
        { pddl_num_inf, "inf" },
        { pddl_num_neg_inf, "-inf" },
        { mk_huge(1, 0), "huge" },
        { mk_huge(-1, 0), "-huge" },
        { mk_huge(1, 1), "huge+1e" },
        { mk_huge(-1, -7), "-huge-7e" },
    };
    for (size_t i = 0; i < sizeof(fmt) / sizeof(fmt[0]); ++i){
        pddlNumFmt(&fmt[i].v, s, sizeof(s));
        if (strcmp(s, fmt[i].str) != 0)
            fprintf(stderr, "Fmt: '%s', expected '%s'\n", s, fmt[i].str);
        assert(strcmp(s, fmt[i].str) == 0);
    }

    // Truncation of the output
    char s4[4];
    pddl_num_t v = mk_int_d(12345, 3);
    pddlNumFmt(&v, s4, sizeof(s4));
    assert(strcmp(s4, "123") == 0);
    char s6[6];
    pddlNumFmt(&v, s6, sizeof(s6));
    assert(strcmp(s6, "12345") == 0);
    char s7[7];
    pddlNumFmt(&v, s7, sizeof(s7));
    assert(strcmp(s7, "12345+") == 0);

    // Hash is consistent with ExactEq
    pddl_num_t a = mk_int_d(7, 2);
    pddl_num_t b;
    assert_ok(pddlNumAddTo(&b, &pddl_num_zero, &a));
    assert(pddlNumExactEq(&a, &b));
    assert(pddlNumHash(&a) == pddlNumHash(&b));
    pddl_num_t nz = mk_flt(-0.f);
    pddl_num_t pz = mk_flt(0.f);
    assert(pddlNumHash(&nz) == pddlNumHash(&pz));
    pddl_num_t c = mk_int_d(7, 3);
    assert(!pddlNumExactEq(&a, &c));
    assert(pddlNumHash(&a) != pddlNumHash(&c));

    // Arrays
    pddl_num_t arr[4] = { mk_int(1), mk_flt_d(0.5f, 1), pddl_num_inf,
                          mk_huge(-1, 0) };
    pddl_num_t *cl = pddlNumArrClone(arr, 4);
    assert(pddlNumArrExactEq(arr, cl, 4));
    assert(pddlNumArrHash(arr, 4) == pddlNumArrHash(cl, 4));
    assert(pddlNumArrHash(arr, 1) == pddlNumHash(arr));
    cl[1] = mk_flt(0.5f);
    assert(!pddlNumArrExactEq(arr, cl, 4));
    assert(pddlNumArrHash(arr, 4) != pddlNumArrHash(cl, 4));
    assert(pddlNumArrExactEq(arr, cl, 1));
    assert(pddlNumArrExactEq(arr, cl, 0));
    assert(pddlNumArrHash(arr, 0) == 0);
    assert(pddlNumArrClone(arr, 0) == NULL);
    pddlNumArrFree(cl, 4);
    pddlNumArrFree(NULL, 0);
}

TEST_PANIC_ONCE(num_panic_addsat_huge_opposite)
{
    pddl_num_t a = mk_huge(1, 0);
    pddl_num_t b = mk_huge(-1, 0);
    pddlNumAddSat(&a, &b);
}

TEST_PANIC_ONCE(num_panic_addsat_huge_neg)
{
    pddl_num_t a = mk_huge(1, 0);
    pddl_num_t b = mk_int(-1);
    pddlNumAddSat(&a, &b);
}

TEST_PANIC_ONCE(num_panic_addsat_neg_huge_flt)
{
    pddl_num_t a = mk_flt(0.5f);
    pddl_num_t b = mk_huge(-1, 0);
    pddlNumAddSat(&a, &b);
}

TEST_PANIC_ONCE(num_panic_addsat_inf_opposite)
{
    pddl_num_t a = pddl_num_inf;
    pddlNumAddSat(&a, &pddl_num_neg_inf);
}

TEST_PANIC_ONCE(num_panic_addsat_delta)
{
    pddl_num_t a = mk_int_d(1, PDDL_NUM_DELTA_MAX);
    pddlNumAddSat(&a, &pddl_num_eps);
}

TEST_PANIC_ONCE(num_panic_addsat_flt_overflow)
{
    pddl_num_t a = mk_flt(FLT_MAX);
    pddl_num_t b = mk_flt(FLT_MAX);
    pddlNumAddSat(&a, &b);
}
