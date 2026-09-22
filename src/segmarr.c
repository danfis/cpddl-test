/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the generic segmented array (pddl/segmarr.h).
 */

#include "pddl/segmarr.h"
#include "test.h"
#include <assert.h>

/* Number of segments needed for SIZE elements. */
static size_t numSegmFor(const pddl_segmarr_t *arr, size_t size)
{
    return (size + arr->els_per_segm - 1) / arr->els_per_segm;
}

/* Stores idx into the idx'th element for idx = 0, ..., size-1. */
static void fillInts(pddl_segmarr_t *arr, size_t size)
{
    for (size_t i = 0; i < size; ++i){
        int *el = pddlSegmArrGet(arr, i);
        *el = (int)i;
    }
}

/* Checks that the elements 0, ..., size-1 hold the values set by fillInts(). */
static void checkInts(pddl_segmarr_t *arr, size_t size)
{
    for (size_t i = 0; i < size; ++i){
        const int *el = pddlSegmArrConstGet(arr, i);
        assert(el != NULL);
        assert(*el == (int)i);
    }
}

/* Checks that every index of every segment maps to the first index of that
 * segment, for the first num_segm segments. */
static void checkSegmentFirstIdx(const pddl_segmarr_t *arr, size_t num_segm)
{
    for (size_t s = 0; s < num_segm; ++s){
        size_t first = s * arr->els_per_segm;
        for (size_t i = first; i < first + arr->els_per_segm; ++i)
            assert(pddlSegmArrSegmentFirstIdx(arr, i) == first);
    }
}

TEST_ONCE(segmarr_init_free)
{
    pddl_segmarr_t arr;

    int ret = pddlSegmArrInit(&arr, sizeof(int), 10 * sizeof(int));
    assert(ret == 0);
    assert(arr.el_size == sizeof(int));
    assert(arr.segm_size == 10 * sizeof(int));
    assert(arr.els_per_segm == 10);
    assert(arr.segm == NULL);
    assert(arr.num_segm == 0);
    assert(arr.alloc_segm == 0);

    // the in-place initialized array behaves exactly as the allocated one
    fillInts(&arr, 25);
    assert(arr.num_segm == 3);
    checkInts(&arr, 25);
    checkSegmentFirstIdx(&arr, 3);

    pddlSegmArrShrinkToSize(&arr, 10);
    assert(arr.num_segm == 1);
    checkInts(&arr, 10);

    pddlSegmArrFree(&arr);
}

TEST_ONCE(segmarr_init_invalid)
{
    pddl_segmarr_t arr;

    // an element must fit into a single segment
    int ret = pddlSegmArrInit(&arr, 16, 8);
    assert(ret == -1);
    pddl_segmarr_t *heap_arr = pddlSegmArrNew(16, 8);
    assert(heap_arr == NULL);

    // the border case of exactly one element per segment is fine
    ret = pddlSegmArrInit(&arr, 8, 8);
    assert(ret == 0);
    assert(arr.els_per_segm == 1);
    pddlSegmArrFree(&arr);
}

TEST_ONCE(segmarr_segment_first_idx)
{
    // el_size divides segm_size exactly
    pddl_segmarr_t *arr = pddlSegmArrNew(sizeof(int), 10 * sizeof(int));
    assert(arr != NULL);
    assert(arr->els_per_segm == 10);

    // valid even though nothing is allocated yet
    assert(arr->num_segm == 0);
    checkSegmentFirstIdx(arr, 100);

    fillInts(arr, 25);
    assert(arr->num_segm == 3);
    checkSegmentFirstIdx(arr, 3);

    // and also past the allocated segments
    assert(pddlSegmArrSegmentFirstIdx(arr, 1000) == 1000);
    assert(pddlSegmArrSegmentFirstIdx(arr, 1009) == 1000);
    pddlSegmArrDel(arr);

    // el_size does not divide segm_size
    arr = pddlSegmArrNew(3, 10);
    assert(arr != NULL);
    assert(arr->els_per_segm == 3);
    checkSegmentFirstIdx(arr, 100);
    assert(pddlSegmArrSegmentFirstIdx(arr, 0) == 0);
    assert(pddlSegmArrSegmentFirstIdx(arr, 2) == 0);
    assert(pddlSegmArrSegmentFirstIdx(arr, 3) == 3);
    assert(pddlSegmArrSegmentFirstIdx(arr, 5) == 3);
    assert(pddlSegmArrSegmentFirstIdx(arr, 6) == 6);
    pddlSegmArrDel(arr);

    // a single element per segment
    arr = pddlSegmArrNew(sizeof(int), sizeof(int));
    assert(arr != NULL);
    assert(arr->els_per_segm == 1);
    for (size_t i = 0; i < 20; ++i)
        assert(pddlSegmArrSegmentFirstIdx(arr, i) == i);
    pddlSegmArrDel(arr);
}

