/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests for the pddl_num_val_t numeric value wrapper.
 *
 * All tests are TEST_ONCE (not per task).
 * Run with:  cd tests && make && ./test -T _ -s num_val
 *
 * Overflow of the integer arithmetic and division by zero are reported
 * via the pddl_num_val_status_t return value and leave the destination
 * untouched.
 */

#include "pddl/num_val.h"
#include "test.h"
#include "context.h"
#include <assert.h>
#include <string.h>

static pddl_num_val_t mk_int(int64_t val)
{
    pddl_num_val_t v;
    pddlNumValSetInt(&v, val);
    return v;
}

static pddl_num_val_t mk_flt(double val)
{
    pddl_num_val_t v;
    pddlNumValSetFlt(&v, val);
    return v;
}

/** Checks that the status of an arithmetic operation is PDDL_NUM_VAL_OK.
 *  The switch is exhaustive and has no default case so that the
 *  compiler's -Wswitch warning points here when the enum gains a value. */
static void assert_ok(pddl_num_val_status_t st)
{
    switch (st){
    case PDDL_NUM_VAL_OK:
        break;
    case PDDL_NUM_VAL_DIV_BY_ZERO:
        assert(st == PDDL_NUM_VAL_OK);
        break;
    case PDDL_NUM_VAL_OVERFLOW:
        assert(st == PDDL_NUM_VAL_OK);
        break;
    }
}

TEST_ONCE(num_val_basic)
{
    pddl_num_val_t v = mk_int(42);
    assert(pddlNumValIsInt(&v));
    assert(!pddlNumValIsFlt(&v));
    assert(v.type == PDDL_NUM_VAL_INT);
    assert(v.v.i == 42);

    pddl_num_val_t f = mk_flt(0.5);
    assert(pddlNumValIsFlt(&f));
    assert(!pddlNumValIsInt(&f));
    assert(f.type == PDDL_NUM_VAL_FLT);
    assert(f.v.f == 0.5);

    // An integral float stays a float
    pddl_num_val_t f2 = mk_flt(2.0);
    assert(pddlNumValIsFlt(&f2));
    assert(f2.v.f == 2.0);

    // Negative zero is normalized to positive zero
    pddl_num_val_t nz = mk_flt(-0.0);
    pddl_num_val_t pz = mk_flt(0.0);
    assert(pddlNumValIsFlt(&nz));
    assert(pddlNumValEq(&nz, &pz));

    // Zero/one predicates (value-based, both variants)
    pddl_num_val_t zi = mk_int(0);
    pddl_num_val_t zf = mk_flt(0.0);
    pddl_num_val_t oi = mk_int(1);
    pddl_num_val_t of = mk_flt(1.0);
    assert(pddlNumValIsZero(&zi));
    assert(pddlNumValIsZero(&zf));
    assert(!pddlNumValIsZero(&oi));
    assert(!pddlNumValIsZero(&f));
    assert(pddlNumValIsOne(&oi));
    assert(pddlNumValIsOne(&of));
    assert(!pddlNumValIsOne(&zi));
    assert(!pddlNumValIsOne(&f));

    // Integer-in-range predicate (closed range, ints only)
    assert(pddlNumValIsIntInRange(&v, 0, 42));
    assert(pddlNumValIsIntInRange(&v, 42, 100));
    assert(!pddlNumValIsIntInRange(&v, 0, 41));
    assert(!pddlNumValIsIntInRange(&v, 43, 100));
    assert(pddlNumValIsIntInRange(&zi, INT64_MIN, INT64_MAX));
    assert(!pddlNumValIsIntInRange(&of, 0, 100));

    // Recast of an integral float value to an integer value
    pddl_num_val_t rc = mk_flt(2.0);
    assert(pddlNumValCanRecastFltToInt(&rc));
    pddlNumValRecastFltToInt(&rc);
    assert(pddlNumValIsInt(&rc) && rc.v.i == 2);
    rc = mk_flt(-3.0);
    assert(pddlNumValCanRecastFltToInt(&rc));
    pddlNumValRecastFltToInt(&rc);
    assert(pddlNumValIsInt(&rc) && rc.v.i == -3);
    // Non-integral, out-of-range, and integer values cannot be recast
    rc = mk_flt(0.5);
    assert(!pddlNumValCanRecastFltToInt(&rc));
    rc = mk_flt(1e300);
    assert(!pddlNumValCanRecastFltToInt(&rc));
    rc = mk_flt(1. / 0.);
    assert(!pddlNumValCanRecastFltToInt(&rc));
    rc = mk_int(2);
    assert(!pddlNumValCanRecastFltToInt(&rc));

    // Copy of both variants
    pddl_num_val_t cp;
    pddlNumValSet(&cp, &v);
    assert(pddlNumValIsInt(&cp));
    assert(cp.v.i == 42);
    assert(pddlNumValEq(&cp, &v));
    pddlNumValSet(&cp, &f);
    assert(pddlNumValIsFlt(&cp));
    assert(cp.v.f == 0.5);
    assert(pddlNumValEq(&cp, &f));

    // Formatting
    char buf[32];
    pddl_num_val_t neg = mk_int(-7);
    assert(strcmp(pddlNumValFmt(&v, buf, sizeof(buf)), "42") == 0);
    assert(strcmp(pddlNumValFmt(&neg, buf, sizeof(buf)), "-7") == 0);
    assert(strcmp(pddlNumValFmt(&f, buf, sizeof(buf)), "0.5") == 0);
    assert(strcmp(pddlNumValFmt(&f2, buf, sizeof(buf)), "2") == 0);
    // Truncation respects the buffer size
    char small[3];
    pddl_num_val_t big = mk_int(12345);
    assert(strcmp(pddlNumValFmt(&big, small, sizeof(small)), "12") == 0);

    // Predefined constants
    assert(pddlNumValIsInt(&pddl_num_val_zero));
    assert(pddlNumValIsZero(&pddl_num_val_zero));
    assert(pddlNumValIsInt(&pddl_num_val_one));
    assert(pddlNumValIsOne(&pddl_num_val_one));

    // Arrays
    pddl_num_val_t arr[3];
    arr[0] = mk_int(1);
    arr[1] = mk_flt(2.5);
    arr[2] = mk_int(-3);
    pddl_num_val_t *clone = pddlNumValArrClone(arr, 3);
    assert(clone != NULL);
    assert(pddlNumValArrEq(arr, clone, 3));
    assert(pddlNumValArrHash(arr, 3) == pddlNumValArrHash(clone, 3));

    clone[1] = mk_int(2);
    assert(!pddlNumValArrEq(arr, clone, 3));
    assert(pddlNumValArrHash(arr, 3) != pddlNumValArrHash(clone, 3));
    pddlNumValArrFree(clone, 3);

    // Empty arrays
    assert(pddlNumValArrClone(arr, 0) == NULL);
    assert(pddlNumValArrEq(NULL, NULL, 0));
    assert(pddlNumValArrHash(NULL, 0) == 0);
}

