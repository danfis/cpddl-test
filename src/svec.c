/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/svec.h"
#include "pddl/rand.h"
#include "test.h"
#include <assert.h>

#define POOL_SIZE 2048

/* Pool of distinct strings "s<i>" at distinct addresses. */
static char pool[POOL_SIZE][8];
static int pool_init = 0;

/* Returns the pool string with the index i. */
static const char *str(int i)
{
    if (!pool_init){
        for (int j = 0; j < POOL_SIZE; ++j)
            snprintf(pool[j], sizeof(pool[j]), "s%d", j);
        pool_init = 1;
    }
    assert(i >= 0 && i < POOL_SIZE);
    return pool[i];
}

/*
 * Check that v contains exactly the given pointers in the given order
 * (i.e., the strings were not copied).
 */
static void checkVec(const pddl_svec_t *v, const char *const *vals, int n)
{
    assert(pddlSVecSize(v) == n);
    assert(v->size <= v->cap);
    for (int i = 0; i < n; ++i)
        assert(pddlSVecGet(v, i) == vals[i]);
}

/* Bounded random integer in [lo, hi]. */
static int randInt(pddl_rand_t *rnd, int lo, int hi)
{
    return lo + (int)(pddlRandInt(rnd) % (unsigned)(hi - lo + 1));
}

/* Reference comparison of strings: NULL is smaller than any string. */
static int refStrCmp(const char *a, const char *b)
{
    if (a == NULL && b == NULL)
        return 0;
    if (a == NULL)
        return -1;
    if (b == NULL)
        return 1;
    int cmp = strcmp(a, b);
    if (cmp < 0)
        return -1;
    if (cmp > 0)
        return 1;
    return 0;
}

/* Reference lexicographic comparison of two arrays of strings. */
static int refCmp(const char *const *a, int n, const char *const *b, int m)
{
    for (int i = 0; i < n && i < m; ++i){
        int cmp = refStrCmp(a[i], b[i]);
        if (cmp != 0)
            return cmp;
    }
    if (n < m)
        return -1;
    if (n > m)
        return 1;
    return 0;
}

static int cmpPtr(const void *a, const void *b)
{
    uintptr_t x = (uintptr_t)*(const char *const *)a;
    uintptr_t y = (uintptr_t)*(const char *const *)b;
    return (x > y) - (x < y);
}

/*
 * Both the static initializer and pddlSVecInit() give an empty vector
 * without any allocation, and freeing it is safe.
 */
TEST_ONCE(svec_init_free)
{
    PDDL_SVEC(a);
    assert(a.v == NULL && a.size == 0 && a.cap == 0);
    assert(pddlSVecIsEmpty(&a));
    assert(pddlSVecSize(&a) == 0);
    assert(pddlSVecData(&a) == NULL);
    pddlSVecFree(&a);

    pddl_svec_t b;
    memset(&b, 0x5a, sizeof(b));
    pddlSVecInit(&b);
    assert(b.v == NULL && b.size == 0 && b.cap == 0);
    pddlSVecFreeAll(&b);
}

/*
 * Push grows the vector by doubling from 8, Last and Pop return the
 * stored pointers in the reverse order.
 */
TEST_ONCE(svec_push_pop)
{
    PDDL_SVEC(v);
    for (int i = 0; i < 1000; ++i){
        pddlSVecPush(&v, str(i));
        assert(pddlSVecSize(&v) == i + 1);
        assert(pddlSVecLast(&v) == str(i));
        assert(v.cap >= 8 && (v.cap & (v.cap - 1)) == 0);
        assert(v.cap < 2 * v.size || v.cap == 8);
    }
    assert(!pddlSVecIsEmpty(&v));
    for (int i = 0; i < 1000; ++i)
        assert(pddlSVecData(&v)[i] == str(i));

    int cap = v.cap;
    for (int i = 999; i >= 0; --i){
        assert(pddlSVecLast(&v) == str(i));
        const char *s = pddlSVecPop(&v);
        assert(s == str(i));
        assert(pddlSVecSize(&v) == i);
    }
    assert(pddlSVecIsEmpty(&v));
    assert(v.cap == cap);

    // NULL elements can be stored
    pddlSVecPush(&v, NULL);
    pddlSVecPush(&v, "x");
    const char *vals[] = { NULL, "x" };
    checkVec(&v, vals, 2);
    pddlSVecFree(&v);
}

