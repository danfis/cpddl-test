/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/ivec.h"
#include "pddl/rand.h"
#include "test.h"
#include <assert.h>

/* Check that v contains exactly the given values in the given order. */
static void checkVec(const pddl_ivec_t *v, const int *vals, int n)
{
    assert(pddlIVecSize(v) == n);
    assert(pddlIVecSize(v) <= pddlIVecCap(v));
    for (int i = 0; i < n; ++i)
        assert(pddlIVecGet(v, i) == vals[i]);
}

/* Check that v is empty and has no memory allocated. */
static void assertUnallocated(const pddl_ivec_t *v)
{
    assert(pddlIVecData(v) == NULL);
    assert(pddlIVecSize(v) == 0);
    assert(pddlIVecCap(v) == 0);
}

/* Bounded random integer in [lo, hi]. */
static int randInt(pddl_rand_t *rnd, int lo, int hi)
{
    return lo + (int)(pddlRandInt(rnd) % (unsigned)(hi - lo + 1));
}

static int cmpInt(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

/* Reference sort of v with libc qsort(). */
static void refSort(pddl_ivec_t *v)
{
    if (pddlIVecSize(v) > 0)
        qsort(pddlIVecData(v), pddlIVecSize(v), sizeof(int), cmpInt);
}

/*
 * Both the static initializer and pddlIVecInit() give an empty vector
 * without any allocation, and freeing it is safe.
 */
TEST_ONCE(ivec_init_free)
{
    PDDL_IVEC(a);
    assertUnallocated(&a);
    assert(pddlIVecIsEmpty(&a));
    assert(pddlIVecSize(&a) == 0);
    assert(pddlIVecData(&a) == NULL);
    pddlIVecFree(&a);

    pddl_ivec_t b;
    memset(&b, 0x5a, sizeof(b));
    pddlIVecInit(&b);
    assertUnallocated(&b);
    pddlIVecFree(&b);
}

/*
 * Push grows the vector over several reallocations, Last and Pop return
 * the elements in the reverse order.
 */
TEST_ONCE(ivec_push_pop)
{
    PDDL_IVEC(v);
    for (int i = 0; i < 1000; ++i){
        pddlIVecPush(&v, 3 * i - 7);
        assert(pddlIVecSize(&v) == i + 1);
        assert(pddlIVecLast(&v) == 3 * i - 7);
        assert(pddlIVecSize(&v) <= pddlIVecCap(&v));
        // Capacity grows by doubling from 8
        int c = pddlIVecCap(&v);
        assert(c >= 8 && (c & (c - 1)) == 0);
        assert(pddlIVecCap(&v) < 2 * pddlIVecSize(&v) || pddlIVecCap(&v) == 8);
    }
    assert(!pddlIVecIsEmpty(&v));
    for (int i = 0; i < 1000; ++i)
        assert(pddlIVecData(&v)[i] == 3 * i - 7);

    int cap = pddlIVecCap(&v);
    for (int i = 999; i >= 0; --i){
        assert(pddlIVecLast(&v) == 3 * i - 7);
        int x = pddlIVecPop(&v);
        assert(x == 3 * i - 7);
        assert(pddlIVecSize(&v) == i);
    }
    assert(pddlIVecIsEmpty(&v));
    assert(pddlIVecCap(&v) == cap);

    // The vector is reusable after it was emptied
    pddlIVecPush(&v, 42);
    int vals[] = { 42 };
    checkVec(&v, vals, 1);
    pddlIVecFree(&v);
}

/*
 * Get and Set access exactly the element at the given index.
 */
TEST_ONCE(ivec_get_set)
{
    PDDL_IVEC(v);
    for (int i = 0; i < 20; ++i)
        pddlIVecPush(&v, i);
    for (int i = 0; i < 20; ++i)
        pddlIVecSet(&v, i, 100 - i);
    pddlIVecSet(&v, 7, -1);
    for (int i = 0; i < 20; ++i){
        if (i == 7){
            assert(pddlIVecGet(&v, i) == -1);
        }else{
            assert(pddlIVecGet(&v, i) == 100 - i);
        }
    }
    assert(pddlIVecSize(&v) == 20);
    pddlIVecFree(&v);
}

/*
 * Insert and Remove at the front, in the middle, and at the end keep the
 * order of the other elements.
 */
TEST_ONCE(ivec_insert_remove)
{
    PDDL_IVEC(v);
    pddlIVecInsert(&v, 0, 5);
    pddlIVecInsert(&v, 0, 1);
    pddlIVecInsert(&v, 2, 9);
    pddlIVecInsert(&v, 1, 3);
    pddlIVecInsert(&v, 3, 7);
    int vals1[] = { 1, 3, 5, 7, 9 };
    checkVec(&v, vals1, 5);

    pddlIVecRemove(&v, 2);
    int vals2[] = { 1, 3, 7, 9 };
    checkVec(&v, vals2, 4);
    pddlIVecRemove(&v, 0);
    int vals3[] = { 3, 7, 9 };
    checkVec(&v, vals3, 3);
    pddlIVecRemove(&v, 2);
    int vals4[] = { 3, 7 };
    checkVec(&v, vals4, 2);
    pddlIVecRemove(&v, 1);
    pddlIVecRemove(&v, 0);
    assert(pddlIVecIsEmpty(&v));

    // Inserts that trigger reallocation, compared with a plain array
    int ref[300];
    int ref_size = 0;
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 1234);
    for (int i = 0; i < 300; ++i){
        int pos = randInt(&rnd, 0, ref_size);
        memmove(ref + pos + 1, ref + pos, sizeof(int) * (ref_size - pos));
        ref[pos] = i;
        ++ref_size;
        pddlIVecInsert(&v, pos, i);
    }
    checkVec(&v, ref, ref_size);
    while (ref_size > 0){
        int pos = randInt(&rnd, 0, ref_size - 1);
        memmove(ref + pos, ref + pos + 1, sizeof(int) * (ref_size - pos - 1));
        --ref_size;
        pddlIVecRemove(&v, pos);
        checkVec(&v, ref, ref_size);
    }
    pddlIVecFree(&v);
}

