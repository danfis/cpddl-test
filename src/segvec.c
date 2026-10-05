/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the segmented vector (pddl/segvec.h).
 */

#include "pddl/segvec.h"
#include "test.h"
#include <assert.h>

/* Sizes of the first segment every layout test is run with. */
static const int first_segm_sizes[] = { 1, 2, 8, 1024 };
#define NUM_FIRST_SEGM_SIZES \
    ((int)(sizeof(first_segm_sizes) / sizeof(first_segm_sizes[0])))

/* Recording state of initCb(). */
struct init_rec {
    /* Number of calls of the callback */
    int calls;
    /* Index passed to the last call */
    int last_idx;
};
typedef struct init_rec init_rec_t;

/* Initializes the element to 1000 + idx and records the call. The indexes
 * must be passed in ascending order without gaps. */
static void initCb(void *el, int idx, void *userdata)
{
    init_rec_t *rec = userdata;
    assert(idx == rec->last_idx + 1);
    rec->last_idx = idx;
    ++rec->calls;
    *(int *)el = 1000 + idx;
}

/* Number of elements in the first NUM_SEGM segments. */
static size_t capacityOf(const pddl_segvec_t *sv, int num_segm)
{
    size_t cap = 0;
    for (int k = 0; k < num_segm; ++k)
        cap += pddlSegVecSegmentSize(sv, k);
    return cap;
}

/* Stores I into the element I for I = 0, ..., SIZE-1 via pddlSegVecGet(). */
static void fillInts(pddl_segvec_t *sv, int size)
{
    for (int i = 0; i < size; ++i){
        int *el = pddlSegVecGet(sv, i);
        *el = i;
    }
}

/* Checks that the elements 0, ..., SIZE-1 hold the values set by fillInts(). */
static void checkInts(const pddl_segvec_t *sv, int size)
{
    for (int i = 0; i < size; ++i){
        const int *el = pddlSegVecGetConst(sv, i);
        assert(el != NULL);
        assert(*el == i);
    }
}

/*
 * Checks the translation of indexes to segments against the expected layout:
 * every segment is filled in turn, the segment IDs and the capacity must
 * change exactly on the segment boundaries, and every element must keep its
 * value.
 */
static void checkLayout(pddl_bool_t exp, int b)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), exp, b);
    assert(pddlSegVecSize(&sv) == 0);
    assert(pddlSegVecCapacity(&sv) == 0);
    assert(pddlSegVecNumSegments(&sv) == 0);
    assert(pddlSegVecElSize(&sv) == sizeof(int));
    assert(pddlSegVecGetConst(&sv, 0) == NULL);

    // expected segment sizes
    for (int k = 0; k < 12; ++k){
        int expected = b;
        if (exp && k > 0)
            expected = b << (k - 1);
        assert(pddlSegVecSegmentSize(&sv, k) == expected);
    }

    const int num_segm = 12;
    int first = 0;
    for (int k = 0; k < num_segm; ++k){
        int segm_size = pddlSegVecSegmentSize(&sv, k);
        // the segment ID is valid even for indexes not allocated yet
        assert(pddlSegVecSegmentId(&sv, first) == k);
        assert(pddlSegVecSegmentId(&sv, first + segm_size - 1) == k);
        if (first > 0)
            assert(pddlSegVecSegmentId(&sv, first - 1) == k - 1);

        for (int i = first; i < first + segm_size; ++i){
            int *el = pddlSegVecGet(&sv, i);
            *el = i;
            assert(pddlSegVecSize(&sv) == i + 1);
            assert(pddlSegVecNumSegments(&sv) == k + 1);
            assert(pddlSegVecCapacity(&sv) == capacityOf(&sv, k + 1));
        }
        // the elements of one segment are contiguous in memory
        const int *el0 = pddlSegVecGetConst(&sv, first);
        const int *el1 = pddlSegVecGetConst(&sv, first + segm_size - 1);
        assert(el1 - el0 == segm_size - 1);

        first += segm_size;
    }
    assert(pddlSegVecCapacity(&sv) == (size_t)first);
    checkInts(&sv, first);
    assert(pddlSegVecGetConst(&sv, first) == NULL);
    assert(pddlSegVecGetConst(&sv, -1) == NULL);

    pddlSegVecFree(&sv);
}