/*
 * The vector stores borrowed pointers: nothing is copied by Push, Set,
 * Insert, or Append, and Pop returns the same pointer.
 */
TEST_ONCE(svec_borrowed)
{
    char buf[] = "hello";
    PDDL_SVEC(v);
    pddlSVecPush(&v, buf);
    assert(pddlSVecGet(&v, 0) == buf);

    // A change of the buffer is visible through the vector
    buf[0] = 'j';
    assert(strcmp(pddlSVecGet(&v, 0), "jello") == 0);

    pddlSVecSet(&v, 0, str(1));
    assert(pddlSVecGet(&v, 0) == str(1));
    pddlSVecInsert(&v, 0, buf);
    const char *arr[] = { str(2), str(3) };
    pddlSVecAppendArr(&v, arr, 2);
    const char *vals[] = { buf, str(1), str(2), str(3) };
    checkVec(&v, vals, 4);
    assert(pddlSVecPop(&v) == str(3));
    pddlSVecFree(&v);
}

/*
 * Get and Set access exactly the element at the given index.
 */
TEST_ONCE(svec_get_set)
{
    PDDL_SVEC(v);
    for (int i = 0; i < 20; ++i)
        pddlSVecPush(&v, str(i));
    for (int i = 0; i < 20; ++i)
        pddlSVecSet(&v, i, str(100 - i));
    pddlSVecSet(&v, 7, NULL);
    for (int i = 0; i < 20; ++i){
        if (i == 7){
            assert(pddlSVecGet(&v, i) == NULL);
        }else{
            assert(pddlSVecGet(&v, i) == str(100 - i));
        }
    }
    assert(pddlSVecSize(&v) == 20);
    pddlSVecFree(&v);
}

/*
 * Insert and Remove at the front, in the middle, and at the end keep the
 * order of the other elements.
 */
TEST_ONCE(svec_insert_remove)
{
    PDDL_SVEC(v);
    pddlSVecInsert(&v, 0, "c");
    pddlSVecInsert(&v, 0, "a");
    pddlSVecInsert(&v, 2, "e");
    pddlSVecInsert(&v, 1, "b");
    pddlSVecInsert(&v, 3, "d");
    assert(pddlSVecSize(&v) == 5);
    const char *exp = "abcde";
    for (int i = 0; i < 5; ++i){
        assert(pddlSVecGet(&v, i)[0] == exp[i]);
        assert(pddlSVecGet(&v, i)[1] == '\0');
    }
    pddlSVecClear(&v);

    // Random inserts and removes compared with a plain array
    const char *ref[300];
    int ref_size = 0;
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 1234);
    for (int i = 0; i < 300; ++i){
        int pos = randInt(&rnd, 0, ref_size);
        memmove(ref + pos + 1, ref + pos,
                sizeof(const char *) * (ref_size - pos));
        ref[pos] = str(i);
        ++ref_size;
        pddlSVecInsert(&v, pos, str(i));
    }
    checkVec(&v, ref, ref_size);
    while (ref_size > 0){
        int pos = randInt(&rnd, 0, ref_size - 1);
        memmove(ref + pos, ref + pos + 1,
                sizeof(const char *) * (ref_size - pos - 1));
        --ref_size;
        pddlSVecRemove(&v, pos);
        checkVec(&v, ref, ref_size);
    }
    pddlSVecFree(&v);
}

/*
 * SwapRemove replaces the removed element with the last one.
 */
TEST_ONCE(svec_swap_remove)
{
    PDDL_SVEC(v);
    for (int i = 0; i < 5; ++i)
        pddlSVecPush(&v, str(i));
    pddlSVecSwapRemove(&v, 1);
    const char *vals1[] = { str(0), str(4), str(2), str(3) };
    checkVec(&v, vals1, 4);
    pddlSVecSwapRemove(&v, 3);
    const char *vals2[] = { str(0), str(4), str(2) };
    checkVec(&v, vals2, 3);
    pddlSVecSwapRemove(&v, 0);
    const char *vals3[] = { str(2), str(4) };
    checkVec(&v, vals3, 2);
    pddlSVecSwapRemove(&v, 0);
    pddlSVecSwapRemove(&v, 0);
    assert(pddlSVecIsEmpty(&v));
    pddlSVecFree(&v);
}