/*
 * SwapRemove replaces the removed element with the last one.
 */
TEST_ONCE(ivec_swap_remove)
{
    PDDL_IVEC(v);
    for (int i = 0; i < 5; ++i)
        pddlIVecPush(&v, 10 * i);
    pddlIVecSwapRemove(&v, 1);
    int vals1[] = { 0, 40, 20, 30 };
    checkVec(&v, vals1, 4);
    pddlIVecSwapRemove(&v, 3);
    int vals2[] = { 0, 40, 20 };
    checkVec(&v, vals2, 3);
    pddlIVecSwapRemove(&v, 0);
    int vals3[] = { 20, 40 };
    checkVec(&v, vals3, 2);
    pddlIVecSwapRemove(&v, 0);
    pddlIVecSwapRemove(&v, 0);
    assert(pddlIVecIsEmpty(&v));
    pddlIVecFree(&v);
}

/*
 * AppendArr and AppendVec add elements at the end, including empty
 * inputs and appending a vector to itself.
 */
TEST_ONCE(ivec_append)
{
    PDDL_IVEC(v);
    PDDL_IVEC(w);

    pddlIVecAppendArr(&v, NULL, 0);
    pddlIVecAppendVec(&v, &w);
    assert(pddlIVecIsEmpty(&v));
    assert(pddlIVecData(&v) == NULL);

    int arr[] = { 1, 2, 3 };
    pddlIVecAppendArr(&v, arr, 3);
    int vals1[] = { 1, 2, 3 };
    checkVec(&v, vals1, 3);

    pddlIVecPush(&w, 4);
    pddlIVecPush(&w, 5);
    pddlIVecAppendVec(&v, &w);
    int vals2[] = { 1, 2, 3, 4, 5 };
    checkVec(&v, vals2, 5);
    int vals_w[] = { 4, 5 };
    checkVec(&w, vals_w, 2);

    // Self-append, repeated so that it reallocates
    int ref[5 * 16];
    for (int i = 0; i < 5; ++i)
        ref[i] = i + 1;
    int ref_size = 5;
    for (int k = 0; k < 4; ++k){
        memcpy(ref + ref_size, ref, sizeof(int) * ref_size);
        ref_size *= 2;
        pddlIVecAppendVec(&v, &v);
        checkVec(&v, ref, ref_size);
    }

    // Many elements at once into an empty vector
    PDDL_IVEC(u);
    int big[1000];
    for (int i = 0; i < 1000; ++i)
        big[i] = i * i;
    pddlIVecAppendArr(&u, big, 1000);
    checkVec(&u, big, 1000);
    // Bulk append rounds the capacity up by doubling, not to the exact size
    assert(pddlIVecCap(&u) == 1024);

    pddlIVecFree(&u);
    pddlIVecFree(&w);
    pddlIVecFree(&v);
}