/*
 * Fixed layout: all segments have the same size and indexes map to
 * segments by a shift.
 */
TEST_ONCE(segvec_fixed_layout)
{
    for (int bi = 0; bi < NUM_FIRST_SEGM_SIZES; ++bi)
        checkLayout(pddl_false, first_segm_sizes[bi]);

    // segment IDs of large indexes
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), pddl_false, 8);
    assert(pddlSegVecSegmentId(&sv, INT_MAX) == INT_MAX / 8);
    assert(pddlSegVecSegmentId(&sv, INT_MAX - 7) == INT_MAX / 8);
    assert(pddlSegVecSegmentId(&sv, INT_MAX - 8) == INT_MAX / 8 - 1);
    assert(pddlSegVecSegmentId(&sv, 1 << 30) == (1 << 30) / 8);
    pddlSegVecFree(&sv);

    pddlSegVecInit(&sv, sizeof(int), pddl_false, 1);
    assert(pddlSegVecSegmentId(&sv, INT_MAX) == INT_MAX);
    pddlSegVecFree(&sv);

    pddlSegVecInit(&sv, sizeof(int), pddl_false, 1 << 30);
    assert(pddlSegVecSegmentSize(&sv, 0) == 1 << 30);
    assert(pddlSegVecSegmentId(&sv, (1 << 30) - 1) == 0);
    assert(pddlSegVecSegmentId(&sv, 1 << 30) == 1);
    assert(pddlSegVecSegmentId(&sv, INT_MAX) == 1);
    pddlSegVecFree(&sv);
}

/*
 * Exponential layout: segments of sizes b, b, 2b, 4b, ... including the
 * mapping of the largest int indexes.
 */
TEST_ONCE(segvec_exp_layout)
{
    for (int bi = 0; bi < NUM_FIRST_SEGM_SIZES; ++bi)
        checkLayout(pddl_true, first_segm_sizes[bi]);

    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), pddl_true, 8);
    // the segment k >= 1 starts at 8 * 2^(k-1) = 2^(k+2)
    for (int k = 1; k <= 28; ++k){
        int start = 1 << (k + 2);
        assert(pddlSegVecSegmentSize(&sv, k) == start);
        assert(pddlSegVecSegmentId(&sv, start) == k);
        assert(pddlSegVecSegmentId(&sv, start - 1) == k - 1);
    }
    // the last segment covers [2^30, 2^31)
    assert(pddlSegVecSegmentId(&sv, INT_MAX) == 28);
    assert(pddlSegVecSegmentSize(&sv, 28) == 1 << 30);
    pddlSegVecFree(&sv);

    pddlSegVecInit(&sv, sizeof(int), pddl_true, 1);
    assert(pddlSegVecSegmentId(&sv, 0) == 0);
    assert(pddlSegVecSegmentId(&sv, 1) == 1);
    assert(pddlSegVecSegmentId(&sv, 2) == 2);
    assert(pddlSegVecSegmentId(&sv, 3) == 2);
    assert(pddlSegVecSegmentId(&sv, 4) == 3);
    assert(pddlSegVecSegmentId(&sv, INT_MAX) == 31);
    assert(pddlSegVecSegmentSize(&sv, 31) == 1 << 30);
    pddlSegVecFree(&sv);

    pddlSegVecInit(&sv, sizeof(int), pddl_true, 1 << 30);
    assert(pddlSegVecSegmentId(&sv, (1 << 30) - 1) == 0);
    assert(pddlSegVecSegmentId(&sv, 1 << 30) == 1);
    assert(pddlSegVecSegmentId(&sv, INT_MAX) == 1);
    assert(pddlSegVecSegmentSize(&sv, 1) == 1 << 30);
    pddlSegVecFree(&sv);
}

