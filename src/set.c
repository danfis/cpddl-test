/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/iset.h"
#include "pddl/ivec.h"
#include "pddl/rand.h"
#include "test.h"
#include <assert.h>

/* Check that .s[] is strictly ascending. */
static void assertSorted(const pddl_iset_t *s)
{
    for (int i = 1; i < pddlISetSize(s); ++i)
        assert(s->s[i - 1] < s->s[i]);
}

/* Check that s contains exactly the given values (in sorted order). */
static void checkSet(const pddl_iset_t *s, const int *vals, int n)
{
    assert(pddlISetSize(s) == n);
    for (int i = 0; i < n; ++i)
        assert(pddlISetGet(s, i) == vals[i]);
    assertSorted(s);
}

TEST_ONCE(iset_init_free)
{
    pddl_iset_t s;
    pddlISetInit(&s);
    assert(pddlISetSize(&s) == 0);
    pddlISetFree(&s);
}

TEST_ONCE(iset_add)
{
    pddl_iset_t s;
    pddlISetInit(&s);

    /* add in order */
    pddlISetAdd(&s, 1);
    pddlISetAdd(&s, 3);
    pddlISetAdd(&s, 5);
    assert(pddlISetSize(&s) == 3);
    assert(pddlISetGet(&s, 0) == 1);
    assert(pddlISetGet(&s, 1) == 3);
    assert(pddlISetGet(&s, 2) == 5);

    /* add duplicate -- size must not change */
    pddlISetAdd(&s, 3);
    assert(pddlISetSize(&s) == 3);
    assertSorted(&s);

    /* add out-of-order (inserts before existing elements) */
    pddlISetAdd(&s, 2);
    assert(pddlISetSize(&s) == 4);
    assert(pddlISetGet(&s, 0) == 1);
    assert(pddlISetGet(&s, 1) == 2);
    assert(pddlISetGet(&s, 2) == 3);
    assert(pddlISetGet(&s, 3) == 5);
    assertSorted(&s);

    /* add element smaller than all existing */
    pddlISetAdd(&s, 0);
    assert(pddlISetSize(&s) == 5);
    assert(pddlISetGet(&s, 0) == 0);
    assertSorted(&s);

    pddlISetFree(&s);
}

TEST_ONCE(iset_has_in)
{
    pddl_iset_t s;
    pddlISetInit(&s);
    PDDL_ISET_ADD(&s, 10, 20, 30, 40, 50);
    assertSorted(&s);

    assert(pddlISetHas(&s, 10));
    assert(pddlISetHas(&s, 30));
    assert(pddlISetHas(&s, 50));
    assert(!pddlISetHas(&s, 0));
    assert(!pddlISetHas(&s, 15));
    assert(!pddlISetHas(&s, 100));

    assert(pddlISetIn(20, &s));
    assert(!pddlISetIn(21, &s));

    pddlISetFree(&s);
}

TEST_ONCE(iset_remove)
{
    pddl_iset_t s;
    pddlISetInit(&s);
    PDDL_ISET_ADD(&s, 1, 2, 3, 4, 5);
    assertSorted(&s);

    /* remove existing element */
    assert(pddlISetRemove(&s, 3));
    assert(pddlISetSize(&s) == 4);
    assert(!pddlISetHas(&s, 3));
    assertSorted(&s);

    /* remove non-existing element */
    assert(!pddlISetRemove(&s, 99));
    assert(pddlISetSize(&s) == 4);

    /* remove first element */
    assert(pddlISetRemove(&s, 1));
    assert(pddlISetGet(&s, 0) == 2);
    assertSorted(&s);

    /* remove last element */
    assert(pddlISetRemove(&s, 5));
    assert(pddlISetGet(&s, pddlISetSize(&s) - 1) == 4);
    assertSorted(&s);

    pddlISetFree(&s);
}

TEST_ONCE(iset_remove_ge)
{
    pddl_iset_t s;
    pddlISetInit(&s);
    PDDL_ISET_ADD(&s, 1, 2, 3, 4, 5);

    pddlISetRemoveGE(&s, 4);
    assert(pddlISetSize(&s) == 3);
    assert(pddlISetGet(&s, 2) == 3);
    assertSorted(&s);

    pddlISetRemoveGE(&s, 0);
    assert(pddlISetSize(&s) == 0);

    pddlISetFree(&s);
}

TEST_ONCE(iset_clear)
{
    pddl_iset_t s;
    pddlISetInit(&s);
    PDDL_ISET_ADD(&s, 1, 2, 3);
    assert(pddlISetSize(&s) == 3);

    pddlISetClear(&s);
    assert(pddlISetSize(&s) == 0);

    /* re-use after empty */
    pddlISetAdd(&s, 7);
    assert(pddlISetSize(&s) == 1);
    assert(pddlISetGet(&s, 0) == 7);
    assertSorted(&s);

    pddlISetFree(&s);
}

TEST_ONCE(iset_copy)
{
    pddl_iset_t src, dst;
    pddlISetInit(&src);
    pddlISetInit(&dst);
    PDDL_ISET_ADD(&src, 5, 10, 15);

    pddlISetCopy(&dst, &src);
    assert(pddlISetSize(&dst) == 3);
    assert(pddlISetEq(&dst, &src));
    assertSorted(&dst);

    /* modify src and verify dst is independent */
    pddlISetAdd(&src, 20);
    assert(pddlISetSize(&dst) == 3);
    assertSorted(&src);

    pddlISetFree(&src);
    pddlISetFree(&dst);
}

TEST_ONCE(iset_eq_cmp)
{
    pddl_iset_t a, b;
    pddlISetInit(&a);
    pddlISetInit(&b);

    PDDL_ISET_ADD(&a, 1, 2, 3);
    PDDL_ISET_ADD(&b, 1, 2, 3);
    assert(pddlISetEq(&a, &b));
    assert(pddlISetCmp(&a, &b) == 0);
    assertSorted(&a);
    assertSorted(&b);

    /* b has extra element */
    pddlISetAdd(&b, 4);
    assert(!pddlISetEq(&a, &b));
    assert(pddlISetCmp(&a, &b) < 0);
    assertSorted(&b);

    /* a has a different value */
    pddlISetClear(&a);
    pddlISetClear(&b);
    PDDL_ISET_ADD(&a, 1, 2, 4);
    PDDL_ISET_ADD(&b, 1, 2, 3);
    assert(!pddlISetEq(&a, &b));
    assert(pddlISetCmp(&a, &b) > 0);
    assertSorted(&a);
    assertSorted(&b);

    pddlISetFree(&a);
    pddlISetFree(&b);
}