/*
 * AppendArr and AppendVec add elements at the end, including empty
 * inputs and appending a vector to itself.
 */
TEST_ONCE(svec_append)
{
    PDDL_SVEC(v);
    PDDL_SVEC(w);

    pddlSVecAppendArr(&v, NULL, 0);
    pddlSVecAppendVec(&v, &w);
    assert(pddlSVecIsEmpty(&v));
    assert(v.v == NULL);

    const char *arr[] = { str(1), str(2), str(3) };
    pddlSVecAppendArr(&v, arr, 3);
    checkVec(&v, arr, 3);

    pddlSVecPush(&w, str(4));
    pddlSVecPush(&w, NULL);
    pddlSVecAppendVec(&v, &w);
    const char *vals2[] = { str(1), str(2), str(3), str(4), NULL };
    checkVec(&v, vals2, 5);
    const char *vals_w[] = { str(4), NULL };
    checkVec(&w, vals_w, 2);

    // Data() of another vector can be passed as an array
    PDDL_SVEC(u);
    pddlSVecAppendArr(&u, pddlSVecData(&v), pddlSVecSize(&v));
    checkVec(&u, vals2, 5);

    // Self-append, repeated so that it reallocates
    const char *ref[5 * 16];
    memcpy(ref, vals2, sizeof(vals2));
    int ref_size = 5;
    for (int k = 0; k < 4; ++k){
        memcpy(ref + ref_size, ref, sizeof(const char *) * ref_size);
        ref_size *= 2;
        pddlSVecAppendVec(&v, &v);
        checkVec(&v, ref, ref_size);
    }

    pddlSVecFree(&u);
    pddlSVecFree(&w);
    pddlSVecFree(&v);
}

/*
 * PrependArr and PrependVec add elements at the beginning, including
 * empty inputs and prepending a vector to itself.
 */
TEST_ONCE(svec_prepend)
{
    PDDL_SVEC(v);
    PDDL_SVEC(w);

    pddlSVecPrependArr(&v, NULL, 0);
    pddlSVecPrependVec(&v, &w);
    assert(pddlSVecIsEmpty(&v));
    assert(v.v == NULL);

    const char *arr[] = { str(4), str(5) };
    pddlSVecPrependArr(&v, arr, 2);
    checkVec(&v, arr, 2);

    pddlSVecPush(&w, str(1));
    pddlSVecPush(&w, str(2));
    pddlSVecPush(&w, str(3));
    pddlSVecPrependVec(&v, &w);
    const char *vals2[] = { str(1), str(2), str(3), str(4), str(5) };
    checkVec(&v, vals2, 5);

    // Prepend into an empty vector
    PDDL_SVEC(e);
    pddlSVecPrependVec(&e, &w);
    checkVec(&e, vals2, 3);

    // Self-prepend, repeated so that it reallocates
    pddlSVecPush(&v, str(6));
    const char *ref[6 * 16];
    for (int i = 0; i < 6; ++i)
        ref[i] = str(i + 1);
    int ref_size = 6;
    for (int k = 0; k < 4; ++k){
        memcpy(ref + ref_size, ref, sizeof(const char *) * ref_size);
        ref_size *= 2;
        pddlSVecPrependVec(&v, &v);
        checkVec(&v, ref, ref_size);
    }

    // Self-prepend without reallocation
    PDDL_SVEC(s);
    pddlSVecReserve(&s, 100);
    const char **data = pddlSVecData(&s);
    const char *arr3[] = { str(7), str(8), str(9) };
    pddlSVecAppendArr(&s, arr3, 3);
    pddlSVecPrependVec(&s, &s);
    const char *vals3[] = { str(7), str(8), str(9), str(7), str(8), str(9) };
    checkVec(&s, vals3, 6);
    assert(pddlSVecData(&s) == data);

    pddlSVecFree(&s);
    pddlSVecFree(&e);
    pddlSVecFree(&w);
    pddlSVecFree(&v);
}

/*
 * Reserve allocates exactly the requested capacity (and never shrinks),
 * Truncate and Clear change only the size.
 */