/* Checks pushing, popping and the top element for one layout. */
static void checkPushPopTop(pddl_bool_t exp, int b)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), exp, b);
    assert(pddlSegVecTop(&sv) == NULL);

    const int size = 3000;
    for (int i = 0; i < size; ++i){
        int idx = -1;
        int *el = pddlSegVecPush(&sv, &idx);
        assert(idx == i);
        *el = i;
        assert(pddlSegVecSize(&sv) == i + 1);
        assert(pddlSegVecTop(&sv) == el);
        assert(pddlSegVecGet(&sv, i) == el);
        assert(pddlSegVecCapacity(&sv) >= (size_t)(i + 1));
    }
    // the index is optional
    int *el = pddlSegVecPush(&sv, NULL);
    *el = size;
    checkInts(&sv, size + 1);

    size_t capacity = pddlSegVecCapacity(&sv);
    int num_segm = pddlSegVecNumSegments(&sv);
    for (int i = size; i >= 0; --i){
        const int *top = pddlSegVecTop(&sv);
        assert(*top == i);
        int new_size = pddlSegVecPop(&sv);
        assert(new_size == i);
        assert(pddlSegVecSize(&sv) == i);
        assert(pddlSegVecGetConst(&sv, i) == NULL);
    }
    assert(pddlSegVecSize(&sv) == 0);
    assert(pddlSegVecTop(&sv) == NULL);
    // the memory is kept
    assert(pddlSegVecCapacity(&sv) == capacity);
    assert(pddlSegVecNumSegments(&sv) == num_segm);

    // and reused by the next pushes
    for (int i = 0; i < size; ++i){
        int idx;
        int *el = pddlSegVecPush(&sv, &idx);
        assert(idx == i);
        *el = i;
    }
    checkInts(&sv, size);
    assert(pddlSegVecCapacity(&sv) == capacity);
    assert(pddlSegVecNumSegments(&sv) == num_segm);

    pddlSegVecFree(&sv);
}

/*
 * Push returns sequential indexes, top and pop work on the last element,
 * and popping keeps the memory for later pushes.
 */
TEST_ONCE(segvec_push_pop_top)
{
    for (int bi = 0; bi < NUM_FIRST_SEGM_SIZES; ++bi){
        checkPushPopTop(pddl_false, first_segm_sizes[bi]);
        checkPushPopTop(pddl_true, first_segm_sizes[bi]);
    }
}

/* Checks pddlSegVecPopN() for one layout. */
static void checkPopN(pddl_bool_t exp, int first_segm_size)
{
    int init = -1;
    pddl_segvec_t sv;
    pddlSegVecInitDefault(&sv, sizeof(int), exp, first_segm_size,
                          &init, NULL, NULL);

    // popping nothing from an empty vector is fine
    assert(pddlSegVecPopN(&sv, 0) == 0);

    const int size = 3000;
    fillInts(&sv, size);
    size_t capacity = pddlSegVecCapacity(&sv);
    int num_segm = pddlSegVecNumSegments(&sv);

    assert(pddlSegVecPopN(&sv, 0) == size);
    assert(pddlSegVecSize(&sv) == size);

    // pop across segment boundaries
    int cur = size;
    const int steps[] = { 1, 7, 100, 1000 };
    for (int si = 0; si < (int)(sizeof(steps) / sizeof(steps[0])); ++si){
        cur -= steps[si];
        int new_size = pddlSegVecPopN(&sv, steps[si]);
        assert(new_size == cur);
        assert(pddlSegVecSize(&sv) == cur);
        assert(pddlSegVecGetConst(&sv, cur) == NULL);
        assert(*(const int *)pddlSegVecTop(&sv) == cur - 1);
        checkInts(&sv, cur);
    }
    // the memory is kept
    assert(pddlSegVecCapacity(&sv) == capacity);
    assert(pddlSegVecNumSegments(&sv) == num_segm);

    // the removed elements are initialized again
    int *el = pddlSegVecPush(&sv, NULL);
    assert(*el == -1);
    el = pddlSegVecGet(&sv, cur + 10);
    assert(*el == -1);
    for (int i = cur; i <= cur + 10; ++i)
        assert(*(const int *)pddlSegVecGetConst(&sv, i) == -1);
    checkInts(&sv, cur);

    // pop everything
    assert(pddlSegVecPopN(&sv, pddlSegVecSize(&sv)) == 0);
    assert(pddlSegVecSize(&sv) == 0);
    assert(pddlSegVecTop(&sv) == NULL);
    assert(pddlSegVecCapacity(&sv) == capacity);

    pddlSegVecFree(&sv);
}