TEST_ONCE(iset_is_subset)
{
    pddl_iset_t a, b;
    pddlISetInit(&a);
    pddlISetInit(&b);

    PDDL_ISET_ADD(&a, 2, 4);
    PDDL_ISET_ADD(&b, 1, 2, 3, 4, 5);
    assertSorted(&a);
    assertSorted(&b);
    assert(pddlISetIsSubset(&a, &b));
    assert(!pddlISetIsSubset(&b, &a));

    /* empty set is subset of any set */
    pddl_iset_t empty;
    pddlISetInit(&empty);
    assert(pddlISetIsSubset(&empty, &b));
    assert(pddlISetIsSubset(&empty, &empty));

    /* equal sets are subsets of each other */
    pddl_iset_t c;
    pddlISetInit(&c);
    pddlISetCopy(&c, &b);
    assert(pddlISetIsSubset(&c, &b));
    assert(pddlISetIsSubset(&b, &c));

    /* element not in superset */
    pddlISetClear(&a);
    PDDL_ISET_ADD(&a, 2, 99);
    assert(!pddlISetIsSubset(&a, &b));

    pddlISetFree(&a);
    pddlISetFree(&b);
    pddlISetFree(&c);
    pddlISetFree(&empty);
}

TEST_ONCE(iset_intersection_size)
{
    pddl_iset_t a, b;
    pddlISetInit(&a);
    pddlISetInit(&b);

    PDDL_ISET_ADD(&a, 1, 2, 3, 4, 5);
    PDDL_ISET_ADD(&b, 3, 4, 5, 6, 7);
    assert(pddlISetIntersectionSize(&a, &b) == 3);

    /* disjoint */
    pddlISetClear(&b);
    PDDL_ISET_ADD(&b, 10, 20);
    assert(pddlISetIntersectionSize(&a, &b) == 0);

    /* one empty */
    pddl_iset_t empty;
    pddlISetInit(&empty);
    assert(pddlISetIntersectionSize(&a, &empty) == 0);

    /* identical sets */
    assert(pddlISetIntersectionSize(&a, &a) == pddlISetSize(&a));

    pddlISetFree(&a);
    pddlISetFree(&b);
    pddlISetFree(&empty);
}

TEST_ONCE(iset_intersection_size_at_least)
{
    pddl_iset_t a, b;
    pddlISetInit(&a);
    pddlISetInit(&b);

    PDDL_ISET_ADD(&a, 1, 2, 3, 4, 5);
    PDDL_ISET_ADD(&b, 3, 4, 5, 6, 7);

    assert(pddlISetIntersectionSizeAtLeast(&a, &b, 0));
    assert(pddlISetIntersectionSizeAtLeast(&a, &b, 1));
    assert(pddlISetIntersectionSizeAtLeast(&a, &b, 3));
    assert(!pddlISetIntersectionSizeAtLeast(&a, &b, 4));

    pddlISetFree(&a);
    pddlISetFree(&b);
}

TEST_ONCE(iset_is_disjoint)
{
    pddl_iset_t a, b;
    pddlISetInit(&a);
    pddlISetInit(&b);

    PDDL_ISET_ADD(&a, 1, 2, 3);
    PDDL_ISET_ADD(&b, 4, 5, 6);
    assertSorted(&a);
    assertSorted(&b);
    assert(pddlISetIsDisjoint(&a, &b));

    pddlISetAdd(&b, 3);
    assertSorted(&b);
    assert(!pddlISetIsDisjoint(&a, &b));

    pddlISetFree(&a);
    pddlISetFree(&b);
}

TEST_ONCE(iset_union)
{
    pddl_iset_t dst, src;
    pddlISetInit(&dst);
    pddlISetInit(&src);

    PDDL_ISET_ADD(&dst, 1, 3, 5);
    PDDL_ISET_ADD(&src, 2, 3, 6);
    pddlISetUnion(&dst, &src);
    int expected[] = {1, 2, 3, 5, 6};
    checkSet(&dst, expected, 5);

    /* union with empty */
    pddlISetClear(&src);
    pddlISetUnion(&dst, &src);
    checkSet(&dst, expected, 5);

    /* union into empty */
    pddlISetClear(&dst);
    PDDL_ISET_ADD(&src, 7, 8);
    pddlISetUnion(&dst, &src);
    assert(pddlISetEq(&dst, &src));
    assertSorted(&dst);

    pddlISetFree(&dst);
    pddlISetFree(&src);
}

TEST_ONCE(iset_union_to)
{
    pddl_iset_t dst, a, b;
    pddlISetInit(&dst);
    pddlISetInit(&a);
    pddlISetInit(&b);

    PDDL_ISET_ADD(&a, 1, 3, 5);
    PDDL_ISET_ADD(&b, 2, 3, 4);
    pddlISetUnionTo(&dst, &a, &b);
    int expected[] = {1, 2, 3, 4, 5};
    checkSet(&dst, expected, 5);

    /* both empty */
    pddlISetClear(&a);
    pddlISetClear(&b);
    pddlISetUnionTo(&dst, &a, &b);
    assert(pddlISetSize(&dst) == 0);

    pddlISetFree(&dst);
    pddlISetFree(&a);
    pddlISetFree(&b);
}

/*
 * Union with a sorted array covers all paths: empty set, empty array,
 * array after / before all elements, interleaved and overlapping values.
 */
TEST_ONCE(iset_union_sorted_arr)
{
    pddl_iset_t s;
    pddlISetInit(&s);

    // into empty set
    int arr1[] = {4, 6};
    pddlISetUnionSortedArr(&s, arr1, 2);
    checkSet(&s, arr1, 2);

    // empty array
    pddlISetUnionSortedArr(&s, NULL, 0);
    checkSet(&s, arr1, 2);

    // all after the last element
    int arr2[] = {8, 9};
    pddlISetUnionSortedArr(&s, arr2, 2);
    int exp2[] = {4, 6, 8, 9};
    checkSet(&s, exp2, 4);

    // all before the first element
    int arr3[] = {1, 2};
    pddlISetUnionSortedArr(&s, arr3, 2);
    int exp3[] = {1, 2, 4, 6, 8, 9};
    checkSet(&s, exp3, 6);

    // interleaved and overlapping
    int arr4[] = {0, 2, 3, 6, 7, 10};
    pddlISetUnionSortedArr(&s, arr4, 6);
    int exp4[] = {0, 1, 2, 3, 4, 6, 7, 8, 9, 10};
    checkSet(&s, exp4, 10);

    // only existing elements
    int arr5[] = {1, 4, 10};
    pddlISetUnionSortedArr(&s, arr5, 3);
    checkSet(&s, exp4, 10);

    pddlISetFree(&s);
}