TEST_ONCE(svec_reserve_truncate_clear)
{
    PDDL_SVEC(v);
    pddlSVecReserve(&v, 0);
    assert(v.v == NULL && v.cap == 0);

    pddlSVecReserve(&v, 100);
    assert(v.cap == 100);
    assert(pddlSVecSize(&v) == 0);
    const char **data = pddlSVecData(&v);
    for (int i = 0; i < 100; ++i)
        pddlSVecPush(&v, str(i));
    assert(pddlSVecData(&v) == data);
    assert(v.cap == 100);

    pddlSVecReserve(&v, 50);
    assert(v.cap == 100);

    pddlSVecPush(&v, str(100));
    assert(v.cap == 200);
    int cap = v.cap;

    pddlSVecTruncate(&v, 101);
    assert(pddlSVecSize(&v) == 101);
    pddlSVecTruncate(&v, 10);
    assert(pddlSVecSize(&v) == 10);
    for (int i = 0; i < 10; ++i)
        assert(pddlSVecGet(&v, i) == str(i));
    assert(v.cap == cap);

    pddlSVecClear(&v);
    assert(pddlSVecIsEmpty(&v));
    assert(v.cap == cap);
    pddlSVecTruncate(&v, 0);
    assert(pddlSVecIsEmpty(&v));

    pddlSVecFree(&v);
}

/*
 * ShrinkToFit reduces the capacity exactly to the size keeping the
 * elements, frees the memory of an empty vector, and the vector remains
 * usable afterwards.
 */
TEST_ONCE(svec_shrink_to_fit)
{
    PDDL_SVEC(v);
    pddlSVecShrinkToFit(&v);
    assert(v.v == NULL && v.size == 0 && v.cap == 0);

    pddlSVecReserve(&v, 100);
    pddlSVecShrinkToFit(&v);
    assert(v.v == NULL && v.size == 0 && v.cap == 0);

    const char *vals[11];
    for (int i = 0; i < 10; ++i){
        vals[i] = str(i);
        pddlSVecPush(&v, vals[i]);
    }
    assert(v.cap == 16);
    pddlSVecShrinkToFit(&v);
    assert(v.cap == 10);
    checkVec(&v, vals, 10);

    const char **data = pddlSVecData(&v);
    pddlSVecShrinkToFit(&v);
    assert(pddlSVecData(&v) == data);

    vals[10] = str(10);
    pddlSVecPush(&v, vals[10]);
    assert(v.cap == 20);
    checkVec(&v, vals, 11);

    pddlSVecClear(&v);
    pddlSVecShrinkToFit(&v);
    assert(v.v == NULL && v.size == 0 && v.cap == 0);
    pddlSVecFree(&v);
}

/*
 * PushDup stores copies (distinct pointers with equal contents, NULL stays
 * NULL), which are released by FreeAll.
 */
TEST_ONCE(svec_push_dup_free_all)
{
    PDDL_SVEC(v);
    char buf[] = "abc";
    pddlSVecPushDup(&v, buf);
    pddlSVecPushDup(&v, NULL);
    pddlSVecPushDup(&v, "");
    pddlSVecPushDup(&v, str(42));
    assert(pddlSVecSize(&v) == 4);

    assert(pddlSVecGet(&v, 0) != buf);
    assert(strcmp(pddlSVecGet(&v, 0), "abc") == 0);
    buf[0] = 'x';
    assert(strcmp(pddlSVecGet(&v, 0), "abc") == 0);
    assert(pddlSVecGet(&v, 1) == NULL);
    assert(strcmp(pddlSVecGet(&v, 2), "") == 0);
    assert(pddlSVecGet(&v, 3) != str(42));
    assert(strcmp(pddlSVecGet(&v, 3), "s42") == 0);

    pddlSVecFreeAll(&v);

    // Copies of many strings across reallocations
    PDDL_SVEC(w);
    for (int i = 0; i < 100; ++i)
        pddlSVecPushDup(&w, str(i));
    for (int i = 0; i < 100; ++i){
        assert(pddlSVecGet(&w, i) != str(i));
        assert(strcmp(pddlSVecGet(&w, i), str(i)) == 0);
    }
    pddlSVecFreeAll(&w);
}

/*
 * Has and In compare the contents of the strings: equal strings stored in
 * different buffers are found, NULL is found only if NULL is stored.
 */
