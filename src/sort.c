/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/sort.h"
#include "pddl/rand.h"
#include "pddl/alloc.h"
#include "test.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/** Comparator for the reference libc qsort() */
static int cmpIntRef(const void *a, const void *b)
{
    int i1 = *(const int *)a;
    int i2 = *(const int *)b;
    if (i1 < i2)
        return -1;
    if (i1 > i2)
        return 1;
    return 0;
}

enum {
    GEN_RANDOM = 0,
    GEN_DUPLICATES,
    GEN_SORTED,
    GEN_REVERSED,
    GEN_ZIGZAG,
    GEN_SIZE,
};

/** Fills ARR of size N according to the generator GEN */
static void genArr(int *arr, int n, int gen, pddl_rand_t *rnd)
{
    for (int i = 0; i < n; ++i){
        switch (gen){
        case GEN_RANDOM:
            arr[i] = (int)pddlRand(rnd, -1000000, 1000000);
            break;
        case GEN_DUPLICATES:
            arr[i] = ((int)pddlRand(rnd, 0, 1000)) % 8;
            break;
        case GEN_SORTED:
            arr[i] = i;
            break;
        case GEN_REVERSED:
            arr[i] = n - i;
            break;
        case GEN_ZIGZAG:
            // Alternating increasing and decreasing runs of length 37
            arr[i] = ((i / 37) % 2 == 0) ? i % 37 : 37 - i % 37;
            break;
        }
    }
}

/*
 * pddlSortInt() sorts arrays of various sizes (covering the binary
 * insertion sort as well as the merging of runs) and shapes (random, many
 * duplicates, sorted, reversed, alternating runs) the same way as the
 * reference libc qsort().
 */
TEST_ONCE(sort_int)
{
    static const int sizes[] = { 0, 1, 2, 3, 15, 16, 17, 63, 64, 65, 100,
                                 1000, 100000 };
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 4321);

    for (int si = 0; si < num_sizes; ++si){
        int n = sizes[si];
        int *arr = PDDL_ALLOC_ARR(int, n + 1);
        int *ref = PDDL_ALLOC_ARR(int, n + 1);
        for (int gen = 0; gen < GEN_SIZE; ++gen){
            genArr(arr, n, gen, &rnd);
            memcpy(ref, arr, sizeof(int) * n);
            qsort(ref, n, sizeof(int), cmpIntRef);

            int ret = pddlSortInt(arr, n);
            assert(ret == 0);
            for (int i = 0; i < n; ++i)
                assert(arr[i] == ref[i]);
        }
        PDDL_FREE(arr);
        PDDL_FREE(ref);
    }
}