/*
 * PopN removes the given number of elements from the end (zero does
 * nothing), keeps the memory, and the removed elements are initialized
 * again when they become part of the vector again.
 */
TEST_ONCE(segvec_pop_n)
{
    for (int bi = 0; bi < NUM_FIRST_SEGM_SIZES; ++bi){
        checkPopN(pddl_false, first_segm_sizes[bi]);
        checkPopN(pddl_true, first_segm_sizes[bi]);
    }
}

/*
 * pddlSegVecGet() past the end extends the vector, pddlSegVecGetConst()
 * returns NULL past the end and the same pointer as pddlSegVecGet() within.
 */
TEST_ONCE(segvec_get_grows)
{
    for (int exp = 0; exp <= 1; ++exp){
        pddl_segvec_t sv;
        pddlSegVecInit(&sv, sizeof(int), exp, 4);

        int *el = pddlSegVecGet(&sv, 10);
        *el = 10;
        assert(pddlSegVecSize(&sv) == 11);
        assert(pddlSegVecCapacity(&sv) >= 11);
        assert(pddlSegVecNumSegments(&sv)
                    == pddlSegVecSegmentId(&sv, 10) + 1);
        assert(pddlSegVecTop(&sv) == el);
        assert(pddlSegVecGetConst(&sv, 10) == el);
        assert(pddlSegVecGetConst(&sv, 11) == NULL);
        assert(pddlSegVecGetConst(&sv, -1) == NULL);

        // getting an element below the size changes nothing
        size_t capacity = pddlSegVecCapacity(&sv);
        el = pddlSegVecGet(&sv, 3);
        *el = 3;
        assert(pddlSegVecSize(&sv) == 11);
        assert(pddlSegVecCapacity(&sv) == capacity);
        for (int i = 0; i < 11; ++i)
            assert(pddlSegVecGet(&sv, i) == pddlSegVecGetConst(&sv, i));

        // the next push follows the largest index
        int idx;
        el = pddlSegVecPush(&sv, &idx);
        assert(idx == 11);

        // a jump over several segments at once
        el = pddlSegVecGet(&sv, 1000);
        *el = 1000;
        assert(pddlSegVecSize(&sv) == 1001);
        assert(pddlSegVecNumSegments(&sv)
                    == pddlSegVecSegmentId(&sv, 1000) + 1);
        assert(pddlSegVecCapacity(&sv)
                    == capacityOf(&sv, pddlSegVecNumSegments(&sv)));
        assert(*(const int *)pddlSegVecGetConst(&sv, 10) == 10);
        assert(*(const int *)pddlSegVecGetConst(&sv, 3) == 3);

        pddlSegVecFree(&sv);
    }
}

/*
 * Pointers to elements stay valid and unchanged while the vector grows.
 */
TEST_ONCE(segvec_pointer_stability)
{
    enum { NUM_PTRS = 100 };
    for (int exp = 0; exp <= 1; ++exp){
        pddl_segvec_t sv;
        pddlSegVecInit(&sv, sizeof(int), exp, 2);

        int *ptrs[NUM_PTRS];
        for (int i = 0; i < NUM_PTRS; ++i){
            ptrs[i] = pddlSegVecPush(&sv, NULL);
            *ptrs[i] = -i;
        }

        for (int i = NUM_PTRS; i < 100000; ++i){
            int *el = pddlSegVecPush(&sv, NULL);
            *el = -i;
        }
        assert(pddlSegVecSize(&sv) == 100000);

        for (int i = 0; i < NUM_PTRS; ++i){
            assert(pddlSegVecGet(&sv, i) == ptrs[i]);
            assert(*ptrs[i] == -i);
        }
        for (int i = 0; i < 100000; ++i)
            assert(*(const int *)pddlSegVecGetConst(&sv, i) == -i);

        pddlSegVecFree(&sv);
    }
}

/*
 * New elements are lazily initialized with a copy of the initial element,
 * which is stored in the vector's own memory.
 */