TEST_ONCE(iset_intersect)
{
    pddl_iset_t dst, src;
    pddlISetInit(&dst);
    pddlISetInit(&src);

    PDDL_ISET_ADD(&dst, 1, 2, 3, 4, 5);
    PDDL_ISET_ADD(&src, 3, 4, 5, 6, 7);
    pddlISetIntersect(&dst, &src);
    int expected[] = {3, 4, 5};
    checkSet(&dst, expected, 3);

    /* intersect to empty (disjoint) */
    pddlISetClear(&src);
    PDDL_ISET_ADD(&src, 10, 20);
    pddlISetIntersect(&dst, &src);
    assert(pddlISetSize(&dst) == 0);

    pddlISetFree(&dst);
    pddlISetFree(&src);
}

TEST_ONCE(iset_intersect_to)
{
    pddl_iset_t dst, a, b;
    pddlISetInit(&dst);
    pddlISetInit(&a);
    pddlISetInit(&b);

    PDDL_ISET_ADD(&a, 1, 2, 3, 4, 5);
    PDDL_ISET_ADD(&b, 2, 4, 6);
    pddlISetIntersectTo(&dst, &a, &b);
    int expected[] = {2, 4};
    checkSet(&dst, expected, 2);

    /* disjoint */
    pddlISetClear(&b);
    PDDL_ISET_ADD(&b, 10, 20);
    pddlISetIntersectTo(&dst, &a, &b);
    assert(pddlISetSize(&dst) == 0);

    pddlISetFree(&dst);
    pddlISetFree(&a);
    pddlISetFree(&b);
}

TEST_ONCE(iset_minus)
{
    pddl_iset_t a, b;
    pddlISetInit(&a);
    pddlISetInit(&b);

    PDDL_ISET_ADD(&a, 1, 2, 3, 4, 5);
    PDDL_ISET_ADD(&b, 2, 4);
    pddlISetMinus(&a, &b);
    int expected[] = {1, 3, 5};
    checkSet(&a, expected, 3);

    /* minus with empty set -- no change */
    pddlISetClear(&b);
    pddlISetMinus(&a, &b);
    checkSet(&a, expected, 3);

    /* minus disjoint set -- no change */
    PDDL_ISET_ADD(&b, 10, 20);
    pddlISetMinus(&a, &b);
    checkSet(&a, expected, 3);

    pddlISetFree(&a);
    pddlISetFree(&b);
}

TEST_ONCE(iset_minus_to)
{
    pddl_iset_t dst, a, b;
    pddlISetInit(&dst);
    pddlISetInit(&a);
    pddlISetInit(&b);

    PDDL_ISET_ADD(&a, 1, 2, 3, 4, 5);
    PDDL_ISET_ADD(&b, 2, 4);
    pddlISetMinusTo(&dst, &a, &b);
    int expected[] = {1, 3, 5};
    checkSet(&dst, expected, 3);

    /* subtract empty -- result equals a */
    pddlISetClear(&b);
    pddlISetMinusTo(&dst, &a, &b);
    assert(pddlISetEq(&dst, &a));

    /* a empty -- result empty */
    pddlISetClear(&a);
    PDDL_ISET_ADD(&b, 1, 2, 3);
    pddlISetMinusTo(&dst, &a, &b);
    assert(pddlISetSize(&dst) == 0);

    pddlISetFree(&dst);
    pddlISetFree(&a);
    pddlISetFree(&b);
}

TEST_ONCE(iset_remap)
{
    pddl_iset_t s;
    pddlISetInit(&s);
    PDDL_ISET_ADD(&s, 0, 1, 2, 3);

    /* identity remap */
    int remap_id[] = {0, 1, 2, 3};
    pddlISetRemap(&s, remap_id);
    assert(pddlISetSize(&s) == 4);
    assert(pddlISetHas(&s, 0));
    assert(pddlISetHas(&s, 3));
    assertSorted(&s);

    /* reverse remap: 0->3, 1->2, 2->1, 3->0 */
    pddlISetClear(&s);
    PDDL_ISET_ADD(&s, 0, 1, 2, 3);
    int remap_rev[] = {3, 2, 1, 0};
    pddlISetRemap(&s, remap_rev);
    assert(pddlISetSize(&s) == 4);
    assert(pddlISetHas(&s, 0));
    assert(pddlISetHas(&s, 3));
    assertSorted(&s);

    /* collapsing remap: multiple values map to same -- duplicates removed */
    pddlISetClear(&s);
    PDDL_ISET_ADD(&s, 0, 1, 2);
    int remap_coll[] = {5, 5, 7};
    pddlISetRemap(&s, remap_coll);
    assert(pddlISetSize(&s) == 2);
    assert(pddlISetHas(&s, 5));
    assert(pddlISetHas(&s, 7));
    assertSorted(&s);

    pddlISetFree(&s);
}

TEST_ONCE(iset_for_each)
{
    pddl_iset_t s;
    pddlISetInit(&s);
    PDDL_ISET_ADD(&s, 2, 4, 6, 8);
    assertSorted(&s);

    int sum = 0;
    int count = 0;
    PDDL_ISET_FOR_EACH(&s, v){
        sum += v;
        ++count;
    }
    assert(count == 4);
    assert(sum == 20);

    pddlISetFree(&s);
}

TEST_ONCE(iset_macros)
{
    pddl_iset_t s;
    pddlISetInit(&s);

    /* PDDL_ISET_ADD */
    PDDL_ISET_ADD(&s, 3, 1, 2);
    assert(pddlISetSize(&s) == 3);
    assert(pddlISetGet(&s, 0) == 1);
    assert(pddlISetGet(&s, 1) == 2);
    assert(pddlISetGet(&s, 2) == 3);
    assertSorted(&s);

    /* PDDL_ISET_ASSIGN resets and fills */
    PDDL_ISET_ASSIGN(&s, 10, 20);
    assert(pddlISetSize(&s) == 2);
    assert(pddlISetGet(&s, 0) == 10);
    assert(pddlISetGet(&s, 1) == 20);
    assertSorted(&s);

    pddlISetFree(&s);
}

TEST_ONCE(iset_large)
{
    /* Exercise binary-search path (size > 256) for pddlISetAdd */
    pddl_iset_t s;
    pddlISetInit(&s);

    int n = 400;
    /* add all even numbers first (forward order) */
    for (int i = 0; i < n; i += 2)
        pddlISetAdd(&s, i);
    /* interleave odd numbers (each inserted mid-list) */
    for (int i = 1; i < n; i += 2)
        pddlISetAdd(&s, i);

    assert(pddlISetSize(&s) == n);
    for (int i = 0; i < n; ++i)
        assert(pddlISetGet(&s, i) == i);

    /* verify has/in on a large set */
    assert(pddlISetHas(&s, 0));
    assert(pddlISetHas(&s, n - 1));
    assert(!pddlISetHas(&s, n));

    pddlISetFree(&s);
}