TEST_ONCE(segmarr_shrink_to_size)
{
    pddl_segmarr_t *arr = pddlSegmArrNew(sizeof(int), 10 * sizeof(int));
    assert(arr != NULL);
    assert(arr->els_per_segm == 10);

    fillInts(arr, 55);
    assert(arr->num_segm == 6);
    size_t alloc_segm = arr->alloc_segm;

    // shrinking to a size that does not fit into the current segments must
    // not change anything
    pddlSegmArrShrinkToSize(arr, 60);
    assert(arr->num_segm == 6);
    pddlSegmArrShrinkToSize(arr, 1000);
    assert(arr->num_segm == 6);
    checkInts(arr, 55);

    // shrink to a size in the middle of a segment
    pddlSegmArrShrinkToSize(arr, 35);
    assert(arr->num_segm == 4);
    assert(arr->num_segm == numSegmFor(arr, 35));
    // the pointer array itself is kept as it is
    assert(arr->alloc_segm == alloc_segm);
    for (size_t i = arr->num_segm; i < arr->alloc_segm; ++i)
        assert(arr->segm[i] == NULL);
    // the retained elements are untouched -- including the ones past the
    // requested size that share a segment with the last retained element
    checkInts(arr, 40);
    assert(pddlSegmArrConstGet(arr, 40) == NULL);

    // shrink to exactly a segment boundary
    pddlSegmArrShrinkToSize(arr, 20);
    assert(arr->num_segm == 2);
    assert(arr->num_segm == numSegmFor(arr, 20));
    checkInts(arr, 20);
    assert(pddlSegmArrConstGet(arr, 20) == NULL);

    // shrinking to the very same size is a no-op
    pddlSegmArrShrinkToSize(arr, 20);
    assert(arr->num_segm == 2);
    checkInts(arr, 20);

    // the array is still extendable and the re-allocated segments are usable
    fillInts(arr, 55);
    assert(arr->num_segm == 6);
    checkInts(arr, 55);

    // shrink everything away
    pddlSegmArrShrinkToSize(arr, 0);
    assert(arr->num_segm == 0);
    for (size_t i = 0; i < arr->alloc_segm; ++i)
        assert(arr->segm[i] == NULL);
    assert(pddlSegmArrConstGet(arr, 0) == NULL);

    // ... and grow it from scratch again
    fillInts(arr, 12);
    assert(arr->num_segm == 2);
    checkInts(arr, 12);

    pddlSegmArrDel(arr);
}

TEST_ONCE(segmarr_shrink_to_size_empty)
{
    pddl_segmarr_t *arr = pddlSegmArrNew(sizeof(int), 10 * sizeof(int));
    assert(arr != NULL);

    // shrinking an array that has no segments allocated yet
    assert(arr->num_segm == 0);
    pddlSegmArrShrinkToSize(arr, 0);
    assert(arr->num_segm == 0);
    assert(arr->segm == NULL);
    pddlSegmArrShrinkToSize(arr, 100);
    assert(arr->num_segm == 0);
    assert(arr->segm == NULL);

    fillInts(arr, 5);
    assert(arr->num_segm == 1);
    checkInts(arr, 5);

    pddlSegmArrDel(arr);
}
