/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the extendable array (pddl/extarr.h).
 */

#include "pddl/extarr.h"
#include "test.h"
#include <assert.h>

/* Initializes the element to BASE + 2 * idx. */
static void initFn(void *el, int idx, const void *userdata)
{
    const int *base = userdata;
    *(int *)el = *base + 2 * idx;
}

static void setInt(pddl_extarr_t *arr, size_t idx, int val)
{
    int *el = pddlExtArrGet(arr, idx);
    *el = val;
}

static int getInt(pddl_extarr_t *arr, size_t idx)
{
    const int *el = pddlExtArrGet(arr, idx);
    return *el;
}

TEST_ONCE(extarr_init_free)
{
    int init = -1;
    pddl_extarr_t arr;

    pddlExtArrInit(&arr, sizeof(int), NULL, &init);
    assert(pddlExtArrSize(&arr) == 0);
    assert(arr.arr.el_size == sizeof(int));
    assert(arr.init_fn == NULL);
    // the template is copied into the array's own storage
    assert(arr.init_data != NULL);
    assert(arr.init_data != (void *)&init);

    size_t size = 2 * arr.arr.els_per_segm;
    assert(getInt(&arr, size - 1) == -1);
    assert(pddlExtArrSize(&arr) == size);
    assert(arr.arr.num_segm == 2);
    for (size_t i = 0; i < size; ++i)
        setInt(&arr, i, (int)i);
    for (size_t i = 0; i < size; ++i)
        assert(getInt(&arr, i) == (int)i);

    pddlExtArrShrinkToSize(&arr, 0);
    assert(pddlExtArrSize(&arr) == 0);
    assert(arr.arr.num_segm == 0);

    pddlExtArrFree(&arr);
}

TEST_ONCE(extarr_init2_free)
{
    int base = 100;
    pddl_extarr_t arr;

    pddlExtArrInit2(&arr, sizeof(int), 1, 4096, initFn, &base);
    assert(arr.arr.els_per_segm >= 4096);
    assert(arr.init_fn == initFn);
    // with an init function the user data is only referenced
    assert(arr.init_data == (void *)&base);

    size_t size = 2 * arr.arr.els_per_segm;
    assert(getInt(&arr, size - 1) == 100 + 2 * (int)(size - 1));
    for (size_t i = 0; i < size; ++i)
        assert(getInt(&arr, i) == 100 + 2 * (int)i);

    pddlExtArrFree(&arr);
}

TEST_ONCE(extarr_init_copy)
{
    int base = 100;
    pddl_extarr_t src;
    pddlExtArrInit(&src, sizeof(int), initFn, &base);

    size_t size = 2 * src.arr.els_per_segm;
    for (size_t i = 0; i < size; ++i)
        setInt(&src, i, (int)(3 * i));

    pddl_extarr_t dst;
    pddlExtArrInitCopy(&dst, &src);
    assert(pddlExtArrSize(&dst) == pddlExtArrSize(&src));
    assert(dst.arr.el_size == src.arr.el_size);
    assert(dst.arr.segm_size == src.arr.segm_size);
    assert(dst.arr.num_segm == src.arr.num_segm);
    assert(dst.init_fn == src.init_fn);
    for (size_t i = 0; i < size; ++i)
        assert(getInt(&dst, i) == (int)(3 * i));

    // the copy is independent of the source
    for (size_t i = 0; i < size; ++i)
        setInt(&dst, i, -7);
    for (size_t i = 0; i < size; ++i){
        assert(getInt(&dst, i) == -7);
        assert(getInt(&src, i) == (int)(3 * i));
    }

    pddlExtArrFree(&dst);
    pddlExtArrFree(&src);
}

TEST_ONCE(extarr_init_copy_init_data)
{
    int init = -1;
    pddl_extarr_t src;
    pddlExtArrInit(&src, sizeof(int), NULL, &init);

    size_t size = src.arr.els_per_segm + 1;
    assert(getInt(&src, size - 1) == -1);
    for (size_t i = 0; i < size; ++i)
        setInt(&src, i, (int)i);

    pddl_extarr_t dst;
    pddlExtArrInitCopy(&dst, &src);
    // the template must be deep-copied, not shared
    assert(dst.init_data != NULL);
    assert(dst.init_data != src.init_data);
    assert(*(int *)dst.init_data == -1);
    assert(pddlExtArrSize(&dst) == size);
    for (size_t i = 0; i < size; ++i)
        assert(getInt(&dst, i) == (int)i);

    // pddlExtArrClone() is the heap-allocating variant of the same thing
    pddl_extarr_t *clone = pddlExtArrClone(&src);
    assert(pddlExtArrSize(clone) == size);
    assert(clone->init_data != src.init_data);
    for (size_t i = 0; i < size; ++i)
        assert(getInt(clone, i) == (int)i);
    pddlExtArrDel(clone);

    pddlExtArrFree(&dst);
    pddlExtArrFree(&src);
}