/* -----------------------------------------------------------------------
 * Tests specifically targeting the "one set is much larger" branches.
 * Each function in iset.c switches from a two-pointer linear scan to a
 * binary-search strategy when one set is more than 4x the size of the
 * other (condition: big->size > 4 * small->size).
 * We use SMALL_N = 5 and BIG_N = 25 so that 25 > 4*5 = 20.
 * ----------------------------------------------------------------------- */

/* Build a set containing every STEP-th value from 0 up to (but not
 * including) n*step: {0, step, 2*step, ..., (n-1)*step}. */
static void buildStride(pddl_iset_t *s, int n, int step)
{
    for (int i = 0; i < n; ++i)
        pddlISetAdd(s, i * step);
}

TEST_ONCE(iset_skew_is_subset)
{
    /* s2->size > 4 * s1->size triggers the binary-search path in
     * pddlISetIsSubset.  We also exercise the reverse (s1 big, s2 small)
     * which must return false immediately because s1->size > s2->size. */
    pddl_iset_t small_s, big_s;
    pddlISetInit(&small_s);
    pddlISetInit(&big_s);

    /* big: {0,1,2,...,24}   small: {0,4,8,12,16}  (every 4th element) */
    buildStride(&big_s, 25, 1);
    buildStride(&small_s, 5, 4);
    assertSorted(&big_s);
    assertSorted(&small_s);

    /* 25 > 4*5 => binary-search path: small IS a subset of big */
    assert(pddlISetIsSubset(&small_s, &big_s));

    /* reverse: big is NOT a subset of small */
    assert(!pddlISetIsSubset(&big_s, &small_s));

    /* small with an element NOT in big: not a subset */
    pddlISetAdd(&small_s, 100);
    assert(!pddlISetIsSubset(&small_s, &big_s));

    /* empty small set is always a subset */
    pddlISetClear(&small_s);
    assert(pddlISetIsSubset(&small_s, &big_s));

    pddlISetFree(&small_s);
    pddlISetFree(&big_s);
}

TEST_ONCE(iset_skew_intersection_size)
{
    /* s2->size > 4 * s1->size triggers binary-search path in
     * pddlISetIntersectionSize.  Test both orientations. */
    pddl_iset_t small_s, big_s;
    pddlISetInit(&small_s);
    pddlISetInit(&big_s);

    /* big: {0..24}  small (matches): {0,4,8,12,16} */
    buildStride(&big_s, 25, 1);
    buildStride(&small_s, 5, 4);
    assertSorted(&big_s);
    assertSorted(&small_s);

    /* small vs big: 25 > 4*5 */
    assert(pddlISetIntersectionSize(&small_s, &big_s) == 5);
    /* symmetric */
    assert(pddlISetIntersectionSize(&big_s, &small_s) == 5);

    /* small with no overlap: {100, 104, 108, 112, 116} */
    pddlISetClear(&small_s);
    buildStride(&small_s, 5, 4);
    for (int i = 0; i < 5; ++i)
        pddlISetAdd(&small_s, 100 + i * 4);
    pddlISetClear(&small_s);
    for (int i = 0; i < 5; ++i)
        pddlISetAdd(&small_s, 100 + i * 4);
    assert(pddlISetIntersectionSize(&small_s, &big_s) == 0);
    assert(pddlISetIntersectionSize(&big_s, &small_s) == 0);

    /* partial overlap: small = {0,4,8,100,104} -- 2 match big */
    pddlISetClear(&small_s);
    pddlISetAdd(&small_s, 0);
    pddlISetAdd(&small_s, 4);
    pddlISetAdd(&small_s, 8);
    pddlISetAdd(&small_s, 100);
    pddlISetAdd(&small_s, 104);
    assert(pddlISetIntersectionSize(&small_s, &big_s) == 3);
    assert(pddlISetIntersectionSize(&big_s, &small_s) == 3);

    pddlISetFree(&small_s);
    pddlISetFree(&big_s);
}

TEST_ONCE(iset_skew_intersection_size_at_least)
{
    /* Same size ratio as above; tests the early-exit limit variant. */
    pddl_iset_t small_s, big_s;
    pddlISetInit(&small_s);
    pddlISetInit(&big_s);

    buildStride(&big_s, 25, 1);   /* {0..24} */
    buildStride(&small_s, 5, 4);  /* {0,4,8,12,16} -- all in big */
    assertSorted(&big_s);
    assertSorted(&small_s);

    /* limit met */
    assert(pddlISetIntersectionSizeAtLeast(&small_s, &big_s, 1));
    assert(pddlISetIntersectionSizeAtLeast(&small_s, &big_s, 5));
    assert(pddlISetIntersectionSizeAtLeast(&big_s, &small_s, 5));
    /* limit not met */
    assert(!pddlISetIntersectionSizeAtLeast(&small_s, &big_s, 6));
    assert(!pddlISetIntersectionSizeAtLeast(&big_s, &small_s, 6));

    /* no overlap: none of small's values are in big */
    pddlISetClear(&small_s);
    for (int i = 0; i < 5; ++i)
        pddlISetAdd(&small_s, 100 + i * 4);
    assert(!pddlISetIntersectionSizeAtLeast(&small_s, &big_s, 1));
    assert(!pddlISetIntersectionSizeAtLeast(&big_s, &small_s, 1));

    pddlISetFree(&small_s);
    pddlISetFree(&big_s);
}

TEST_ONCE(iset_skew_intersect)
{
    /* dst->size > 4 * src->size  OR  src->size > 4 * dst->size
     * triggers binary-search in pddlISetIntersect.
     * When dst is the larger set the implementation internally swaps to
     * iterate over the smaller one, so the result must still be correct. */
    pddl_iset_t dst, src;
    pddlISetInit(&dst);
    pddlISetInit(&src);

    /* Case 1: src small, dst big */
    buildStride(&dst, 25, 1);   /* {0..24} */
    buildStride(&src, 5, 4);    /* {0,4,8,12,16} */
    pddlISetIntersect(&dst, &src);
    int exp1[] = {0, 4, 8, 12, 16};
    checkSet(&dst, exp1, 5);

    /* Case 2: dst small, src big */
    pddlISetClear(&dst);
    buildStride(&dst, 5, 4);    /* {0,4,8,12,16} */
    pddlISetClear(&src);
    buildStride(&src, 25, 1);   /* {0..24} */
    pddlISetIntersect(&dst, &src);
    checkSet(&dst, exp1, 5);

    /* Case 3: no overlap -- result empty */
    pddlISetClear(&dst);
    buildStride(&dst, 25, 1);
    pddlISetClear(&src);
    for (int i = 0; i < 5; ++i)
        pddlISetAdd(&src, 100 + i);
    pddlISetIntersect(&dst, &src);
    assert(pddlISetSize(&dst) == 0);

    pddlISetClear(&dst);
    buildStride(&dst, 25, 1);
    pddlISetClear(&src);
    for (int i = 0; i < 5; ++i)
        pddlISetAdd(&src, 100 + i);
    pddlISetIntersect(&src, &dst);
    assert(pddlISetSize(&src) == 0);

    pddlISetFree(&dst);
    pddlISetFree(&src);
}