TEST_ONCE(segvec_init_el)
{
    for (int exp = 0; exp <= 1; ++exp){
        int init = -1;
        pddl_segvec_t sv;
        pddlSegVecInitDefault(&sv, sizeof(int), exp, 8, &init, NULL, NULL);
        assert(sv.init_el != NULL);
        assert(sv.init_el != (void *)&init);
        // modification of the caller's element has no effect
        init = -2;

        for (int i = 0; i < 6; ++i){
            int *el = pddlSegVecPush(&sv, NULL);
            assert(*el == -1);
            *el = i;
        }
        // elements 6, ..., 10 are initialized
        int *el = pddlSegVecGet(&sv, 10);
        assert(*el == -1);
        assert(pddlSegVecSize(&sv) == 11);
        checkInts(&sv, 6);
        for (int i = 6; i <= 10; ++i)
            assert(*(const int *)pddlSegVecGetConst(&sv, i) == -1);
        // the rest of the segment (8, ..., 15) is not initialized, write
        // junk to the memory of the element 11
        assert(pddlSegVecSegmentId(&sv, 10) == pddlSegVecSegmentId(&sv, 11));
        int *junk = el + 1;
        *junk = 555;
        el = pddlSegVecPush(&sv, NULL);
        assert(el == junk);
        assert(*el == -1);

        // popped elements are initialized again
        for (int i = 0; i < 12; ++i){
            el = pddlSegVecGet(&sv, i);
            *el = i;
        }
        int new_size = pddlSegVecPop(&sv);
        assert(new_size == 11);
        new_size = pddlSegVecPop(&sv);
        assert(new_size == 10);
        el = pddlSegVecPush(&sv, NULL);
        assert(*el == -1);
        el = pddlSegVecGet(&sv, 11);
        assert(*el == -1);
        checkInts(&sv, 10);

        // larger growth over multiple segments
        el = pddlSegVecGet(&sv, 500);
        for (int i = 12; i <= 500; ++i)
            assert(*(const int *)pddlSegVecGetConst(&sv, i) == -1);

        pddlSegVecFree(&sv);
    }
}

/*
 * The initialization callback is called exactly once for every index that
 * becomes part of the vector, in ascending order, with the user data.
 */
TEST_ONCE(segvec_init_fn)
{
    for (int exp = 0; exp <= 1; ++exp){
        init_rec_t rec = { 0, -1 };
        pddl_segvec_t sv;
        pddlSegVecInitDefault(&sv, sizeof(int), exp, 4, NULL, initCb, &rec);
        assert(sv.init_el == NULL);
        assert(sv.init_fn_userdata == &rec);

        for (int i = 0; i < 6; ++i){
            int idx;
            int *el = pddlSegVecPush(&sv, &idx);
            assert(*el == 1000 + i);
            assert(rec.calls == i + 1);
            assert(rec.last_idx == i);
        }

        // only 6, ..., 10 are initialized
        int *el = pddlSegVecGet(&sv, 10);
        assert(*el == 1010);
        assert(rec.calls == 11);
        assert(rec.last_idx == 10);
        for (int i = 0; i <= 10; ++i)
            assert(*(const int *)pddlSegVecGetConst(&sv, i) == 1000 + i);

        // no initialization on access within the vector
        for (int i = 0; i <= 10; ++i){
            el = pddlSegVecGet(&sv, i);
            *el = i;
        }
        assert(rec.calls == 11);

        // pop and push again calls the callback again for the same index
        int new_size = pddlSegVecPop(&sv);
        assert(new_size == 10);
        new_size = pddlSegVecPop(&sv);
        assert(new_size == 9);
        rec.last_idx = 8;
        el = pddlSegVecPush(&sv, NULL);
        assert(*el == 1009);
        assert(rec.calls == 12);
        el = pddlSegVecGet(&sv, 100);
        assert(*el == 1100);
        assert(rec.calls == 12 + 91);
        assert(rec.last_idx == 100);
        checkInts(&sv, 9);
        for (int i = 9; i <= 100; ++i)
            assert(*(const int *)pddlSegVecGetConst(&sv, i) == 1000 + i);

        pddlSegVecFree(&sv);
    }
}

/*
 * pddlSegVecInitDefault() without any initialization behaves the same as
 * pddlSegVecInit().
 */
