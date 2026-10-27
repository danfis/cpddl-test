/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the priority queue of int values with pddl_num_t keys
 * (pddl/ipq.h) in both of its modes (array and splay tree).
 */

#include "pddl/ipq.h"
#include "pddl/rand.h"
#include "test.h"
#include <assert.h>

/** Returns the number parsed from S (see pddlNumParse()) */
static pddl_num_t num(const char *s)
{
    pddl_num_t v;
    int ret = pddlNumParse(&v, s);
    assert(ret == 0);
    return v;
}

/** Returns a huge number with the sign of SIGN */
static pddl_num_t hugeNum(int sign)
{
    pddl_num_t v;
    if (sign > 0){
        pddlNumSetInt(&v, INT_MAX);
        pddlNumAddIntSat(&v, 1);
    }else{
        pddlNumSetInt(&v, INT_MIN);
        pddlNumAddIntSat(&v, -1);
    }
    assert(pddlNumIsHuge(&v));
    return v;
}

/** Pushes VALUE with the key parsed from KEY */
static void push(pddl_ipq_t *q, const char *key, int value)
{
    pddl_num_t k = num(key);
    pddlIPQPush(q, &k, value);
}

/** Pops a value and asserts that it is VALUE with the key KEY (the same
 *  bit pattern) */
static void popExpectNum(pddl_ipq_t *q, int value, const pddl_num_t *key)
{
    int v;
    pddl_num_t k;
    int ret = pddlIPQPop(q, &v, &k);
    assert(ret == 0);
    assert(v == value);
    assert(pddlNumExactEq(&k, key));
}

/** Same as popExpectNum() with the key parsed from KEY */
static void popExpect(pddl_ipq_t *q, int value, const char *key)
{
    pddl_num_t k = num(key);
    popExpectNum(q, value, &k);
}

/** Asserts that the lowest key is KEY */
static void topExpect(pddl_ipq_t *q, const char *key)
{
    pddl_num_t k;
    int ret = pddlIPQTopKey(q, &k);
    assert(ret == 0);
    pddl_num_t exp = num(key);
    assert(pddlNumExactEq(&k, &exp));
}

/** Pops all values with the lowest key and asserts that they are
 *  VALUE[0..SIZE) (in this order) with the key KEY */
static void popAllExpect(pddl_ipq_t *q,
                         const char *key,
                         int size,
                         const int *value)
{
    PDDL_IVEC(out);
    pddl_num_t k;
    int ret = pddlIPQPopAll(q, &out, &k);
    assert(ret == 0);
    pddl_num_t exp = num(key);
    assert(pddlNumExactEq(&k, &exp));
    assert(pddlIVecSize(&out) == size);
    for (int i = 0; i < size; ++i)
        assert(pddlIVecGet(&out, i) == value[i]);
    pddlIVecFree(&out);
}

/** Asserts that the queue is empty */
static void assertEmpty(pddl_ipq_t *q)
{
    assert(pddlIPQIsEmpty(q));
    assert(pddlIPQSize(q) == 0);
    int v;
    pddl_num_t k;
    int ret = pddlIPQPop(q, &v, &k);
    assert(ret == -1);
    ret = pddlIPQTopKey(q, &k);
    assert(ret == -1);
    PDDL_IVEC(out);
    pddlIVecPush(&out, 1);
    ret = pddlIPQPopAll(q, &out, &k);
    assert(ret == -1);
    assert(pddlIVecSize(&out) == 0);
    pddlIVecFree(&out);
}

/*
 * Integer keys in [0, PDDL_IPQ_ARRAY_SIZE) are popped in the order of the
 * keys and in LIFO order within equal keys, also when lower keys are pushed
 * after a pop; the queue stays in the array mode.
 */