TEST_ONCE(num_val_arith)
{
    // Integer arithmetic stays integer
    pddl_num_val_t a = mk_int(10);
    pddl_num_val_t b = mk_int(3);
    pddl_num_val_t r;
    assert_ok(pddlNumValAddTo(&r, &a, &b));
    assert(pddlNumValIsInt(&r) && r.v.i == 13);
    assert_ok(pddlNumValSubTo(&r, &a, &b));
    assert(pddlNumValIsInt(&r) && r.v.i == 7);
    assert_ok(pddlNumValMulTo(&r, &a, &b));
    assert(pddlNumValIsInt(&r) && r.v.i == 30);

    // Non-overflowing boundary cases
    pddl_num_val_t vmax = mk_int(INT64_MAX);
    pddl_num_val_t vmin = mk_int(INT64_MIN);
    pddl_num_val_t zero = mk_int(0);
    pddl_num_val_t one = mk_int(1);
    assert_ok(pddlNumValAddTo(&r, &vmax, &zero));
    assert(pddlNumValIsInt(&r) && r.v.i == INT64_MAX);
    assert_ok(pddlNumValAddTo(&r, &vmin, &vmax));
    assert(pddlNumValIsInt(&r) && r.v.i == -1);
    assert_ok(pddlNumValMulTo(&r, &vmax, &one));
    assert(pddlNumValIsInt(&r) && r.v.i == INT64_MAX);
    assert_ok(pddlNumValSubTo(&r, &vmax, &vmax));
    assert(pddlNumValIsInt(&r) && r.v.i == 0);

    // Float contagion: any float operand makes the result float, and it
    // stays float even when the value is integral
    pddl_num_val_t fa = mk_flt(2.5);
    pddl_num_val_t fb = mk_flt(1.5);
    pddl_num_val_t four = mk_int(4);
    assert_ok(pddlNumValAddTo(&r, &fa, &fb));
    assert(pddlNumValIsFlt(&r) && r.v.f == 4.0);
    assert(pddlNumValCmp(&r, &four) == 0);
    assert_ok(pddlNumValAddTo(&r, &a, &fa));
    assert(pddlNumValIsFlt(&r) && r.v.f == 12.5);
    assert_ok(pddlNumValAddTo(&r, &fa, &a));
    assert(pddlNumValIsFlt(&r) && r.v.f == 12.5);
    assert_ok(pddlNumValSubTo(&r, &fa, &a));
    assert(pddlNumValIsFlt(&r) && r.v.f == -7.5);
    assert_ok(pddlNumValMulTo(&r, &fa, &b));
    assert(pddlNumValIsFlt(&r) && r.v.f == 7.5);

    // Aliasing of dst with the operands
    pddl_num_val_t x = mk_int(5);
    pddl_num_val_t y = mk_int(2);
    assert_ok(pddlNumValAddTo(&x, &x, &y));
    assert(pddlNumValIsInt(&x) && x.v.i == 7);
    assert_ok(pddlNumValSubTo(&x, &y, &x));
    assert(pddlNumValIsInt(&x) && x.v.i == -5);
    assert_ok(pddlNumValMulTo(&x, &x, &x));
    assert(pddlNumValIsInt(&x) && x.v.i == 25);

    // In-place variants
    x = mk_int(5);
    assert_ok(pddlNumValAdd(&x, &y));
    assert(pddlNumValIsInt(&x) && x.v.i == 7);
    assert_ok(pddlNumValSub(&x, &y));
    assert(pddlNumValIsInt(&x) && x.v.i == 5);
    assert_ok(pddlNumValMul(&x, &y));
    assert(pddlNumValIsInt(&x) && x.v.i == 10);
    assert_ok(pddlNumValDiv(&x, &y));
    assert(pddlNumValIsInt(&x) && x.v.i == 5);
    x = mk_flt(1.5);
    assert_ok(pddlNumValAdd(&x, &y));
    assert(pddlNumValIsFlt(&x) && x.v.f == 3.5);

    // Division: exact integer division stays integer
    pddl_num_val_t six = mk_int(6);
    pddl_num_val_t three = mk_int(3);
    assert_ok(pddlNumValDivTo(&r, &six, &three));
    assert(pddlNumValIsInt(&r) && r.v.i == 2);
    // Non-exact integer division becomes float
    assert_ok(pddlNumValDivTo(&r, &one, &three));
    assert(pddlNumValIsFlt(&r));
    pddl_num_val_t third = mk_flt(1.0 / 3.0);
    assert(pddlNumValCmp(&r, &third) == 0);
    // Float division
    pddl_num_val_t half = mk_flt(0.5);
    assert_ok(pddlNumValDivTo(&r, &fa, &half));
    assert(pddlNumValIsFlt(&r) && r.v.f == 5.0);
    // Division by zero fails and leaves the destination untouched
    pddl_num_val_t saved = mk_int(99);
    pddlNumValSet(&r, &saved);
    pddl_num_val_t fzero = mk_flt(0.0);
    pddl_num_val_status_t st = pddlNumValDivTo(&r, &six, &zero);
    assert(st == PDDL_NUM_VAL_DIV_BY_ZERO);
    assert(pddlNumValEq(&r, &saved));
    st = pddlNumValDivTo(&r, &fa, &fzero);
    assert(st == PDDL_NUM_VAL_DIV_BY_ZERO);
    assert(pddlNumValEq(&r, &saved));
    st = pddlNumValDiv(&r, &zero);
    assert(st == PDDL_NUM_VAL_DIV_BY_ZERO);
    assert(pddlNumValEq(&r, &saved));
}