TEST_ONCE(segvec_init_default_null)
{
    for (int exp = 0; exp <= 1; ++exp){
        pddl_segvec_t sv1, sv2;
        pddlSegVecInit(&sv1, sizeof(int), exp, 16);
        pddlSegVecInitDefault(&sv2, sizeof(int), exp, 16, NULL, NULL, NULL);
        assert(memcmp(&sv1, &sv2, sizeof(sv1)) == 0);
        assert(sv2.init_el == NULL);
        assert(sv2.init_fn == NULL);

        fillInts(&sv1, 100);
        fillInts(&sv2, 100);
        checkInts(&sv1, 100);
        checkInts(&sv2, 100);
        assert(pddlSegVecCapacity(&sv1) == pddlSegVecCapacity(&sv2));
        assert(pddlSegVecNumSegments(&sv1) == pddlSegVecNumSegments(&sv2));
        assert(pddlSegVecAllocBytes(&sv1) == pddlSegVecAllocBytes(&sv2));

        pddlSegVecFree(&sv1);
        pddlSegVecFree(&sv2);
    }
}

struct large_el {
    int a;
    double b;
    char c[9];
};
typedef struct large_el large_el_t;

/*
 * Elements larger than a word keep their content and do not overlap.
 */
TEST_ONCE(segvec_large_el)
{
    for (int exp = 0; exp <= 1; ++exp){
        large_el_t init = { -1, -0.5, "init" };
        pddl_segvec_t sv;
        pddlSegVecInitDefault(&sv, sizeof(large_el_t), exp, 4,
                              &init, NULL, NULL);
        assert(pddlSegVecElSize(&sv) == sizeof(large_el_t));

        for (int i = 0; i < 1000; ++i){
            int idx;
            large_el_t *el = pddlSegVecPush(&sv, &idx);
            assert(idx == i);
            assert(el->a == -1);
            assert(el->b == -0.5);
            assert(strcmp(el->c, "init") == 0);
            el->a = i;
            el->b = i / 2.;
            snprintf(el->c, sizeof(el->c), "e%d", i);
        }
        for (int i = 0; i < 1000; ++i){
            const large_el_t *el = pddlSegVecGetConst(&sv, i);
            char c[9];
            snprintf(c, sizeof(c), "e%d", i);
            assert(el->a == i);
            assert(el->b == i / 2.);
            assert(strcmp(el->c, c) == 0);
            if (i > 0 && pddlSegVecSegmentId(&sv, i)
                            == pddlSegVecSegmentId(&sv, i - 1)){
                const large_el_t *prev = pddlSegVecGetConst(&sv, i - 1);
                assert(prev + 1 == el);
            }
        }

        pddlSegVecFree(&sv);
    }
}

/*
 * The number of allocated bytes accounts for the segments, the array of
 * segments, and the copy of the initial element.
 */
TEST_ONCE(segvec_alloc_bytes)
{
    for (int exp = 0; exp <= 1; ++exp){
        pddl_segvec_t sv;
        pddlSegVecInit(&sv, sizeof(int), exp, 8);
        assert(pddlSegVecAllocBytes(&sv) == 0);

        for (int size = 1; size < 5000; size += 37){
            (void)pddlSegVecGet(&sv, size - 1);
            size_t cap = capacityOf(&sv, pddlSegVecNumSegments(&sv));
            assert(pddlSegVecCapacity(&sv) == cap);
            assert(pddlSegVecAllocBytes(&sv)
                        == cap * sizeof(int)
                            + sv.alloc_segm * sizeof(char *));
        }
        pddlSegVecFree(&sv);

        double init = 1.;
        pddlSegVecInitDefault(&sv, sizeof(double), exp, 8, &init, NULL, NULL);
        assert(pddlSegVecAllocBytes(&sv) == sizeof(double));
        (void)pddlSegVecGet(&sv, 100);
        size_t cap = pddlSegVecCapacity(&sv);
        assert(pddlSegVecAllocBytes(&sv)
                    == cap * sizeof(double)
                        + sv.alloc_segm * sizeof(char *)
                        + sizeof(double));
        pddlSegVecFree(&sv);
    }

    // the exponential layout needs far fewer segments
    pddl_segvec_t fixed, exp;
    pddlSegVecInit(&fixed, sizeof(int), pddl_false, 8);
    pddlSegVecInit(&exp, sizeof(int), pddl_true, 8);
    (void)pddlSegVecGet(&fixed, 8 * 1024 - 1);
    (void)pddlSegVecGet(&exp, 8 * 1024 - 1);
    assert(pddlSegVecNumSegments(&fixed) == 1024);
    assert(pddlSegVecNumSegments(&exp) == 11);
    assert(pddlSegVecCapacity(&fixed) == 8 * 1024);
    assert(pddlSegVecCapacity(&exp) == 8 * 1024);
    pddlSegVecFree(&fixed);
    pddlSegVecFree(&exp);
}