TEST_ONCE(iset_skew_intersect_to)
{
    /* Same as iset_skew_intersect but using pddlISetIntersectTo (result in
     * separate output set). */
    pddl_iset_t dst, small_s, big_s;
    pddlISetInit(&dst);
    pddlISetInit(&small_s);
    pddlISetInit(&big_s);

    buildStride(&big_s, 50, 1);   /* {0..24} */
    buildStride(&small_s, 5, 4);  /* {0,4,8,12,16} */
    pddlISetAdd(&small_s, 101);
    pddlISetAdd(&small_s, 102);

    /* small as first argument */
    pddlISetIntersectTo(&dst, &small_s, &big_s);
    int exp[] = {0, 4, 8, 12, 16};
    checkSet(&dst, exp, 5);

    /* big as first argument */
    pddlISetIntersectTo(&dst, &big_s, &small_s);
    checkSet(&dst, exp, 5);

    /* no overlap */
    pddlISetClear(&small_s);
    for (int i = 0; i < 5; ++i)
        pddlISetAdd(&small_s, 100 + i);
    pddlISetIntersectTo(&dst, &small_s, &big_s);
    assert(pddlISetSize(&dst) == 0);
    pddlISetIntersectTo(&dst, &big_s, &small_s);
    assert(pddlISetSize(&dst) == 0);

    pddlISetFree(&dst);
    pddlISetFree(&small_s);
    pddlISetFree(&big_s);
}

/* -----------------------------------------------------------------------
 * Tests for the two size-based branches inside pddlISetAdd:
 *   s->size <= 256 : right-to-left linear shift
 *   s->size >  256 : binary search for insertion point
 * Both branches also handle duplicates internally.
 * ----------------------------------------------------------------------- */

TEST_ONCE(iset_add_linear_shift)
{
    /* Keep the set at or below 256 elements so every out-of-order insert
     * goes through the linear-shift branch (s->size <= 256). */
    pddl_iset_t s;
    pddlISetInit(&s);

    /* Populate with even values 0..510 in order (fast append path, no
     * branch taken yet -- just sets up the 256-element base). */
    for (int i = 0; i <= 510; i += 2)
        pddlISetAdd(&s, i);
    assert(pddlISetSize(&s) == 256);
    assertSorted(&s);

    /* Now insert one odd value at the very beginning: must shift all 256
     * elements right -- exercises the max-work linear-shift case. */
    pddlISetAdd(&s, 1);
    assert(pddlISetSize(&s) == 257);  /* size is now 257 ... */
    assertSorted(&s);

    /* ... but we want to keep testing the <=256 branch: rebuild smaller. */
    pddlISetFree(&s);
    pddlISetInit(&s);

    /* Build {0, 2, 4, ..., 126}: 64 even values. */
    for (int i = 0; i <= 126; i += 2)
        pddlISetAdd(&s, i);
    assert(pddlISetSize(&s) == 64);
    assertSorted(&s);

    /* Insert odd values in reverse order so each goes to a different
     * interior position -- stresses the shift path across many slots. */
    for (int i = 127; i >= 1; i -= 2)
        pddlISetAdd(&s, i);
    assert(pddlISetSize(&s) == 128);
    assertSorted(&s);

    /* Result must be {0, 1, 2, ..., 127} */
    for (int i = 0; i < 128; ++i)
        assert(pddlISetGet(&s, i) == i);
    assertSorted(&s);

    /* Duplicate insertion via the linear-shift branch: re-insert existing
     * interior values; size must not change. */
    for (int i = 0; i < 128; ++i)
        pddlISetAdd(&s, i);
    assert(pddlISetSize(&s) == 128);
    assertSorted(&s);

    /* Insert at the front (v < s[0]) -- maximum shift distance. */
    pddlISetAdd(&s, -1);
    assert(pddlISetSize(&s) == 129);
    assert(pddlISetGet(&s, 0) == -1);
    assertSorted(&s);

    /* Duplicate of the newly inserted front element. */
    pddlISetAdd(&s, -1);
    assert(pddlISetSize(&s) == 129);
    assertSorted(&s);

    pddlISetFree(&s);
}

TEST_ONCE(iset_add_binary_search)
{
    /* Build a set with more than 256 elements so out-of-order inserts
     * exercise the binary-search branch (s->size > 256). */
    pddl_iset_t s;
    pddlISetInit(&s);

    /* Populate with even values 0..1022 in order (512 elements, all via
     * the fast append path). */
    for (int i = 0; i <= 1022; i += 2)
        pddlISetAdd(&s, i);
    assert(pddlISetSize(&s) == 512);
    assertSorted(&s);

    /* Insert odd values scattered across the range so each triggers a
     * binary search followed by a memmove. */
    for (int i = 1; i <= 1023; i += 2)
        pddlISetAdd(&s, i);
    assert(pddlISetSize(&s) == 1024);
    assertSorted(&s);

    /* Verify the set is {0, 1, 2, ..., 1023}. */
    for (int i = 0; i < 1024; ++i)
        assert(pddlISetGet(&s, i) == i);
    assertSorted(&s);

    /* Duplicate insertion via the binary-search branch: none should change
     * the size. */
    for (int i = 0; i < 1024; ++i)
        pddlISetAdd(&s, i);
    assert(pddlISetSize(&s) == 1024);
    assertSorted(&s);

    /* Insert a value smaller than all existing -- binary search finds
     * lo == 0, triggers memmove of the entire array. */
    pddlISetAdd(&s, -1);
    assert(pddlISetSize(&s) == 1025);
    assert(pddlISetGet(&s, 0) == -1);
    assertSorted(&s);

    /* Duplicate of that front element (binary search finds exact match). */
    pddlISetAdd(&s, -1);
    assert(pddlISetSize(&s) == 1025);
    assertSorted(&s);

    /* Insert a value larger than all existing -- goes through the fast
     * append path even at this size; confirm. */
    pddlISetAdd(&s, 9999);
    assert(pddlISetSize(&s) == 1026);
    assert(pddlISetGet(&s, 1025) == 9999);
    assertSorted(&s);

    pddlISetFree(&s);
}

/* -----------------------------------------------------------------------
 * Random tests.
 * All values are drawn from [0, RAND_RANGE) so a simple boolean array
 * serves as an independent reference implementation for every operation.
 * ----------------------------------------------------------------------- */

#define RAND_RANGE 1024
#define RAND_ITERS 100

/* Bounded random integer in [lo, hi]. */
static int randInt(pddl_rand_t *rnd, int lo, int hi)
{
    return lo + (int)(pddlRandInt(rnd) % (unsigned)(hi - lo + 1));
}