/*
 * PrependArr and PrependVec add elements at the beginning, including
 * empty inputs and prepending a vector to itself.
 */
TEST_ONCE(ivec_prepend)
{
    PDDL_IVEC(v);
    PDDL_IVEC(w);

    pddlIVecPrependArr(&v, NULL, 0);
    pddlIVecPrependVec(&v, &w);
    assert(pddlIVecIsEmpty(&v));
    assert(pddlIVecData(&v) == NULL);

    int arr[] = { 4, 5 };
    pddlIVecPrependArr(&v, arr, 2);
    int vals1[] = { 4, 5 };
    checkVec(&v, vals1, 2);

    pddlIVecPush(&w, 1);
    pddlIVecPush(&w, 2);
    pddlIVecPush(&w, 3);
    pddlIVecPrependVec(&v, &w);
    int vals2[] = { 1, 2, 3, 4, 5 };
    checkVec(&v, vals2, 5);

    // Prepend into an empty vector
    PDDL_IVEC(e);
    pddlIVecPrependVec(&e, &w);
    int vals_w[] = { 1, 2, 3 };
    checkVec(&e, vals_w, 3);

    // Self-prepend, repeated so that it reallocates
    pddlIVecPush(&v, 6);
    int ref[6 * 16];
    for (int i = 0; i < 6; ++i)
        ref[i] = i + 1;
    int ref_size = 6;
    for (int k = 0; k < 4; ++k){
        memcpy(ref + ref_size, ref, sizeof(int) * ref_size);
        ref_size *= 2;
        pddlIVecPrependVec(&v, &v);
        checkVec(&v, ref, ref_size);
    }

    // Self-prepend without reallocation
    PDDL_IVEC(s);
    pddlIVecReserve(&s, 100);
    int *data = pddlIVecData(&s);
    int arr3[] = { 7, 8, 9 };
    pddlIVecAppendArr(&s, arr3, 3);
    pddlIVecPrependVec(&s, &s);
    int vals3[] = { 7, 8, 9, 7, 8, 9 };
    checkVec(&s, vals3, 6);
    assert(pddlIVecData(&s) == data);

    pddlIVecFree(&s);
    pddlIVecFree(&e);
    pddlIVecFree(&w);
    pddlIVecFree(&v);
}

/*
 * Reserve allocates exactly the requested capacity (and never shrinks),
 * Truncate and Clear change only the size.
 */