TEST_ONCE(num_val_cmp_hash)
{
    pddl_num_val_t i1 = mk_int(1);
    pddl_num_val_t i2 = mk_int(2);
    pddl_num_val_t f15 = mk_flt(1.5);
    pddl_num_val_t f2 = mk_flt(2.0);

    // Total order on mixed values
    assert(pddlNumValCmp(&i1, &f15) < 0);
    assert(pddlNumValCmp(&f15, &i2) < 0);
    assert(pddlNumValCmp(&i1, &i2) < 0);
    assert(pddlNumValCmp(&i2, &i1) > 0);
    assert(pddlNumValCmp(&i1, &i1) == 0);
    assert(pddlNumValCmp(&f15, &f15) == 0);
    // Int and float compare by value
    assert(pddlNumValCmp(&i2, &f2) == 0);
    assert(pddlNumValCmp(&f2, &i2) == 0);
    // Symmetry
    assert(pddlNumValCmp(&i1, &f15) == -pddlNumValCmp(&f15, &i1));

    // Eq is identity, not numeric equality
    pddl_num_val_t i2b = mk_int(2);
    assert(pddlNumValEq(&i2, &i2b));
    assert(!pddlNumValEq(&i2, &f2));
    assert(!pddlNumValEq(&i1, &i2));
    assert(!pddlNumValEq(&f15, &f2));

    // Hash is consistent with Eq
    assert(pddlNumValHash(&i2) == pddlNumValHash(&i2b));
    pddl_num_val_t nz = mk_flt(-0.0);
    pddl_num_val_t pz = mk_flt(0.0);
    assert(pddlNumValEq(&nz, &pz));
    assert(pddlNumValHash(&nz) == pddlNumValHash(&pz));
    // Distinct values give distinct hashes (spot checks)
    assert(pddlNumValHash(&i1) != pddlNumValHash(&i2));
    assert(pddlNumValHash(&i2) != pddlNumValHash(&f2));
    assert(pddlNumValHash(&f15) != pddlNumValHash(&f2));
}