TEST_ONCE(ipq_array)
{
    pddl_ipq_t q;
    pddlIPQInit(&q);
    assertEmpty(&q);
    push(&q, "5", 1);
    push(&q, "3", 2);
    push(&q, "5", 3);
    push(&q, "0", 4);
    push(&q, "1023", 5);
    push(&q, "3", 6);
    assert(pddlIPQSize(&q) == 6);
    topExpect(&q, "0");
    popExpect(&q, 4, "0");

    push(&q, "1", 7);
    push(&q, "0", 8);
    assert(pddlIPQSize(&q) == 7);
    topExpect(&q, "0");
    popExpect(&q, 8, "0");
    popExpect(&q, 7, "1");
    popExpect(&q, 6, "3");
    popExpect(&q, 2, "3");
    popExpect(&q, 3, "5");
    popExpect(&q, 1, "5");
    popExpect(&q, 5, "1023");
    assertEmpty(&q);

    push(&q, "7", 9);
    push(&q, "7", 9);
    popExpect(&q, 9, "7");
    popExpect(&q, 9, "7");
    assertEmpty(&q);
    assert(q.mode == PDDL_IPQ_MODE_ARRAY);
    pddlIPQFree(&q);
}

/*
 * The first key outside of the array (an integer out of the range, a key
 * with delta != 0, a float, or an infinity) promotes the queue to the splay
 * tree mode: the values pushed before and after the promotion are popped in
 * the correct order. The promotion works also with nothing ever pushed and
 * with an emptied queue.
 */
TEST_ONCE(ipq_promote)
{
    static const struct {
        const char *key;
        int order[7];
    } cases[] = {
        { "1024", { 5, 7, 3, 1, 6, 2, 4 } },
        { "-1", { 4, 5, 7, 3, 1, 6, 2 } },
        { "5+1e", { 5, 7, 3, 1, 4, 6, 2 } },
        { "2.5", { 5, 7, 4, 3, 1, 6, 2 } },
        { "inf", { 5, 7, 3, 1, 6, 2, 4 } },
    };
    for (int ci = 0; ci < (int)(sizeof(cases) / sizeof(cases[0])); ++ci){
        pddl_ipq_t q;
        pddlIPQInit(&q);
        push(&q, "3", 1);
        push(&q, "700", 2);
        push(&q, "3", 3);
        assert(q.mode == PDDL_IPQ_MODE_ARRAY);
        push(&q, cases[ci].key, 4);
        assert(q.mode == PDDL_IPQ_MODE_SPLAYTREE);
        push(&q, "0", 5);
        push(&q, "700", 6);
        push(&q, "2", 7);
        assert(pddlIPQSize(&q) == 7);

        pddl_num_t last = pddl_num_neg_inf;
        for (int i = 0; i < 7; ++i){
            int v;
            pddl_num_t k;
            int ret = pddlIPQPop(&q, &v, &k);
            assert(ret == 0);
            assert(v == cases[ci].order[i]);
            assert(pddlNumLE(&last, &k));
            last = k;
        }
        assertEmpty(&q);
        assert(q.mode == PDDL_IPQ_MODE_SPLAYTREE);
        pddlIPQFree(&q);
    }

    pddl_ipq_t q;
    pddlIPQInit(&q);
    push(&q, "-5", 1);
    assert(q.mode == PDDL_IPQ_MODE_SPLAYTREE);
    popExpect(&q, 1, "-5");
    assertEmpty(&q);
    pddlIPQFree(&q);

    pddlIPQInit(&q);
    push(&q, "3", 1);
    popExpect(&q, 1, "3");
    push(&q, "2000", 2);
    assert(q.mode == PDDL_IPQ_MODE_SPLAYTREE);
    push(&q, "3", 3);
    popExpect(&q, 3, "3");
    popExpect(&q, 2, "2000");
    assertEmpty(&q);
    pddlIPQFree(&q);
}