/* The size of the first segment must be a power of 2. */
TEST_PANIC_ONCE(segvec_panic_non_pow2)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), pddl_false, 3);
}

/* The size of the first segment must be positive. */
TEST_PANIC_ONCE(segvec_panic_zero_segm_size)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), pddl_true, 0);
}

/* Elements must have a positive size. */
TEST_PANIC_ONCE(segvec_panic_zero_el_size)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, 0, pddl_false, 8);
}

/* The initial element and the callback cannot be used together. */
TEST_PANIC_ONCE(segvec_panic_el_and_fn)
{
    init_rec_t rec = { 0, -1 };
    int init = 0;
    pddl_segvec_t sv;
    pddlSegVecInitDefault(&sv, sizeof(int), pddl_false, 8,
                          &init, initCb, &rec);
}

/* Popping from an empty vector panics. */
TEST_PANIC_ONCE(segvec_panic_pop_empty)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), pddl_false, 8);
    (void)pddlSegVecPush(&sv, NULL);
    int size = pddlSegVecPop(&sv);
    assert(size == 0);
    pddlSegVecPop(&sv);
}

/* Accessing a negative index panics. */
TEST_PANIC_ONCE(segvec_panic_negative_idx)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), pddl_true, 8);
    (void)pddlSegVecGet(&sv, 3);
    (void)pddlSegVecGet(&sv, -1);
}

/* Popping more elements than the vector holds panics. */
TEST_PANIC_ONCE(segvec_panic_pop_n_too_many)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), pddl_false, 8);
    (void)pddlSegVecPush(&sv, NULL);
    (void)pddlSegVecPush(&sv, NULL);
    pddlSegVecPopN(&sv, 3);
}

/* Popping a negative number of elements panics. */
TEST_PANIC_ONCE(segvec_panic_pop_n_negative)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), pddl_true, 8);
    (void)pddlSegVecPush(&sv, NULL);
    pddlSegVecPopN(&sv, -1);
}

/*
 * Truncate keeps exactly the given number of elements (truncating to the
 * current size does nothing), keeps the memory, and the removed elements
 * are initialized again when they become part of the vector again.
 */
TEST_ONCE(segvec_truncate)
{
    for (int exp = 0; exp <= 1; ++exp){
        int init = -1;
        pddl_segvec_t sv;
        pddlSegVecInitDefault(&sv, sizeof(int), exp, 4, &init, NULL, NULL);
        fillInts(&sv, 100);
        size_t capacity = pddlSegVecCapacity(&sv);

        pddlSegVecTruncate(&sv, 100);
        assert(pddlSegVecSize(&sv) == 100);
        checkInts(&sv, 100);

        pddlSegVecTruncate(&sv, 40);
        assert(pddlSegVecSize(&sv) == 40);
        assert(pddlSegVecCapacity(&sv) == capacity);
        assert(pddlSegVecGetConst(&sv, 40) == NULL);
        checkInts(&sv, 40);

        // the removed elements are initialized again
        int *el = pddlSegVecPush(&sv, NULL);
        assert(*el == -1);
        assert(pddlSegVecSize(&sv) == 41);

        pddlSegVecTruncate(&sv, 0);
        assert(pddlSegVecSize(&sv) == 0);
        assert(pddlSegVecTop(&sv) == NULL);
        assert(pddlSegVecCapacity(&sv) == capacity);
        pddlSegVecFree(&sv);
    }
}

/* Truncating to a size greater than the size of the vector panics. */
TEST_PANIC_ONCE(segvec_panic_truncate_too_big)
{
    pddl_segvec_t sv;
    pddlSegVecInit(&sv, sizeof(int), pddl_true, 8);
    (void)pddlSegVecPush(&sv, NULL);
    pddlSegVecTruncate(&sv, 2);
}