TEST_ONCE(svec_has_in)
{
    PDDL_SVEC(v);
    assert(!pddlSVecHas(&v, "a"));
    assert(!pddlSVecHas(&v, NULL));
    assert(!pddlSVecIn("a", &v));

    pddlSVecPush(&v, "foo");
    pddlSVecPush(&v, "");
    pddlSVecPush(&v, str(5));

    char buf[] = "foo";
    assert(pddlSVecHas(&v, buf));
    assert(pddlSVecIn(buf, &v));
    assert(pddlSVecHas(&v, ""));
    assert(pddlSVecHas(&v, "s5"));
    assert(pddlSVecHas(&v, str(5)));
    assert(!pddlSVecHas(&v, "fo"));
    assert(!pddlSVecHas(&v, "foo "));
    assert(!pddlSVecHas(&v, "s6"));
    assert(!pddlSVecHas(&v, NULL));
    assert(!pddlSVecIn(NULL, &v));

    pddlSVecPush(&v, NULL);
    assert(pddlSVecHas(&v, NULL));
    assert(pddlSVecIn(NULL, &v));
    assert(pddlSVecHas(&v, buf));
    pddlSVecFree(&v);
}

/*
 * Eq compares sizes and the contents of the strings; Cmp is the
 * lexicographic order of the strings (NULL first, a proper prefix is
 * smaller) returning exactly -1, 0, or 1.
 */
TEST_ONCE(svec_eq_cmp)
{
    PDDL_SVEC(a);
    PDDL_SVEC(b);
    assert(pddlSVecEq(&a, &b));
    assert(pddlSVecCmp(&a, &b) == 0);

    // Empty vs. non-empty
    pddlSVecPush(&a, "x");
    assert(!pddlSVecEq(&a, &b));
    assert(pddlSVecCmp(&a, &b) == 1);
    assert(pddlSVecCmp(&b, &a) == -1);

    // Equal contents in different buffers
    char buf[] = "x";
    pddlSVecPush(&b, buf);
    assert(pddlSVecEq(&a, &b));
    assert(pddlSVecCmp(&a, &b) == 0);

    // Prefix
    pddlSVecPush(&a, "y");
    assert(!pddlSVecEq(&a, &b));
    assert(pddlSVecCmp(&a, &b) == 1);
    assert(pddlSVecCmp(&b, &a) == -1);

    // The first difference decides: {"x", "y"} > {"x", "b", "z"}
    pddlSVecPush(&b, "b");
    pddlSVecPush(&b, "z");
    assert(pddlSVecCmp(&a, &b) == 1);
    assert(pddlSVecCmp(&b, &a) == -1);

    // Order of single strings, NULL < "" < any non-empty string
    const char *pairs[][2] = {
        { NULL, "" },
        { NULL, "a" },
        { "", "a" },
        { "a", "ab" },
        { "ab", "b" },
        { "B", "a" },
        { "s10", "s9" },
    };
    int num_pairs = sizeof(pairs) / sizeof(pairs[0]);
    for (int i = 0; i < num_pairs; ++i){
        pddlSVecClear(&a);
        pddlSVecClear(&b);
        pddlSVecPush(&a, pairs[i][0]);
        pddlSVecPush(&b, pairs[i][1]);
        assert(!pddlSVecEq(&a, &b));
        assert(pddlSVecCmp(&a, &b) == -1);
        assert(pddlSVecCmp(&b, &a) == 1);
        assert(pddlSVecCmp(&a, &a) == 0);
        assert(pddlSVecEq(&a, &a));
    }

    // NULL equals NULL
    pddlSVecClear(&a);
    pddlSVecClear(&b);
    pddlSVecPush(&a, NULL);
    pddlSVecPush(&b, NULL);
    assert(pddlSVecEq(&a, &b));
    assert(pddlSVecCmp(&a, &b) == 0);

    // Random vectors over a small alphabet (each string in two different
    // buffers, plus NULL) agree with the reference comparison
    static char alpha[2][4][3] = {
        { "", "a", "ab", "b" },
        { "", "a", "ab", "b" },
    };
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 4321);
    for (int iter = 0; iter < 2000; ++iter){
        pddlSVecClear(&a);
        pddlSVecClear(&b);
        int n = randInt(&rnd, 0, 4);
        int m = randInt(&rnd, 0, 4);
        for (int i = 0; i < n + m; ++i){
            int x = randInt(&rnd, -1, 3);
            const char *s = (x < 0 ? NULL : alpha[randInt(&rnd, 0, 1)][x]);
            pddlSVecPush((i < n ? &a : &b), s);
        }

        int ref = refCmp(a.v, n, b.v, m);
        assert(pddlSVecCmp(&a, &b) == ref);
        assert(pddlSVecCmp(&b, &a) == -ref);
        assert(!pddlSVecEq(&a, &b) == !(ref == 0));
    }

    pddlSVecFree(&a);
    pddlSVecFree(&b);
}