/*
 * Special keys are ordered as -inf < -huge < finite values < +huge < +inf,
 * keys with delta != 0 and floats are ordered by pddlNumCmp(), and the int
 * 2 and the float 2.0 share a bucket whose key is the int 2 that created
 * it.
 */
TEST_ONCE(ipq_special_keys)
{
    pddl_ipq_t q;
    pddlIPQInit(&q);
    pddl_num_t huge_pos = hugeNum(1);
    pddl_num_t huge_neg = hugeNum(-1);
    pddl_num_t flt2;
    pddlNumSetFlt(&flt2, 2.f);
    pddl_num_t int2 = num("2");

    push(&q, "-inf", 1);
    pddlIPQPush(&q, &huge_pos, 2);
    push(&q, "inf", 3);
    push(&q, "-7", 4);
    push(&q, "2.5", 5);
    push(&q, "2.5+1e", 6);
    push(&q, "2.5-1e", 7);
    pddlIPQPush(&q, &int2, 8);
    pddlIPQPush(&q, &flt2, 9);
    push(&q, "2+1e", 10);
    pddlIPQPush(&q, &huge_neg, 11);
    push(&q, "0", 12);
    assert(pddlIPQSize(&q) == 12);

    popExpect(&q, 1, "-inf");
    popExpectNum(&q, 11, &huge_neg);
    popExpect(&q, 4, "-7");
    popExpect(&q, 12, "0");
    popExpectNum(&q, 9, &int2);
    popExpectNum(&q, 8, &int2);
    popExpect(&q, 10, "2+1e");
    popExpect(&q, 7, "2.5-1e");
    popExpect(&q, 5, "2.5");
    popExpect(&q, 6, "2.5+1e");
    popExpectNum(&q, 2, &huge_pos);
    popExpect(&q, 3, "inf");
    assertEmpty(&q);
    pddlIPQFree(&q);
}

/** Pushes the values of ipq_pop_all and checks pddlIPQPopAll() */
static void popAllCheck(pddl_ipq_t *q)
{
    push(q, "4", 1);
    push(q, "4", 2);
    push(q, "9", 3);
    push(q, "4", 4);
    int v1[3] = { 1, 2, 4 };
    popAllExpect(q, "4", 3, v1);
    assert(pddlIPQSize(q) == 1);

    push(q, "4", 5);
    push(q, "4", 6);
    int v2[2] = { 5, 6 };
    popAllExpect(q, "4", 2, v2);
    push(q, "9", 7);
    popExpect(q, 7, "9");
    int v3[1] = { 3 };
    popAllExpect(q, "9", 1, v3);
    assertEmpty(q);
}

/*
 * pddlIPQPopAll() returns all values with the lowest key in the order of
 * their pushes, values pushed with the same key afterwards are returned by
 * the next pop, and an empty queue empties OUT; in both modes.
 */
TEST_ONCE(ipq_pop_all)
{
    pddl_ipq_t q;
    pddlIPQInit(&q);
    popAllCheck(&q);
    assert(q.mode == PDDL_IPQ_MODE_ARRAY);
    pddlIPQFree(&q);

    pddlIPQInit(&q);
    push(&q, "-1", 0);
    popExpect(&q, 0, "-1");
    popAllCheck(&q);
    assert(q.mode == PDDL_IPQ_MODE_SPLAYTREE);
    pddlIPQFree(&q);
}

/*
 * pddlIPQClear() empties the queue and keeps its mode, and the queue can
 * be used again; in the splay tree mode also with more buckets than the
 * pool of emptied buckets can hold.
 */