TEST_ONCE(ivec_reserve_truncate_clear)
{
    PDDL_IVEC(v);
    pddlIVecReserve(&v, 0);
    assert(pddlIVecData(&v) == NULL && pddlIVecCap(&v) == 0);

    pddlIVecReserve(&v, 100);
    assert(pddlIVecCap(&v) == 100);
    assert(pddlIVecSize(&v) == 0);
    int *data = pddlIVecData(&v);
    for (int i = 0; i < 100; ++i)
        pddlIVecPush(&v, i);
    assert(pddlIVecData(&v) == data);
    assert(pddlIVecCap(&v) == 100);

    pddlIVecReserve(&v, 50);
    assert(pddlIVecCap(&v) == 100);

    pddlIVecPush(&v, 100);
    assert(pddlIVecCap(&v) >= 101);
    int cap = pddlIVecCap(&v);

    pddlIVecTruncate(&v, 101);
    assert(pddlIVecSize(&v) == 101);
    pddlIVecTruncate(&v, 10);
    int vals[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    checkVec(&v, vals, 10);
    assert(pddlIVecCap(&v) == cap);

    pddlIVecClear(&v);
    assert(pddlIVecIsEmpty(&v));
    assert(pddlIVecCap(&v) == cap);
    pddlIVecTruncate(&v, 0);
    assert(pddlIVecIsEmpty(&v));

    pddlIVecFree(&v);
}

/*
 * ShrinkToFit reduces the capacity exactly to the size keeping the
 * elements, frees the memory of an empty vector, and the vector remains
 * usable afterwards.
 */
TEST_ONCE(ivec_shrink_to_fit)
{
    // Zero-initialized vector: nothing to do
    PDDL_IVEC(v);
    pddlIVecShrinkToFit(&v);
    assertUnallocated(&v);

    // Reserved but empty: freed
    pddlIVecReserve(&v, 100);
    assert(pddlIVecData(&v) != NULL && pddlIVecCap(&v) == 100);
    pddlIVecShrinkToFit(&v);
    assertUnallocated(&v);
    pddlIVecPush(&v, 1);
    int vals1[] = { 1 };
    checkVec(&v, vals1, 1);
    pddlIVecClear(&v);

    // Non-empty: capacity is exactly the size, the elements are kept
    int vals[11];
    for (int i = 0; i < 10; ++i){
        vals[i] = 5 * i - 3;
        pddlIVecPush(&v, vals[i]);
    }
    assert(pddlIVecCap(&v) == 16);
    pddlIVecShrinkToFit(&v);
    assert(pddlIVecCap(&v) == 10);
    checkVec(&v, vals, 10);

    // Already tight: no reallocation
    int *data = pddlIVecData(&v);
    pddlIVecShrinkToFit(&v);
    assert(pddlIVecData(&v) == data);
    assert(pddlIVecCap(&v) == 10);

    // Growing again doubles from the shrunk capacity
    vals[10] = 77;
    pddlIVecPush(&v, vals[10]);
    assert(pddlIVecCap(&v) == 20);
    checkVec(&v, vals, 11);

    // After Truncate and Clear
    pddlIVecTruncate(&v, 3);
    pddlIVecShrinkToFit(&v);
    assert(pddlIVecCap(&v) == 3);
    checkVec(&v, vals, 3);
    pddlIVecClear(&v);
    pddlIVecShrinkToFit(&v);
    assertUnallocated(&v);

    pddlIVecFree(&v);
}

/*
 * Has and In find exactly the stored elements.
 */
TEST_ONCE(ivec_has_in)
{
    PDDL_IVEC(v);
    assert(!pddlIVecHas(&v, 0));
    assert(!pddlIVecIn(0, &v));

    int arr[] = { 5, -3, 8, 5, 0 };
    pddlIVecAppendArr(&v, arr, 5);
    for (int x = -10; x <= 10; ++x){
        pddl_bool_t has = (x == 5 || x == -3 || x == 8 || x == 0);
        assert(!pddlIVecHas(&v, x) == !has);
        assert(!pddlIVecIn(x, &v) == !has);
    }
    pddlIVecFree(&v);
}

/* Reference numeric lexicographic comparison of two arrays. */
static int refCmp(const int *a, int n, const int *b, int m)
{
    for (int i = 0; i < n && i < m; ++i){
        if (a[i] < b[i])
            return -1;
        if (a[i] > b[i])
            return 1;
    }
    if (n < m)
        return -1;
    if (n > m)
        return 1;
    return 0;
}

/*
 * Eq compares sizes and elements; Cmp is the numeric lexicographic order
 * (a proper prefix is smaller) returning exactly -1, 0, or 1.
 */
TEST_ONCE(ivec_eq_cmp)
{
    PDDL_IVEC(a);
    PDDL_IVEC(b);
    assert(pddlIVecEq(&a, &b));
    assert(pddlIVecCmp(&a, &b) == 0);

    // Empty vs. non-empty
    pddlIVecPush(&a, 1);
    assert(!pddlIVecEq(&a, &b));
    assert(pddlIVecCmp(&a, &b) == 1);
    assert(pddlIVecCmp(&b, &a) == -1);

    // Equal contents, different capacities
    pddlIVecReserve(&b, 64);
    pddlIVecPush(&b, 1);
    assert(pddlIVecEq(&a, &b));
    assert(pddlIVecCmp(&a, &b) == 0);

    // Prefix
    pddlIVecPush(&a, 2);
    pddlIVecPush(&a, 3);
    pddlIVecPush(&b, 2);
    assert(!pddlIVecEq(&a, &b));
    assert(pddlIVecCmp(&a, &b) == 1);
    assert(pddlIVecCmp(&b, &a) == -1);

    // Same size, different elements: {1, 2, 3} < {1, 2, 4}
    pddlIVecPush(&b, 4);
    assert(!pddlIVecEq(&a, &b));
    assert(pddlIVecCmp(&a, &b) == -1);
    assert(pddlIVecCmp(&b, &a) == 1);

    // The first difference decides regardless of the sizes:
    // {1, 2, 3} > {1, 1, 9, 9}
    pddlIVecClear(&b);
    int arr_b[] = { 1, 1, 9, 9 };
    pddlIVecAppendArr(&b, arr_b, 4);
    assert(pddlIVecCmp(&a, &b) == 1);
    assert(pddlIVecCmp(&b, &a) == -1);

    // Numeric order, including pairs where the byte-wise order
    // (memcmp) differs
    int pairs[][2] = {
        { 1, 256 },
        { -1, 1 },
        { -256, -1 },
        { 255, 256 },
        { INT_MIN, INT_MAX },
        { INT_MIN, 0 },
        { -1, 0 },
    };
    int num_pairs = sizeof(pairs) / sizeof(pairs[0]);
    for (int i = 0; i < num_pairs; ++i){
        pddlIVecClear(&a);
        pddlIVecClear(&b);
        pddlIVecPush(&a, pairs[i][0]);
        pddlIVecPush(&b, pairs[i][1]);
        assert(pddlIVecCmp(&a, &b) == -1);
        assert(pddlIVecCmp(&b, &a) == 1);
        assert(pddlIVecCmp(&a, &a) == 0);
    }

    // Random vectors: Eq and Cmp agree with the reference comparison
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 4321);
    for (int iter = 0; iter < 2000; ++iter){
        pddlIVecClear(&a);
        pddlIVecClear(&b);
        int n = randInt(&rnd, 0, 4);
        int m = randInt(&rnd, 0, 4);
        int range = (iter % 2 == 0 ? 2 : 300);
        for (int i = 0; i < n; ++i)
            pddlIVecPush(&a, randInt(&rnd, -range, range));
        for (int i = 0; i < m; ++i)
            pddlIVecPush(&b, randInt(&rnd, -range, range));

        int ref = refCmp(pddlIVecData(&a), n, pddlIVecData(&b), m);
        assert(pddlIVecCmp(&a, &b) == ref);
        assert(pddlIVecCmp(&b, &a) == -ref);
        assert(!pddlIVecEq(&a, &b) == !(ref == 0));
    }

    pddlIVecFree(&a);
    pddlIVecFree(&b);
}

