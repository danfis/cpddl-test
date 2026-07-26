/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests for the numeric-constant formula node PDDL_FM_NUM_EXP_NUM backed
 * by pddl_num_val_t.
 */

#include "pddl/fm.h"
#include "test.h"
#include <assert.h>
#include <string.h>

TEST_ONCE(fm_num_exp)
{
    // Constructors and predicates
    pddl_fm_num_exp_t *i42 = pddlFmNewNumExpNumInt(42);
    assert(i42->fm.type == PDDL_FM_NUM_EXP_NUM);
    assert(pddlFmIsNumExp(&i42->fm));
    assert(pddlFmIsNumExpNum(&i42->fm));
    assert(pddlFmIsNumExpNumInt(&i42->fm));
    assert(!pddlFmIsNumExpNumFlt(&i42->fm));
    assert(i42->e.num.v.i == 42);

    pddl_fm_num_exp_t *fhalf = pddlFmNewNumExpNumFlt(0.5);
    assert(pddlFmIsNumExpNum(&fhalf->fm));
    assert(pddlFmIsNumExpNumFlt(&fhalf->fm));
    assert(!pddlFmIsNumExpNumInt(&fhalf->fm));
    assert(fhalf->e.num.v.f == 0.5);

    pddl_num_val_t val;
    pddlNumValSetInt(&val, -7);
    pddl_fm_num_exp_t *im7 = pddlFmNewNumExpNum(&val);
    assert(pddlFmIsNumExpNumInt(&im7->fm));
    assert(im7->e.num.v.i == -7);

    // Print-format lock-in: numeric constants are printed with
    // pddlNumValFmt(), i.e., "%lld" for ints and "%g" for floats
    char buf[64];
    assert(strcmp(pddlFmFmt(&i42->fm, NULL, NULL, buf, sizeof(buf)),
                  "42") == 0);
    assert(strcmp(pddlFmFmt(&im7->fm, NULL, NULL, buf, sizeof(buf)),
                  "-7") == 0);
    assert(strcmp(pddlFmFmt(&fhalf->fm, NULL, NULL, buf, sizeof(buf)),
                  "0.5") == 0);
    pddl_fm_num_exp_t *f2 = pddlFmNewNumExpNumFlt(2.0);
    assert(strcmp(pddlFmFmt(&f2->fm, NULL, NULL, buf, sizeof(buf)),
                  "2") == 0);

    // Zero/one and int-in-range predicates
    pddl_fm_num_exp_t *i0 = pddlFmNewNumExpNumInt(0);
    pddl_fm_num_exp_t *i1 = pddlFmNewNumExpNumInt(1);
    pddl_fm_num_exp_t *f0 = pddlFmNewNumExpNumFlt(0.0);
    pddl_fm_num_exp_t *f1 = pddlFmNewNumExpNumFlt(1.0);
    assert(pddlFmIsNumExpNumZero(&i0->fm));
    assert(pddlFmIsNumExpNumZero(&f0->fm));
    assert(!pddlFmIsNumExpNumZero(&i1->fm));
    assert(!pddlFmIsNumExpNumZero(&fhalf->fm));
    assert(pddlFmIsNumExpNumOne(&i1->fm));
    assert(pddlFmIsNumExpNumOne(&f1->fm));
    assert(!pddlFmIsNumExpNumOne(&i0->fm));
    assert(!pddlFmIsNumExpNumOne(&fhalf->fm));
    assert(pddlFmIsNumExpNumIntInRange(&i42->fm, 0, 42));
    assert(pddlFmIsNumExpNumIntInRange(&i42->fm, 42, 100));
    assert(!pddlFmIsNumExpNumIntInRange(&i42->fm, 0, 41));
    assert(!pddlFmIsNumExpNumIntInRange(&im7->fm, 0, 100));
    assert(!pddlFmIsNumExpNumIntInRange(&f1->fm, 0, 100));
    pddlFmDel(&i0->fm);
    pddlFmDel(&i1->fm);
    pddlFmDel(&f0->fm);
    pddlFmDel(&f1->fm);

    // Recast of an integral float constant to an integer constant
    assert(pddlFmNumExpCanRecastFltToInt(f2));
    pddlFmNumExpRecastFltToInt(f2);
    assert(pddlFmIsNumExpNumInt(&f2->fm));
    assert(f2->e.num.v.i == 2);
    assert(strcmp(pddlFmFmt(&f2->fm, NULL, NULL, buf, sizeof(buf)),
                  "2") == 0);
    // Non-integral and out-of-range floats cannot be recast
    assert(!pddlFmNumExpCanRecastFltToInt(fhalf));
    pddl_fm_num_exp_t *fbig = pddlFmNewNumExpNumFlt(1e300);
    assert(!pddlFmNumExpCanRecastFltToInt(fbig));
    // Neither can an int constant
    assert(!pddlFmNumExpCanRecastFltToInt(i42));

    // Equality: identity of type and value
    pddl_fm_num_exp_t *i2 = pddlFmNewNumExpNumInt(2);
    pddl_fm_num_exp_t *f2b = pddlFmNewNumExpNumFlt(2.0);
    assert(pddlFmEq(&i2->fm, &f2->fm));
    assert(!pddlFmEq(&i2->fm, &f2b->fm));
    assert(!pddlFmEq(&i2->fm, &i42->fm));

    // Clone
    pddl_fm_t *clone = pddlFmClone(&fhalf->fm);
    assert(pddlFmIsNumExpNumFlt(clone));
    assert(pddlFmEq(clone, &fhalf->fm));
    pddlFmDel(clone);

    pddlFmDel(&i42->fm);
    pddlFmDel(&fhalf->fm);
    pddlFmDel(&im7->fm);
    pddlFmDel(&f2->fm);
    pddlFmDel(&fbig->fm);
    pddlFmDel(&i2->fm);
    pddlFmDel(&f2b->fm);
}