TEST_ONCE(ipq_clear)
{
    pddl_ipq_t q;
    pddlIPQInit(&q);
    pddlIPQClear(&q);
    assertEmpty(&q);
    push(&q, "3", 1);
    push(&q, "10", 2);
    pddlIPQClear(&q);
    assertEmpty(&q);
    assert(q.mode == PDDL_IPQ_MODE_ARRAY);
    push(&q, "8", 3);
    push(&q, "1", 4);
    popExpect(&q, 4, "1");
    popExpect(&q, 3, "8");
    assertEmpty(&q);
    pddlIPQFree(&q);

    pddlIPQInit(&q);
    for (int i = 0; i < 3 * PDDL_IPQ_BUCKET_POOL_SIZE; ++i){
        pddl_num_t k;
        pddlNumSetInt(&k, 2000 - i);
        pddlIPQPush(&q, &k, i);
    }
    assert(q.mode == PDDL_IPQ_MODE_SPLAYTREE);
    pddlIPQClear(&q);
    assertEmpty(&q);
    assert(q.mode == PDDL_IPQ_MODE_SPLAYTREE);
    push(&q, "8", 3);
    push(&q, "1", 4);
    popExpect(&q, 4, "1");
    popExpect(&q, 3, "8");
    assertEmpty(&q);
    pddlIPQFree(&q);
}

/** Maximal number of entries of the reference queue of ipq_random */
#define RANDOM_MAX_SIZE 4096

/** Reference queue: entries stored in an array */
struct ref_queue {
    pddl_num_t key[RANDOM_MAX_SIZE];
    int value[RANDOM_MAX_SIZE];
    /** Order of the push of the entry */
    int seq[RANDOM_MAX_SIZE];
    int size;
    int next_seq;
};
typedef struct ref_queue ref_queue_t;

/** Returns the index of the entry with the lowest key and the latest push
 *  among the entries with this key; the reference must not be empty */
static int refLowest(const ref_queue_t *r)
{
    int best = 0;
    for (int i = 1; i < r->size; ++i){
        int cmp = pddlNumCmp(&r->key[i], &r->key[best]);
        if (cmp < 0 || (cmp == 0 && r->seq[i] > r->seq[best]))
            best = i;
    }
    return best;
}

static void refRemove(ref_queue_t *r, int i)
{
    --r->size;
    r->key[i] = r->key[r->size];
    r->value[i] = r->value[r->size];
    r->seq[i] = r->seq[r->size];
}

/** Returns a random key for the key range RANGE:
 *  0 -- integers in [0, 50) (the array mode only),
 *  1 -- integers in [0, PDDL_IPQ_ARRAY_SIZE), rarely out of this range,
 *  2 -- integers, floats (including floats equal to integers), keys with
 *       delta != 0, infinities, and huge keys */
static pddl_num_t randomKey(pddl_rand_t *rnd, int range)
{
    pddl_num_t k;
    if (range == 0){
        pddlNumSetInt(&k, pddlRandInt(rnd) % 50);

    }else if (range == 1){
        if (pddlRandInt(rnd) % 100 == 0){
            pddlNumSetInt(&k, (int)(pddlRandInt(rnd) % 3000) - 1000);
        }else{
            pddlNumSetInt(&k, pddlRandInt(rnd) % PDDL_IPQ_ARRAY_SIZE);
        }

    }else{
        int c = pddlRandInt(rnd) % 20;
        int v = pddlRandInt(rnd) % 20;
        if (c < 8){
            pddlNumSetInt(&k, v);
        }else if (c < 11){
            pddlNumSetFlt(&k, v + .5f);
        }else if (c < 13){
            pddlNumSetFlt(&k, (float)v);
        }else if (c < 16){
            pddlNumSetIntDelta(&k, v, (pddlRandInt(rnd) % 2 == 0 ? 1 : -1));
        }else if (c == 16){
            k = pddl_num_inf;
        }else if (c == 17){
            k = pddl_num_neg_inf;
        }else if (c == 18){
            k = hugeNum(1);
        }else{
            pddlNumSetInt(&k, -v);
        }
    }
    return k;
}