/*
 * Reverse works for empty, one-element, even, and odd sizes.
 */
TEST_ONCE(ivec_reverse)
{
    PDDL_IVEC(v);
    pddlIVecReverse(&v);
    assert(pddlIVecIsEmpty(&v));

    for (int n = 1; n <= 9; ++n){
        pddlIVecClear(&v);
        for (int i = 0; i < n; ++i)
            pddlIVecPush(&v, i);
        pddlIVecReverse(&v);
        for (int i = 0; i < n; ++i)
            assert(pddlIVecGet(&v, i) == n - 1 - i);
        pddlIVecReverse(&v);
        for (int i = 0; i < n; ++i)
            assert(pddlIVecGet(&v, i) == i);
    }
    pddlIVecFree(&v);
}

enum sort_input {
    SORT_RAND_WIDE = 0,
    SORT_RAND_DUPS,
    SORT_SORTED,
    SORT_REV_SORTED,
    SORT_ALL_EQUAL,
    SORT_ORGAN_PIPE,
    SORT_INPUT_SIZE,
};

static void sortFill(pddl_ivec_t *v, enum sort_input in, int n,
                     pddl_rand_t *rnd)
{
    pddlIVecClear(v);
    for (int i = 0; i < n; ++i){
        switch (in){
        case SORT_RAND_WIDE:
            pddlIVecPush(v, (int)pddlRandInt(rnd));
            break;
        case SORT_RAND_DUPS:
            pddlIVecPush(v, randInt(rnd, -5, 5));
            break;
        case SORT_SORTED:
            pddlIVecPush(v, i - n / 2);
            break;
        case SORT_REV_SORTED:
            pddlIVecPush(v, n - i);
            break;
        case SORT_ALL_EQUAL:
            pddlIVecPush(v, 7);
            break;
        case SORT_ORGAN_PIPE:
            pddlIVecPush(v, (i < n / 2 ? i : n - i));
            break;
        case SORT_INPUT_SIZE:
            assert(0);
            break;
        }
    }
}

/*
 * Sort gives the same result as libc qsort() for various sizes and input
 * patterns, including INT_MIN and INT_MAX.
 */