/* Add n values drawn at random from [0, RAND_RANGE) to s. */
static void fillRand(pddl_iset_t *s, pddl_rand_t *rnd, int n)
{
    for (int i = 0; i < n; ++i)
        pddlISetAdd(s, randInt(rnd, 0, RAND_RANGE - 1));
    assertSorted(s);
}

/* Encode set as a membership bit-array (ref[v] == 1 iff v is in s). */
static void setToRef(const pddl_iset_t *s, int ref[RAND_RANGE])
{
    memset(ref, 0, RAND_RANGE * sizeof(int));
    PDDL_ISET_FOR_EACH(s, v)
        ref[v] = 1;
}

/* Assert s matches a membership bit-array exactly (also checks sortedness). */
static void assertMatchesRef(const pddl_iset_t *s, const int ref[RAND_RANGE])
{
    int count = 0;
    for (int i = 0; i < RAND_RANGE; ++i){
        if (ref[i]){
            assert(pddlISetHas(s, i));
            ++count;
        }else{
            assert(!pddlISetHas(s, i));
        }
    }
    assert(pddlISetSize(s) == count);
    assertSorted(s);
}

TEST_ONCE(iset_rand_add_has)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    for (int iter = 0; iter < RAND_ITERS; ++iter){
        int n = randInt(&rnd, 1, 60);
        int ref[RAND_RANGE] = {0};
        pddl_iset_t s;
        pddlISetInit(&s);

        for (int i = 0; i < n; ++i){
            int v = randInt(&rnd, 0, RAND_RANGE - 1);
            ref[v] = 1;
            pddlISetAdd(&s, v);
        }
        assertMatchesRef(&s, ref);
        pddlISetFree(&s);
    }
}

TEST_ONCE(iset_rand_remove)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    for (int iter = 0; iter < RAND_ITERS; ++iter){
        int ref[RAND_RANGE] = {0};
        pddl_iset_t s;
        pddlISetInit(&s);
        fillRand(&s, &rnd, randInt(&rnd, 5, 60));
        setToRef(&s, ref);

        /* Remove a random selection of values; verify return value and
         * updated membership for each removal. */
        int rm_count = randInt(&rnd, 1, RAND_RANGE / 2);
        for (int i = 0; i < rm_count; ++i){
            int v = randInt(&rnd, 0, RAND_RANGE - 1);
            int was_in = pddlISetRemove(&s, v) ? 1 : 0;
            assert(was_in == ref[v]);
            ref[v] = 0;
        }
        assertMatchesRef(&s, ref);
        pddlISetFree(&s);
    }
}

TEST_ONCE(iset_rand_union)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    for (int iter = 0; iter < RAND_ITERS; ++iter){
        pddl_iset_t a, b, dst;
        pddlISetInit(&a);
        pddlISetInit(&b);
        pddlISetInit(&dst);

        fillRand(&a, &rnd, randInt(&rnd, 0, 40));
        fillRand(&b, &rnd, randInt(&rnd, 0, 40));

        int refa[RAND_RANGE], refb[RAND_RANGE], refu[RAND_RANGE];
        setToRef(&a, refa);
        setToRef(&b, refb);
        for (int i = 0; i < RAND_RANGE; ++i)
            refu[i] = refa[i] | refb[i];

        /* pddlISetUnion: dst = a; dst |= b */
        pddlISetCopy(&dst, &a);
        pddlISetUnion(&dst, &b);
        assertMatchesRef(&dst, refu);

        /* pddlISetUnionTo */
        pddlISetUnionTo(&dst, &a, &b);
        assertMatchesRef(&dst, refu);

        /* commutativity */
        pddlISetUnionTo(&dst, &b, &a);
        assertMatchesRef(&dst, refu);

        pddlISetFree(&a);
        pddlISetFree(&b);
        pddlISetFree(&dst);
    }
}

TEST_ONCE(iset_rand_intersect)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    for (int iter = 0; iter < RAND_ITERS; ++iter){
        pddl_iset_t a, b, dst;
        pddlISetInit(&a);
        pddlISetInit(&b);
        pddlISetInit(&dst);

        fillRand(&a, &rnd, randInt(&rnd, 0, 40));
        fillRand(&b, &rnd, randInt(&rnd, 0, 40));

        int refa[RAND_RANGE], refb[RAND_RANGE], refi[RAND_RANGE];
        setToRef(&a, refa);
        setToRef(&b, refb);
        for (int i = 0; i < RAND_RANGE; ++i)
            refi[i] = refa[i] & refb[i];

        /* pddlISetIntersect */
        pddlISetCopy(&dst, &a);
        pddlISetIntersect(&dst, &b);
        assertMatchesRef(&dst, refi);

        /* pddlISetIntersectTo, both argument orders */
        pddlISetIntersectTo(&dst, &a, &b);
        assertMatchesRef(&dst, refi);

        pddlISetIntersectTo(&dst, &b, &a);
        assertMatchesRef(&dst, refi);

        pddlISetFree(&a);
        pddlISetFree(&b);
        pddlISetFree(&dst);
    }
}

TEST_ONCE(iset_rand_minus)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    for (int iter = 0; iter < RAND_ITERS; ++iter){
        pddl_iset_t a, b, dst;
        pddlISetInit(&a);
        pddlISetInit(&b);
        pddlISetInit(&dst);

        fillRand(&a, &rnd, randInt(&rnd, 0, 40));
        fillRand(&b, &rnd, randInt(&rnd, 0, 40));

        int refa[RAND_RANGE], refb[RAND_RANGE], refm[RAND_RANGE];
        setToRef(&a, refa);
        setToRef(&b, refb);
        for (int i = 0; i < RAND_RANGE; ++i)
            refm[i] = refa[i] & !refb[i];

        /* pddlISetMinus */
        pddlISetCopy(&dst, &a);
        pddlISetMinus(&dst, &b);
        assertMatchesRef(&dst, refm);

        /* pddlISetMinusTo */
        pddlISetMinusTo(&dst, &a, &b);
        assertMatchesRef(&dst, refm);

        pddlISetFree(&a);
        pddlISetFree(&b);
        pddlISetFree(&dst);
    }
}

TEST_ONCE(iset_rand_is_subset)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    for (int iter = 0; iter < RAND_ITERS; ++iter){
        pddl_iset_t a, b;
        pddlISetInit(&a);
        pddlISetInit(&b);

        fillRand(&a, &rnd, randInt(&rnd, 0, 30));
        fillRand(&b, &rnd, randInt(&rnd, 0, 30));

        int refa[RAND_RANGE], refb[RAND_RANGE];
        setToRef(&a, refa);
        setToRef(&b, refb);

        /* reference: a is subset of b iff every element of a is in b */
        int ref_ab = 1, ref_ba = 1;
        for (int i = 0; i < RAND_RANGE; ++i){
            if (refa[i] && !refb[i])
                ref_ab = 0;
            if (refb[i] && !refa[i])
                ref_ba = 0;
        }
        assert(!!pddlISetIsSubset(&a, &b) == ref_ab);
        assert(!!pddlISetIsSubset(&b, &a) == ref_ba);

        pddlISetFree(&a);
        pddlISetFree(&b);
    }
}