/** Random operations over Q compared with the reference queue R */
static void randomSteps(pddl_ipq_t *q,
                        ref_queue_t *r,
                        pddl_rand_t *rnd,
                        int range,
                        int num_steps)
{
    PDDL_IVEC(out);
    for (int step = 0; step < num_steps; ++step){
        int op = pddlRandInt(rnd) % 100;
        if (op < 55 && r->size < RANDOM_MAX_SIZE){
            pddl_num_t k = randomKey(rnd, range);
            int value = pddlRandInt(rnd) % 1000;
            pddlIPQPush(q, &k, value);
            r->key[r->size] = k;
            r->value[r->size] = value;
            r->seq[r->size] = r->next_seq++;
            ++r->size;

        }else if (op < 85){
            int v;
            pddl_num_t k;
            int ret = pddlIPQPop(q, &v, &k);
            if (r->size == 0){
                assert(ret == -1);
            }else{
                assert(ret == 0);
                int best = refLowest(r);
                assert(v == r->value[best]);
                assert(pddlNumEq(&k, &r->key[best]));
                refRemove(r, best);
            }

        }else if (op < 93){
            pddl_num_t k;
            int ret = pddlIPQPopAll(q, &out, &k);
            if (r->size == 0){
                assert(ret == -1);
                assert(pddlIVecSize(&out) == 0);
                continue;
            }
            assert(ret == 0);
            pddl_num_t low = r->key[refLowest(r)];
            assert(pddlNumEq(&k, &low));
            // OUT is in the order of the pushes, i.e., of increasing seq
            int last_seq = -1;
            PDDL_IVEC_FOR_EACH(&out, v){
                int found = -1;
                for (int i = 0; i < r->size; ++i){
                    if (pddlNumCmp(&r->key[i], &low) == 0
                            && r->seq[i] > last_seq
                            && (found < 0 || r->seq[i] < r->seq[found])){
                        found = i;
                    }
                }
                assert(found >= 0);
                assert(r->value[found] == v);
                last_seq = r->seq[found];
                refRemove(r, found);
            }
            for (int i = 0; i < r->size; ++i)
                assert(pddlNumCmp(&r->key[i], &low) != 0);

        }else if (op < 99){
            pddl_num_t k;
            int ret = pddlIPQTopKey(q, &k);
            if (r->size == 0){
                assert(ret == -1);
            }else{
                assert(ret == 0);
                assert(pddlNumEq(&k, &r->key[refLowest(r)]));
            }

        }else{
            pddlIPQClear(q);
            r->size = 0;
        }
        assert(pddlIPQSize(q) == r->size);
    }
    pddlIVecFree(&out);
}

/*
 * Random pushes, pops, pops of all values with the lowest key, lowest keys,
 * and clears give the same results as a reference queue (the lowest key,
 * LIFO order within equal keys) for integer keys of the array mode, integer
 * keys with the promotion, and keys of all kinds.
 */
TEST_ONCE(ipq_random)
{
    static ref_queue_t ref;
    for (int range = 0; range < 3; ++range){
        for (unsigned seed = 1; seed <= 10; ++seed){
            pddl_rand_t rnd;
            pddlRandInit(&rnd, seed);
            pddl_ipq_t q;
            pddlIPQInit(&q);
            ref.size = 0;
            ref.next_seq = 0;
            randomSteps(&q, &ref, &rnd, range, 3000);
            if (range == 0){
                assert(q.mode == PDDL_IPQ_MODE_ARRAY);
            }else{
                assert(q.mode == PDDL_IPQ_MODE_SPLAYTREE);
            }
            // Drain the queue
            while (ref.size > 0){
                int best = refLowest(&ref);
                int v;
                pddl_num_t k;
                int ret = pddlIPQPop(&q, &v, &k);
                assert(ret == 0);
                assert(v == ref.value[best]);
                assert(pddlNumEq(&k, &ref.key[best]));
                refRemove(&ref, best);
            }
            assertEmpty(&q);
            pddlIPQFree(&q);
            pddlRandFree(&rnd);
        }
    }
}