TEST_ONCE(ivec_sort)
{
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 98765);

    PDDL_IVEC(v);
    PDDL_IVEC(ref);
    int sizes[] = { 0, 1, 2, 3, 4, 5, 15, 16, 17, 18, 31, 32, 33, 100, 257,
                    1000, 2000, 10000 };
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    for (int si = 0; si < num_sizes; ++si){
        int n = sizes[si];
        for (int in = 0; in < SORT_INPUT_SIZE; ++in){
            for (int rep = 0; rep < 3; ++rep){
                sortFill(&v, in, n, &rnd);
                if (n > 2 && rep == 1){
                    pddlIVecSet(&v, 0, INT_MAX);
                    pddlIVecSet(&v, n - 1, INT_MIN);
                }
                pddlIVecClear(&ref);
                pddlIVecAppendVec(&ref, &v);
                refSort(&ref);

                pddlIVecSort(&v);
                assert(pddlIVecEq(&v, &ref));
            }
        }
    }

    // Interleaved pattern, a hard case for the median-of-three pivots
    int n = 4096;
    pddlIVecClear(&v);
    for (int i = 0; i < n; ++i)
        pddlIVecPush(&v, (i % 2 == 0 ? i : n + i));
    pddlIVecReverse(&v);
    pddlIVecClear(&ref);
    pddlIVecAppendVec(&ref, &v);
    refSort(&ref);
    pddlIVecSort(&v);
    assert(pddlIVecEq(&v, &ref));

    pddlIVecFree(&ref);
    pddlIVecFree(&v);
}

/*
 * FOR_EACH visits all elements in order, and none of an empty vector.
 */
TEST_ONCE(ivec_for_each)
{
    PDDL_IVEC(v);
    int cnt = 0;
    PDDL_IVEC_FOR_EACH(&v, x){
        (void)x;
        ++cnt;
    }
    assert(cnt == 0);

    for (int i = 0; i < 50; ++i)
        pddlIVecPush(&v, 2 * i);
    PDDL_IVEC_FOR_EACH(&v, x){
        assert(x == 2 * cnt);
        ++cnt;
    }
    assert(cnt == 50);
    pddlIVecFree(&v);
}

/*
 * pddlIVecUniq() replaces each run of adjacent equal elements by a single
 * element (keeping non-adjacent duplicates) in place, as the unix uniq.
 */
TEST_ONCE(ivec_uniq)
{
    PDDL_IVEC(v);

    // Empty vector stays unallocated
    pddlIVecUniq(&v);
    assertUnallocated(&v);

    // Single element
    pddlIVecPush(&v, 5);
    pddlIVecUniq(&v);
    int exp1[] = { 5 };
    checkVec(&v, exp1, 1);

    // No duplicates: unchanged
    int in2[] = { 3, 1, 2, 5, 4 };
    pddlIVecClear(&v);
    pddlIVecAppendArr(&v, in2, 5);
    pddlIVecUniq(&v);
    checkVec(&v, in2, 5);

    // Runs at the start, in the middle, and at the end; the data pointer
    // and the capacity are kept
    int in3[] = { 1, 1, 2, 3, 3, 3, 4, 4 };
    pddlIVecClear(&v);
    pddlIVecAppendArr(&v, in3, 8);
    int *data = pddlIVecData(&v);
    int cap = pddlIVecCap(&v);
    pddlIVecUniq(&v);
    int exp3[] = { 1, 2, 3, 4 };
    checkVec(&v, exp3, 4);
    assert(pddlIVecData(&v) == data);
    assert(pddlIVecCap(&v) == cap);

    // All elements equal
    pddlIVecClear(&v);
    for (int i = 0; i < 10; ++i)
        pddlIVecPush(&v, -7);
    pddlIVecUniq(&v);
    int exp4[] = { -7 };
    checkVec(&v, exp4, 1);

    // Non-adjacent duplicates are kept
    int in5[] = { 1, 1, 2, 1, 1 };
    pddlIVecClear(&v);
    pddlIVecAppendArr(&v, in5, 5);
    pddlIVecUniq(&v);
    int exp5[] = { 1, 2, 1 };
    checkVec(&v, exp5, 3);

    // Random vectors with many adjacent duplicates against a naive
    // reference
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 4242);
    PDDL_IVEC(ref);
    for (int rep = 0; rep < 200; ++rep){
        int n = randInt(&rnd, 0, 100);
        pddlIVecClear(&v);
        pddlIVecClear(&ref);
        for (int i = 0; i < n; ++i){
            int x = randInt(&rnd, 0, 3);
            pddlIVecPush(&v, x);
            if (pddlIVecIsEmpty(&ref) || pddlIVecLast(&ref) != x)
                pddlIVecPush(&ref, x);
        }
        pddlIVecUniq(&v);
        assert(pddlIVecEq(&v, &ref));
    }

    pddlIVecFree(&ref);
    pddlIVecFree(&v);
}