TEST_ONCE(iset_rand_intersection_size)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    for (int iter = 0; iter < RAND_ITERS; ++iter){
        pddl_iset_t a, b;
        pddlISetInit(&a);
        pddlISetInit(&b);

        fillRand(&a, &rnd, randInt(&rnd, 0, 40));
        fillRand(&b, &rnd, randInt(&rnd, 0, 40));

        int refa[RAND_RANGE], refb[RAND_RANGE];
        setToRef(&a, refa);
        setToRef(&b, refb);

        int ref_count = 0;
        for (int i = 0; i < RAND_RANGE; ++i)
            if (refa[i] && refb[i])
                ++ref_count;

        assert(pddlISetIntersectionSize(&a, &b) == ref_count);
        assert(pddlISetIntersectionSize(&b, &a) == ref_count);
        assert(!!pddlISetIsDisjoint(&a, &b) == (ref_count == 0));

        /* IntersectionSizeAtLeast */
        assert(pddlISetIntersectionSizeAtLeast(&a, &b, 0));
        if (ref_count > 0){
            assert(pddlISetIntersectionSizeAtLeast(&a, &b, ref_count));
            assert(!pddlISetIntersectionSizeAtLeast(&a, &b, ref_count + 1));
        }else{
            assert(!pddlISetIntersectionSizeAtLeast(&a, &b, 1));
        }

        pddlISetFree(&a);
        pddlISetFree(&b);
    }
}

TEST_ONCE(iset_rand_remap)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    /* Use a small value range so the remap array fits on the stack and
     * reference computation is simple. */
    int range = 128;
    for (int iter = 0; iter < RAND_ITERS; ++iter){
        int remap[range];
        for (int i = 0; i < range; ++i)
            remap[i] = randInt(&rnd, 0, range - 1);

        pddl_iset_t s, expected;
        pddlISetInit(&s);
        pddlISetInit(&expected);

        int n = randInt(&rnd, 1, range);
        for (int i = 0; i < n; ++i){
            int v = randInt(&rnd, 0, range - 1);
            pddlISetAdd(&s, v);
            pddlISetAdd(&expected, remap[v]);
        }

        pddlISetRemap(&s, remap);
        assert(pddlISetEq(&s, &expected));
        assertSorted(&s);

        pddlISetFree(&s);
        pddlISetFree(&expected);
    }
}

TEST_ONCE(iset_rand_skew)
{
    /* Random tests that always maintain the >4x size ratio so the
     * binary-search branches inside each function are exercised.
     * Big set: sequential 0..big_n-1 (guaranteed size).
     * Small set: up to 4 random values from [0, RAND_RANGE) so that
     * big_n >= 21 > 4 * 4 is always satisfied. */
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    for (int iter = 0; iter < RAND_ITERS; ++iter){
        pddl_iset_t small_s, big_s, dst;
        pddlISetInit(&small_s);
        pddlISetInit(&big_s);
        pddlISetInit(&dst);

        int big_n = randInt(&rnd, 21, 80);
        big_n += 300;
        for (int i = 0; i < big_n; ++i)
            pddlISetAdd(&big_s, randInt(&rnd, 0, RAND_RANGE - 1));

        /* small_s has at most 4 elements; big_n >= 21 > 4*4 */
        fillRand(&small_s, &rnd, randInt(&rnd, 1, 4));

        int refs[RAND_RANGE] = {0};
        int refb[RAND_RANGE] = {0};
        setToRef(&small_s, refs);
        setToRef(&big_s, refb);

        int refi[RAND_RANGE];
        for (int i = 0; i < RAND_RANGE; ++i)
            refi[i] = refs[i] & refb[i];

        /* is_subset */
        int ref_subset = 1;
        for (int i = 0; i < RAND_RANGE; ++i)
            if (refs[i] && !refb[i])
                ref_subset = 0;
        assert(!!pddlISetIsSubset(&small_s, &big_s) == ref_subset);

        /* intersection size, both orientations */
        int ref_isz = 0;
        for (int i = 0; i < RAND_RANGE; ++i)
            if (refi[i])
                ++ref_isz;
        assert(pddlISetIntersectionSize(&small_s, &big_s) == ref_isz);
        assert(pddlISetIntersectionSize(&big_s, &small_s) == ref_isz);
        if (ref_isz > 0){
            assert(pddlISetIntersectionSizeAtLeast(&small_s, &big_s, ref_isz));
            assert(!pddlISetIntersectionSizeAtLeast(&small_s, &big_s, ref_isz + 1));
        }

        /* intersect, all four orientation/function combinations */
        pddlISetCopy(&dst, &small_s);
        pddlISetIntersect(&dst, &big_s);
        assertMatchesRef(&dst, refi);

        pddlISetCopy(&dst, &big_s);
        pddlISetIntersect(&dst, &small_s);
        assertMatchesRef(&dst, refi);

        pddlISetIntersectTo(&dst, &small_s, &big_s);
        assertMatchesRef(&dst, refi);

        pddlISetIntersectTo(&dst, &big_s, &small_s);
        assertMatchesRef(&dst, refi);

        pddlISetFree(&small_s);
        pddlISetFree(&big_s);
        pddlISetFree(&dst);
    }
}

/*
 * pddlISetMoveToIVec() hands the buffer of the set over to the vector
 * (replacing and freeing its previous contents) and leaves the set empty
 * and unallocated.
 */
TEST_ONCE(iset_move_to_ivec)
{
    PDDL_ISET(s);
    PDDL_ISET_ADD(&s, 7, 3, 5, 1);
    int *buf = s.s;
    int cap = s.cap;

    PDDL_IVEC(v);
    pddlIVecPush(&v, 42);
    pddlIVecPush(&v, 43);

    pddlISetMoveToIVec(&s, &v);
    int exp[] = { 1, 3, 5, 7 };
    assert(pddlIVecSize(&v) == 4);
    for (int i = 0; i < 4; ++i)
        assert(pddlIVecGet(&v, i) == exp[i]);
    assert(pddlIVecData(&v) == buf);
    assert(pddlIVecCap(&v) == cap);
    assert(s.s == NULL);
    assert(s.size == 0);
    assert(s.cap == 0);

    // The vector is fully usable
    pddlIVecPush(&v, 0);
    assert(pddlIVecSize(&v) == 5);
    assert(pddlIVecLast(&v) == 0);

    // An empty, unallocated set results in an empty, unallocated vector
    pddlISetMoveToIVec(&s, &v);
    assert(pddlIVecData(&v) == NULL);
    assert(pddlIVecSize(&v) == 0);
    assert(pddlIVecCap(&v) == 0);

    pddlIVecFree(&v);
    pddlISetFree(&s);
}

