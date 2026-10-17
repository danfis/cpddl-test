/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "test.h"
#include <assert.h>
#include <string.h>
#include "pddl/iarr.h"

/*
 * pddlIArrAppendBuf() appends the given elements in order after the
 * existing ones: appending nothing (even from NULL) keeps an empty array
 * unallocated, and appending many elements grows the array as needed.
 */
TEST_ONCE(iarr_append_buf)
{
    PDDL_IARR(arr);
    pddlIArrAppendBuf(&arr, NULL, 0);
    assert(pddlIArrSize(&arr) == 0);
    assert(pddlIArrGetArr(&arr) == NULL);

    static const int a[] = { 5, 3, 8 };
    pddlIArrAppendBuf(&arr, a, 3);
    assert(pddlIArrSize(&arr) == 3);
    assert(memcmp(pddlIArrGetArr(&arr), a, sizeof(a)) == 0);

    static const int b[] = { 1, 1, 0, -2, 7 };
    pddlIArrAppendBuf(&arr, b, 5);
    assert(pddlIArrSize(&arr) == 8);
    assert(memcmp(pddlIArrGetArr(&arr), a, sizeof(a)) == 0);
    assert(memcmp(pddlIArrGetArr(&arr) + 3, b, sizeof(b)) == 0);

    pddlIArrAppendBuf(&arr, b, 0);
    assert(pddlIArrSize(&arr) == 8);

    int big[1000];
    for (int i = 0; i < 1000; ++i)
        big[i] = 3 * i;
    pddlIArrAppendBuf(&arr, big, 1000);
    pddlIArrAppendBuf(&arr, big, 1000);
    assert(pddlIArrSize(&arr) == 2008);
    assert(memcmp(pddlIArrGetArr(&arr), a, sizeof(a)) == 0);
    assert(memcmp(pddlIArrGetArr(&arr) + 3, b, sizeof(b)) == 0);
    assert(memcmp(pddlIArrGetArr(&arr) + 8, big, sizeof(big)) == 0);
    assert(memcmp(pddlIArrGetArr(&arr) + 1008, big, sizeof(big)) == 0);
    pddlIArrFree(&arr);
}