TEST_ONCE(extarr_segment_first_idx)
{
    pddl_extarr_t *arr = pddlExtArrNew(sizeof(int), NULL, NULL);
    size_t els_per_segm = arr->arr.els_per_segm;
    assert(els_per_segm > 1);

    // valid even though no element has been stored yet
    assert(pddlExtArrSize(arr) == 0);
    for (size_t s = 0; s < 3; ++s){
        size_t first = s * els_per_segm;
        assert(pddlExtArrSegmentFirstIdx(arr, first) == first);
        assert(pddlExtArrSegmentFirstIdx(arr, first + 1) == first);
        assert(pddlExtArrSegmentFirstIdx(arr, first + els_per_segm - 1)
                    == first);
    }

    // and it is just a forward to the underlying segmented array
    for (size_t i = 0; i < 3 * els_per_segm; ++i){
        assert(pddlExtArrSegmentFirstIdx(arr, i)
                    == pddlSegmArrSegmentFirstIdx(&arr->arr, i));
    }

    pddlExtArrDel(arr);
}

TEST_ONCE(extarr_shrink_to_size_init_data)
{
    int init = -1;
    pddl_extarr_t *arr = pddlExtArrNew(sizeof(int), NULL, &init);
    size_t els_per_segm = arr->arr.els_per_segm;
    size_t size = 3 * els_per_segm;

    // all elements are initialized from the template
    assert(getInt(arr, size - 1) == -1);
    assert(pddlExtArrSize(arr) == size);
    assert(arr->arr.num_segm == 3);
    for (size_t i = 0; i < size; ++i)
        assert(getInt(arr, i) == -1);

    // overwrite them with distinct values
    for (size_t i = 0; i < size; ++i)
        setInt(arr, i, (int)i);

    size_t shrink_to = els_per_segm + els_per_segm / 2;
    pddlExtArrShrinkToSize(arr, shrink_to);
    assert(pddlExtArrSize(arr) == shrink_to);
    assert(arr->arr.num_segm == 2);

    // the retained elements keep their values
    for (size_t i = 0; i < shrink_to; ++i)
        assert(getInt(arr, i) == (int)i);

    // the elements from shrink_to on are initialized from the template again
    assert(getInt(arr, size - 1) == -1);
    assert(pddlExtArrSize(arr) == size);
    assert(arr->arr.num_segm == 3);
    for (size_t i = 0; i < shrink_to; ++i)
        assert(getInt(arr, i) == (int)i);
    for (size_t i = shrink_to; i < size; ++i)
        assert(getInt(arr, i) == -1);

    pddlExtArrDel(arr);
}

TEST_ONCE(extarr_shrink_to_size_init_fn)
{
    int base = 100;
    pddl_extarr_t *arr = pddlExtArrNew(sizeof(int), initFn, &base);
    size_t els_per_segm = arr->arr.els_per_segm;
    size_t size = 2 * els_per_segm;

    assert(getInt(arr, size - 1) == 100 + 2 * (int)(size - 1));
    assert(pddlExtArrSize(arr) == size);
    for (size_t i = 0; i < size; ++i)
        assert(getInt(arr, i) == 100 + 2 * (int)i);

    // overwrite everything with junk
    for (size_t i = 0; i < size; ++i)
        setInt(arr, i, -7);

    size_t shrink_to = els_per_segm / 2;
    pddlExtArrShrinkToSize(arr, shrink_to);
    assert(pddlExtArrSize(arr) == shrink_to);
    assert(arr->arr.num_segm == 1);

    // the junk below shrink_to survives ...
    for (size_t i = 0; i < shrink_to; ++i)
        assert(getInt(arr, i) == -7);
    // ... and everything above is initialized by the callback again
    assert(getInt(arr, size - 1) == 100 + 2 * (int)(size - 1));
    for (size_t i = 0; i < shrink_to; ++i)
        assert(getInt(arr, i) == -7);
    for (size_t i = shrink_to; i < size; ++i)
        assert(getInt(arr, i) == 100 + 2 * (int)i);

    pddlExtArrDel(arr);
}

TEST_ONCE(extarr_shrink_to_size_noop)
{
    pddl_extarr_t *arr = pddlExtArrNew(sizeof(int), NULL, NULL);
    size_t els_per_segm = arr->arr.els_per_segm;
    size_t size = 3 * els_per_segm;

    for (size_t i = 0; i < size; ++i)
        setInt(arr, i, (int)i);
    assert(pddlExtArrSize(arr) == size);
    assert(arr->arr.num_segm == 3);

    // shrinking to the current size or above changes nothing
    pddlExtArrShrinkToSize(arr, size);
    assert(pddlExtArrSize(arr) == size);
    assert(arr->arr.num_segm == 3);

    pddlExtArrShrinkToSize(arr, size + els_per_segm);
    assert(pddlExtArrSize(arr) == size);
    assert(arr->arr.num_segm == 3);
    for (size_t i = 0; i < size; ++i)
        assert(getInt(arr, i) == (int)i);

    // shrinking to a size within the last segment keeps all the segments
    pddlExtArrShrinkToSize(arr, size - 1);
    assert(pddlExtArrSize(arr) == size - 1);
    assert(arr->arr.num_segm == 3);

    // shrink everything away and grow the array again
    pddlExtArrShrinkToSize(arr, 0);
    assert(pddlExtArrSize(arr) == 0);
    assert(arr->arr.num_segm == 0);

    for (size_t i = 0; i < size; ++i)
        setInt(arr, i, (int)(2 * i));
    assert(pddlExtArrSize(arr) == size);
    assert(arr->arr.num_segm == 3);
    for (size_t i = 0; i < size; ++i)
        assert(getInt(arr, i) == (int)(2 * i));

    pddlExtArrDel(arr);
}