/*
 * pddlIVecMoveToISet() hands the buffer of the vector over to the set
 * (replacing and freeing its previous contents), sorts it, and removes
 * duplicates; the vector is left empty and unallocated.
 */
TEST_ONCE(ivec_move_to_iset)
{
    PDDL_IVEC(v);
    int vals[] = { 5, 3, 9, 3, 1, 5, 5, 0, 9 };
    pddlIVecAppendArr(&v, vals, 9);
    int *buf = pddlIVecData(&v);
    int cap = pddlIVecCap(&v);

    PDDL_ISET(s);
    PDDL_ISET_ADD(&s, 100, 200);

    pddlIVecMoveToISet(&v, &s);
    int exp[] = { 0, 1, 3, 5, 9 };
    checkSet(&s, exp, 5);
    assert(s.s == buf);
    assert(s.cap == cap);
    assert(pddlIVecData(&v) == NULL);
    assert(pddlIVecSize(&v) == 0);
    assert(pddlIVecCap(&v) == 0);

    // The set is fully usable
    pddlISetAdd(&s, 4);
    pddlISetAdd(&s, 3);
    int exp2[] = { 0, 1, 3, 4, 5, 9 };
    checkSet(&s, exp2, 6);
    assert(pddlISetHas(&s, 4));
    assert(!pddlISetHas(&s, 2));

    // A single element
    pddlIVecPush(&v, 8);
    pddlIVecMoveToISet(&v, &s);
    int exp3[] = { 8 };
    checkSet(&s, exp3, 1);

    // All elements equal
    for (int i = 0; i < 5; ++i)
        pddlIVecPush(&v, 2);
    pddlIVecMoveToISet(&v, &s);
    int exp4[] = { 2 };
    checkSet(&s, exp4, 1);

    // An empty, unallocated vector results in an empty, unallocated set
    pddlIVecMoveToISet(&v, &s);
    assert(s.s == NULL);
    assert(s.size == 0);
    assert(s.cap == 0);

    pddlISetFree(&s);
    pddlIVecFree(&v);
}

/*
 * pddlIVecMoveToISetSorted() hands the buffer of an already sorted vector
 * over to the set (removing duplicates in place) and leaves the vector
 * empty and unallocated.
 */
TEST_ONCE(ivec_move_to_iset_sorted)
{
    PDDL_IVEC(v);
    pddlIVecReserve(&v, 100);
    for (int i = 0; i < 50; ++i)
        pddlIVecPush(&v, 3 * i - 20);
    int *buf = pddlIVecData(&v);

    PDDL_ISET(s);
    PDDL_ISET_ADD(&s, 1, 2, 3);

    pddlIVecMoveToISetSorted(&v, &s);
    assert(pddlISetSize(&s) == 50);
    for (int i = 0; i < 50; ++i)
        assert(pddlISetGet(&s, i) == 3 * i - 20);
    assert(s.s == buf);
    assert(s.cap == 100);
    assert(pddlIVecData(&v) == NULL);
    assert(pddlIVecSize(&v) == 0);
    assert(pddlIVecCap(&v) == 0);

    // Appending to the set uses the remaining capacity
    pddlISetAdd(&s, 1000);
    assert(pddlISetSize(&s) == 51);
    assert(s.s == buf);

    // Sorted input with duplicates
    int vals[] = { -3, -3, 0, 2, 2, 2, 7 };
    pddlIVecAppendArr(&v, vals, 7);
    buf = pddlIVecData(&v);
    pddlIVecMoveToISetSorted(&v, &s);
    int exp[] = { -3, 0, 2, 7 };
    checkSet(&s, exp, 4);
    assert(s.s == buf);
    assert(pddlIVecData(&v) == NULL);
    assert(pddlIVecSize(&v) == 0);
    assert(pddlIVecCap(&v) == 0);

    pddlISetFree(&s);
    pddlIVecFree(&v);
}

/*
 * pddlIVecMoveToISetSortedUniq() hands the buffer of an already sorted
 * unique vector over to the set unchanged (replacing the previous contents
 * of the set) and leaves the vector empty and unallocated.
 */
TEST_ONCE(ivec_move_to_iset_sorted_uniq)
{
    PDDL_IVEC(v);
    pddlIVecReserve(&v, 64);
    for (int i = 0; i < 30; ++i)
        pddlIVecPush(&v, 5 * i - 50);
    int *buf = pddlIVecData(&v);

    PDDL_ISET(s);
    PDDL_ISET_ADD(&s, 4, 8, 15, 16, 23, 42);

    pddlIVecMoveToISetSortedUniq(&v, &s);
    assert(pddlISetSize(&s) == 30);
    for (int i = 0; i < 30; ++i)
        assert(pddlISetGet(&s, i) == 5 * i - 50);
    assertSorted(&s);
    assert(s.s == buf);
    assert(s.cap == 64);
    assert(pddlIVecData(&v) == NULL);
    assert(pddlIVecSize(&v) == 0);
    assert(pddlIVecCap(&v) == 0);

    // The set is fully usable and appending uses the remaining capacity
    pddlISetAdd(&s, 1000);
    pddlISetAdd(&s, -49);
    assert(pddlISetSize(&s) == 32);
    assert(pddlISetHas(&s, 1000));
    assert(pddlISetHas(&s, -49));
    assertSorted(&s);
    assert(s.s == buf);

    // An empty, unallocated vector results in an empty, unallocated set
    pddlIVecMoveToISetSortedUniq(&v, &s);
    assert(s.s == NULL);
    assert(s.size == 0);
    assert(s.cap == 0);

    pddlISetFree(&s);
    pddlIVecFree(&v);
}

/*
 * Moving a set to a vector, modifying the vector, and moving it back
 * yields the expected set.
 */
TEST_ONCE(iset_ivec_move_round_trip)
{
    PDDL_ISET(s);
    for (int i = 0; i < 20; ++i)
        pddlISetAdd(&s, 2 * i);

    PDDL_IVEC(v);
    pddlISetMoveToIVec(&s, &v);
    for (int i = 0; i < 20; ++i)
        pddlIVecPush(&v, 2 * i + 1);
    pddlIVecPush(&v, 7);
    pddlIVecPush(&v, -1);

    pddlIVecMoveToISet(&v, &s);
    assert(pddlISetSize(&s) == 41);
    for (int i = 0; i < 41; ++i)
        assert(pddlISetGet(&s, i) == i - 1);
    assertSorted(&s);

    pddlISetFree(&s);
    pddlIVecFree(&v);
}