/*
 * Reverse works for empty, one-element, even, and odd sizes.
 */
TEST_ONCE(svec_reverse)
{
    PDDL_SVEC(v);
    pddlSVecReverse(&v);
    assert(pddlSVecIsEmpty(&v));

    for (int n = 1; n <= 9; ++n){
        pddlSVecClear(&v);
        for (int i = 0; i < n; ++i)
            pddlSVecPush(&v, str(i));
        pddlSVecReverse(&v);
        for (int i = 0; i < n; ++i)
            assert(pddlSVecGet(&v, i) == str(n - 1 - i));
        pddlSVecReverse(&v);
        for (int i = 0; i < n; ++i)
            assert(pddlSVecGet(&v, i) == str(i));
    }
    pddlSVecFree(&v);
}

/*
 * Sort orders the strings by contents (NULL first) and only permutes the
 * stored pointers; checked on random inputs with duplicates (also in
 * different buffers) and NULLs, and on sorted and reverse-sorted inputs.
 */
TEST_ONCE(svec_sort)
{
    static char dups[16][8];
    for (int i = 0; i < 16; ++i)
        snprintf(dups[i], sizeof(dups[i]), "s%d", i);

    pddl_rand_t rnd;
    pddlRandInit(&rnd, 98765);

    PDDL_SVEC(v);
    PDDL_SVEC(orig);
    PDDL_SVEC(sorted);
    int sizes[] = { 0, 1, 2, 3, 4, 5, 15, 16, 17, 33, 100, 257, 1000, 2000 };
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    for (int si = 0; si < num_sizes; ++si){
        int n = sizes[si];
        for (int in = 0; in < 4; ++in){
            pddlSVecClear(&v);
            for (int i = 0; i < n; ++i){
                if (in == 0){
                    pddlSVecPush(&v, str(randInt(&rnd, 0, POOL_SIZE - 1)));
                }else if (in == 1){
                    int x = randInt(&rnd, -1, 31);
                    if (x < 0){
                        pddlSVecPush(&v, NULL);
                    }else if (x < 16){
                        pddlSVecPush(&v, str(x));
                    }else{
                        pddlSVecPush(&v, dups[x - 16]);
                    }
                }else if (in == 2){
                    pddlSVecPush(&v, str(i));
                }else{
                    pddlSVecPush(&v, str(n - 1 - i));
                }
            }

            pddlSVecClear(&orig);
            pddlSVecAppendVec(&orig, &v);
            pddlSVecSort(&v);
            assert(pddlSVecSize(&v) == n);
            for (int i = 1; i < n; ++i)
                assert(refStrCmp(v.v[i - 1], v.v[i]) <= 0);

            // The same multiset of pointers
            pddlSVecClear(&sorted);
            pddlSVecAppendVec(&sorted, &v);
            if (n > 0){
                qsort(orig.v, n, sizeof(const char *), cmpPtr);
                qsort(sorted.v, n, sizeof(const char *), cmpPtr);
            }
            for (int i = 0; i < n; ++i)
                assert(orig.v[i] == sorted.v[i]);
        }
    }

    pddlSVecFree(&sorted);
    pddlSVecFree(&orig);
    pddlSVecFree(&v);
}

/*
 * FOR_EACH visits all elements in order, and none of an empty vector.
 */
TEST_ONCE(svec_for_each)
{
    PDDL_SVEC(v);
    int cnt = 0;
    PDDL_SVEC_FOR_EACH(&v, s){
        (void)s;
        ++cnt;
    }
    assert(cnt == 0);

    for (int i = 0; i < 50; ++i)
        pddlSVecPush(&v, (i == 10 ? NULL : str(i)));
    PDDL_SVEC_FOR_EACH(&v, s){
        if (cnt == 10){
            assert(s == NULL);
        }else{
            assert(s == str(cnt));
        }
        ++cnt;
    }
    assert(cnt == 50);
    pddlSVecFree(&v);
}