TEST_ONCE(num_val_overflow)
{
    // Overflowing integer arithmetic reports PDDL_NUM_VAL_OVERFLOW and
    // leaves the destination untouched
    pddl_num_val_t vmax = mk_int(INT64_MAX);
    pddl_num_val_t vmin = mk_int(INT64_MIN);
    pddl_num_val_t one = mk_int(1);
    pddl_num_val_t two = mk_int(2);
    pddl_num_val_t neg_one = mk_int(-1);
    pddl_num_val_t saved = mk_int(99);
    pddl_num_val_t r;
    pddlNumValSet(&r, &saved);

    pddl_num_val_status_t st = pddlNumValAddTo(&r, &vmax, &one);
    assert(st == PDDL_NUM_VAL_OVERFLOW);
    assert(pddlNumValEq(&r, &saved));

    st = pddlNumValSubTo(&r, &vmin, &one);
    assert(st == PDDL_NUM_VAL_OVERFLOW);
    assert(pddlNumValEq(&r, &saved));

    st = pddlNumValMulTo(&r, &vmax, &two);
    assert(st == PDDL_NUM_VAL_OVERFLOW);
    assert(pddlNumValEq(&r, &saved));

    st = pddlNumValDivTo(&r, &vmin, &neg_one);
    assert(st == PDDL_NUM_VAL_OVERFLOW);
    assert(pddlNumValEq(&r, &saved));

    // In-place variants report the overflow and leave X untouched as well
    pddl_num_val_t x = mk_int(INT64_MAX);
    st = pddlNumValAdd(&x, &one);
    assert(st == PDDL_NUM_VAL_OVERFLOW);
    assert(pddlNumValIsInt(&x) && x.v.i == INT64_MAX);
    x = mk_int(INT64_MIN);
    st = pddlNumValSub(&x, &one);
    assert(st == PDDL_NUM_VAL_OVERFLOW);
    assert(pddlNumValIsInt(&x) && x.v.i == INT64_MIN);
    x = mk_int(INT64_MAX);
    st = pddlNumValMul(&x, &two);
    assert(st == PDDL_NUM_VAL_OVERFLOW);
    assert(pddlNumValIsInt(&x) && x.v.i == INT64_MAX);
    x = mk_int(INT64_MIN);
    st = pddlNumValDiv(&x, &neg_one);
    assert(st == PDDL_NUM_VAL_OVERFLOW);
    assert(pddlNumValIsInt(&x) && x.v.i == INT64_MIN);

    // Float arithmetic never reports overflow
    pddl_num_val_t fbig = mk_flt(1e308);
    assert_ok(pddlNumValAddTo(&r, &fbig, &fbig));
    assert(pddlNumValIsFlt(&r));
}

TEST_ONCE(num_val_no_overflow)
{
    // Arithmetic that stays in range succeeds
    pddl_num_val_t r, a = mk_int(10), b = mk_int(3);
    assert_ok(pddlNumValAddTo(&r, &a, &b));
    assert(pddlNumValIsInt(&r) && r.v.i == 13);
}
