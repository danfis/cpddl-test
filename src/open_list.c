/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the open list (pddl/open_list.h) with all key types:
 *   PDDL_OPEN_LIST_KEY_INT      -- int keys of size 1, 2 and 3
 *   PDDL_OPEN_LIST_KEY_COST     -- pddl_cost_t keys of size 1 and 2
 *   PDDL_OPEN_LIST_KEY_NUM_VAL  -- pddl_num_val_t keys of size 1 and 2
 *
 * All tests are TEST_ONCE (not per task).
 * Run with:  cd tests && make && ./test -T _ -s open_list
 */

#include "pddl/open_list.h"
#include "pddl/cost.h"
#include "pddl/num_val.h"
#include "test.h"
#include <assert.h>

/*
 * Pop one element and check that both the key and the state ID match the
 * expected values.  key_size must be <= 3.
 */
static void checkPop(pddl_open_list_t *list, int key_size,
                     const int *exp_key, pddl_state_id_t exp_id)
{
    int got_key[3];
    pddl_state_id_t got_id;
    int ret = pddlOpenListPop(list, &got_id, got_key);
    assert(ret == 0);
    for (int i = 0; i < key_size; ++i)
        assert(got_key[i] == exp_key[i]);
    assert(got_id == exp_id);
}

/*
 * Peek at the top element (without removing it) and verify key and ID.
 */
static void checkTop(pddl_open_list_t *list, int key_size,
                     const int *exp_key, pddl_state_id_t exp_id)
{
    int got_key[3];
    pddl_state_id_t got_id;
    int ret = pddlOpenListTop(list, &got_id, got_key);
    assert(ret == 0);
    for (int i = 0; i < key_size; ++i)
        assert(got_key[i] == exp_key[i]);
    assert(got_id == exp_id);
}

/*
 * Assert that both Top and Pop return -1 (list is empty).
 * The buffer is large enough for any key type used in these tests.
 */
static void checkEmpty(pddl_open_list_t *list)
{
    pddl_num_val_t key[3];
    pddl_state_id_t id;
    int ret = pddlOpenListTop(list, &id, key);
    assert(ret == -1);
    ret = pddlOpenListPop(list, &id, key);
    assert(ret == -1);
}

/*
 * Pop every element from the list and assert that consecutive keys are
 * non-decreasing in lexicographic order.  Returns the total count popped.
 * key_size must be <= 3.
 */
static int popAllCheckOrder(pddl_open_list_t *list, int key_size)
{
    int prev[3] = { 0 };
    int cur[3];
    pddl_state_id_t id;
    int count = 0;
    int first = 1;

    while (pddlOpenListPop(list, &id, cur) == 0){
        if (!first){
            /* Compute lexicographic comparison cur vs prev. */
            int cmp = 0;
            for (int i = 0; i < key_size && (cmp = cur[i] - prev[i]) == 0; ++i);
            assert(cmp >= 0);
        }
        for (int i = 0; i < key_size; ++i)
            prev[i] = cur[i];
        first = 0;
        ++count;
    }
    return count;
}


/* ---------------------------------------------------------------------- */
/* INT, 1-D key                                                            */
/* ---------------------------------------------------------------------- */

TEST_ONCE(open_list_int_1d)
{
    pddl_open_list_t *list = pddlOpenListNew(PDDL_OPEN_LIST_KEY_INT, 1);

    /* empty list: both Top and Pop must return -1 */
    checkEmpty(list);

    /* single push then pop */
    int c5[] = {5};
    pddlOpenListPush(list, c5, 42);
    checkTop(list, 1, c5, 42);  /* top does not remove */
    checkTop(list, 1, c5, 42);  /* still there */
    checkPop(list, 1, c5, 42);
    checkEmpty(list);

    /* push in non-ascending order, verify ascending pop */
    int keys[] = {7, 2, 9, 1, 4};
    for (int i = 0; i < 5; ++i){
        int c[] = {keys[i]};
        pddlOpenListPush(list, c, (pddl_state_id_t)i);
    }
    int n = popAllCheckOrder(list, 1);
    assert(n == 5);
    checkEmpty(list);

    /* FIFO within same key: IDs pushed first must come out first */
    int csame[] = {3};
    pddlOpenListPush(list, csame, 10);
    pddlOpenListPush(list, csame, 20);
    pddlOpenListPush(list, csame, 30);
    checkPop(list, 1, csame, 10);
    checkPop(list, 1, csame, 20);
    checkPop(list, 1, csame, 30);
    checkEmpty(list);

    /* clear: push a few elements, clear, list must be empty */
    pddlOpenListPush(list, c5, 1);
    pddlOpenListPush(list, c5, 2);
    pddlOpenListClear(list);
    checkEmpty(list);

    /* push after clear works correctly */
    int c1[] = {1};
    pddlOpenListPush(list, c1, 99);
    checkPop(list, 1, c1, 99);
    checkEmpty(list);

    /* extreme values must be ordered without overflow */
    int cmin[] = {INT_MIN};
    int cmax[] = {INT_MAX};
    int czero[] = {0};
    pddlOpenListPush(list, cmax, 1);
    pddlOpenListPush(list, czero, 2);
    pddlOpenListPush(list, cmin, 3);
    checkPop(list, 1, cmin, 3);
    checkPop(list, 1, czero, 2);
    checkPop(list, 1, cmax, 1);
    checkEmpty(list);

    /* many distinct keys (more than any internal bucket pool), interleaved
     * pops, so that buckets are created, emptied and re-created */
    for (int round = 0; round < 3; ++round){
        for (int i = 0; i < 100; ++i){
            int c[] = {(i * 37) % 101};
            pddlOpenListPush(list, c, (pddl_state_id_t)i);
        }
        n = popAllCheckOrder(list, 1);
        assert(n == 100);
        checkEmpty(list);
    }
    for (int i = 0; i < 100; ++i){
        int c[] = {i};
        pddlOpenListPush(list, c, (pddl_state_id_t)i);
    }
    pddlOpenListClear(list);
    checkEmpty(list);

    pddlOpenListDel(list);
}


/* ---------------------------------------------------------------------- */
/* INT, 2-D key (primary + one tiebreaker)                                 */
/* ---------------------------------------------------------------------- */

TEST_ONCE(open_list_int_2d)
{
    pddl_open_list_t *list = pddlOpenListNew(PDDL_OPEN_LIST_KEY_INT, 2);

    checkEmpty(list);

    /* primary key takes precedence: (3,9) must come before (5,0) */
    int ca[] = {5, 0};
    int cb[] = {3, 9};
    pddlOpenListPush(list, ca, 1);
    pddlOpenListPush(list, cb, 2);
    checkPop(list, 2, cb, 2);  /* (3,...) < (5,...) */
    checkPop(list, 2, ca, 1);
    checkEmpty(list);

    /* secondary key breaks ties on equal primary */
    int c1[] = {4, 1};
    int c2[] = {4, 5};
    int c3[] = {4, 2};
    pddlOpenListPush(list, c2, 20);
    pddlOpenListPush(list, c3, 30);
    pddlOpenListPush(list, c1, 10);
    checkPop(list, 2, c1, 10);  /* (4,1) */
    checkPop(list, 2, c3, 30);  /* (4,2) */
    checkPop(list, 2, c2, 20);  /* (4,5) */
    checkEmpty(list);

    /* FIFO within identical (key0, key1) */
    int csame[] = {6, 6};
    pddlOpenListPush(list, csame, 100);
    pddlOpenListPush(list, csame, 101);
    pddlOpenListPush(list, csame, 102);
    checkPop(list, 2, csame, 100);
    checkPop(list, 2, csame, 101);
    checkPop(list, 2, csame, 102);
    checkEmpty(list);

    /* top does not remove the element */
    int ct[] = {8, 1};
    pddlOpenListPush(list, ct, 55);
    checkTop(list, 2, ct, 55);
    checkTop(list, 2, ct, 55);
    checkPop(list, 2, ct, 55);
    checkEmpty(list);

    /* larger mix: push many elements with repeated keys, verify global order */
    int entries[][2] = {{2,5},{1,3},{1,1},{3,0},{2,2},{1,3},{2,2}};
    int n_entries = 7;
    for (int i = 0; i < n_entries; ++i)
        pddlOpenListPush(list, entries[i], (pddl_state_id_t)i);
    int n = popAllCheckOrder(list, 2);
    assert(n == n_entries);
    checkEmpty(list);

    /* clear */
    pddlOpenListPush(list, ca, 1);
    pddlOpenListPush(list, cb, 2);
    pddlOpenListClear(list);
    checkEmpty(list);

    pddlOpenListDel(list);
}


/* ---------------------------------------------------------------------- */
/* INT, 3-D key: lexicographic ordering                                    */
/* ---------------------------------------------------------------------- */

TEST_ONCE(open_list_int_3d)
{
    pddl_open_list_t *list = pddlOpenListNew(PDDL_OPEN_LIST_KEY_INT, 3);

    checkEmpty(list);

    /* primary key dominates: (1,...) < (2,...) */
    int ca[] = {2, 9, 9};
    int cb[] = {1, 0, 0};
    pddlOpenListPush(list, ca, 1);
    pddlOpenListPush(list, cb, 2);
    checkPop(list, 3, cb, 2);
    checkPop(list, 3, ca, 1);
    checkEmpty(list);

    /* secondary tiebreaker: equal primary, (5,2,...) < (5,7,...) */
    int c1[] = {5, 2, 9};
    int c2[] = {5, 7, 0};
    pddlOpenListPush(list, c2, 20);
    pddlOpenListPush(list, c1, 10);
    checkPop(list, 3, c1, 10);
    checkPop(list, 3, c2, 20);
    checkEmpty(list);

    /* tertiary tiebreaker: equal primary+secondary, (5,3,1) < (5,3,8) */
    int c3[] = {5, 3, 1};
    int c4[] = {5, 3, 8};
    pddlOpenListPush(list, c4, 40);
    pddlOpenListPush(list, c3, 30);
    checkPop(list, 3, c3, 30);
    checkPop(list, 3, c4, 40);
    checkEmpty(list);

    /* FIFO within identical 3-D key */
    int csame[] = {3, 3, 3};
    pddlOpenListPush(list, csame, 100);
    pddlOpenListPush(list, csame, 101);
    pddlOpenListPush(list, csame, 102);
    checkPop(list, 3, csame, 100);
    checkPop(list, 3, csame, 101);
    checkPop(list, 3, csame, 102);
    checkEmpty(list);

    /* top does not remove */
    int ct[] = {1, 1, 1};
    pddlOpenListPush(list, ct, 77);
    checkTop(list, 3, ct, 77);
    checkTop(list, 3, ct, 77);
    checkPop(list, 3, ct, 77);
    checkEmpty(list);

    /* larger mix: 12 entries with various 3-D keys, verify global order */
    int entries[][3] = {
        {3,1,2}, {1,5,0}, {3,1,1}, {2,2,2},
        {1,5,0}, {3,1,2}, {2,1,9}, {1,4,7},
        {2,2,1}, {3,0,5}, {1,5,1}, {2,1,8}
    };
    int n_entries = 12;
    for (int i = 0; i < n_entries; ++i)
        pddlOpenListPush(list, entries[i], (pddl_state_id_t)i);
    int n = popAllCheckOrder(list, 3);
    assert(n == n_entries);
    checkEmpty(list);

    /* clear */
    pddlOpenListPush(list, ca, 5);
    pddlOpenListPush(list, cb, 6);
    pddlOpenListClear(list);
    checkEmpty(list);

    pddlOpenListDel(list);
}


/* ---------------------------------------------------------------------- */
/* COST keys                                                               */
/* ---------------------------------------------------------------------- */

static void checkPopCost(pddl_open_list_t *list, int key_size,
                         const pddl_cost_t *exp_key, pddl_state_id_t exp_id)
{
    pddl_cost_t got_key[2];
    pddl_state_id_t got_id;
    int ret = pddlOpenListPop(list, &got_id, got_key);
    assert(ret == 0);
    for (int i = 0; i < key_size; ++i)
        assert(pddlCostCmp(got_key + i, exp_key + i) == 0);
    assert(got_id == exp_id);
}

static void checkTopCost(pddl_open_list_t *list, int key_size,
                         const pddl_cost_t *exp_key, pddl_state_id_t exp_id)
{
    pddl_cost_t got_key[2];
    pddl_state_id_t got_id;
    int ret = pddlOpenListTop(list, &got_id, got_key);
    assert(ret == 0);
    for (int i = 0; i < key_size; ++i)
        assert(pddlCostCmp(got_key + i, exp_key + i) == 0);
    assert(got_id == exp_id);
}

TEST_ONCE(open_list_cost_1d)
{
    pddl_open_list_t *list = pddlOpenListNew(PDDL_OPEN_LIST_KEY_COST, 1);

    checkEmpty(list);

    /* .cost is the primary criterion, .zero_cost the secondary one */
    pddl_cost_t c30 = { .cost = 3, .zero_cost = 0 };
    pddl_cost_t c31 = { .cost = 3, .zero_cost = 1 };
    pddl_cost_t c40 = { .cost = 4, .zero_cost = 0 };
    pddl_cost_t c02 = { .cost = 0, .zero_cost = 2 };
    pddlOpenListPush(list, &c40, 1);
    pddlOpenListPush(list, &c31, 2);
    pddlOpenListPush(list, &c30, 3);
    pddlOpenListPush(list, &c02, 4);
    checkTopCost(list, 1, &c02, 4);
    checkTopCost(list, 1, &c02, 4);
    checkPopCost(list, 1, &c02, 4);
    checkPopCost(list, 1, &c30, 3);
    checkPopCost(list, 1, &c31, 2);
    checkPopCost(list, 1, &c40, 1);
    checkEmpty(list);

    /* FIFO within the same cost, built with the pddlCostSet*() helpers */
    pddl_cost_t czero, cop0, cop5;
    pddlCostSetZero(&czero);
    pddlCostSetOp(&cop0, 0);
    pddlCostSetOp(&cop5, 5);
    pddlOpenListPush(list, &cop5, 50);
    pddlOpenListPush(list, &cop0, 10);
    pddlOpenListPush(list, &cop0, 11);
    pddlOpenListPush(list, &czero, 1);
    pddlOpenListPush(list, &czero, 2);
    pddlOpenListPush(list, &cop0, 12);
    checkPopCost(list, 1, &czero, 1);   /* (0,0) */
    checkPopCost(list, 1, &czero, 2);
    checkPopCost(list, 1, &cop0, 10);   /* (0,1) */
    checkPopCost(list, 1, &cop0, 11);
    checkPopCost(list, 1, &cop0, 12);
    checkPopCost(list, 1, &cop5, 50);   /* (5,0) */
    checkEmpty(list);

    /* dead-end and max costs sort last */
    pddlOpenListPush(list, &pddl_cost_dead_end, 1);
    pddlOpenListPush(list, &pddl_cost_max, 2);
    pddlOpenListPush(list, &cop5, 3);
    checkPopCost(list, 1, &cop5, 3);
    if (pddlCostCmp(&pddl_cost_dead_end, &pddl_cost_max) < 0){
        checkPopCost(list, 1, &pddl_cost_dead_end, 1);
        checkPopCost(list, 1, &pddl_cost_max, 2);
    }else{
        checkPopCost(list, 1, &pddl_cost_max, 2);
        checkPopCost(list, 1, &pddl_cost_dead_end, 1);
    }
    checkEmpty(list);

    /* clear */
    pddlOpenListPush(list, &c30, 1);
    pddlOpenListPush(list, &c40, 2);
    pddlOpenListClear(list);
    checkEmpty(list);

    pddlOpenListDel(list);
}

TEST_ONCE(open_list_cost_2d)
{
    pddl_open_list_t *list = pddlOpenListNew(PDDL_OPEN_LIST_KEY_COST, 2);

    checkEmpty(list);

    /* primary key dominates */
    pddl_cost_t ka[2] = { { .cost = 5, .zero_cost = 0 },
                          { .cost = 0, .zero_cost = 0 } };
    pddl_cost_t kb[2] = { { .cost = 3, .zero_cost = 2 },
                          { .cost = 9, .zero_cost = 9 } };
    pddlOpenListPush(list, ka, 1);
    pddlOpenListPush(list, kb, 2);
    checkPopCost(list, 2, kb, 2);
    checkPopCost(list, 2, ka, 1);
    checkEmpty(list);

    /* .zero_cost of the primary key beats the secondary key */
    pddl_cost_t k1[2] = { { .cost = 4, .zero_cost = 1 },
                          { .cost = 0, .zero_cost = 0 } };
    pddl_cost_t k2[2] = { { .cost = 4, .zero_cost = 0 },
                          { .cost = 8, .zero_cost = 0 } };
    pddlOpenListPush(list, k1, 10);
    pddlOpenListPush(list, k2, 20);
    checkPopCost(list, 2, k2, 20);
    checkPopCost(list, 2, k1, 10);
    checkEmpty(list);

    /* secondary tiebreaker on equal primary */
    pddl_cost_t k3[2] = { { .cost = 4, .zero_cost = 0 },
                          { .cost = 2, .zero_cost = 0 } };
    pddl_cost_t k4[2] = { { .cost = 4, .zero_cost = 0 },
                          { .cost = 2, .zero_cost = 1 } };
    pddlOpenListPush(list, k4, 40);
    pddlOpenListPush(list, k2, 20);
    pddlOpenListPush(list, k3, 30);
    checkPopCost(list, 2, k3, 30);
    checkPopCost(list, 2, k4, 40);
    checkPopCost(list, 2, k2, 20);
    checkEmpty(list);

    /* FIFO within identical keys */
    pddlOpenListPush(list, k3, 100);
    pddlOpenListPush(list, k3, 101);
    pddlOpenListPush(list, k3, 102);
    checkTopCost(list, 2, k3, 100);
    checkPopCost(list, 2, k3, 100);
    checkPopCost(list, 2, k3, 101);
    checkPopCost(list, 2, k3, 102);
    checkEmpty(list);

    /* clear */
    pddlOpenListPush(list, ka, 1);
    pddlOpenListPush(list, kb, 2);
    pddlOpenListClear(list);
    checkEmpty(list);

    pddlOpenListDel(list);
}


/* ---------------------------------------------------------------------- */
/* NUM_VAL keys                                                            */
/* ---------------------------------------------------------------------- */

static void checkPopNumVal(pddl_open_list_t *list, int key_size,
                           const pddl_num_val_t *exp_key,
                           pddl_state_id_t exp_id)
{
    pddl_num_val_t got_key[2];
    pddl_state_id_t got_id;
    int ret = pddlOpenListPop(list, &got_id, got_key);
    assert(ret == 0);
    for (int i = 0; i < key_size; ++i)
        assert(pddlNumValCmp(got_key + i, exp_key + i) == 0);
    assert(got_id == exp_id);
}

static void checkTopNumVal(pddl_open_list_t *list, int key_size,
                           const pddl_num_val_t *exp_key,
                           pddl_state_id_t exp_id)
{
    pddl_num_val_t got_key[2];
    pddl_state_id_t got_id;
    int ret = pddlOpenListTop(list, &got_id, got_key);
    assert(ret == 0);
    for (int i = 0; i < key_size; ++i)
        assert(pddlNumValCmp(got_key + i, exp_key + i) == 0);
    assert(got_id == exp_id);
}

TEST_ONCE(open_list_num_val_1d)
{
    pddl_open_list_t *list = pddlOpenListNew(PDDL_OPEN_LIST_KEY_NUM_VAL, 1);

    checkEmpty(list);

    /* mixed integers and floats are ordered by value: -1 < 1.5 < 2 < 2.5 */
    pddl_num_val_t vneg, v15, v2i, v2f, v25;
    pddlNumValSetInt(&vneg, -1);
    pddlNumValSetFlt(&v15, 1.5);
    pddlNumValSetInt(&v2i, 2);
    pddlNumValSetFlt(&v2f, 2.0);
    pddlNumValSetFlt(&v25, 2.5);
    pddlOpenListPush(list, &v25, 1);
    pddlOpenListPush(list, &v2i, 2);
    pddlOpenListPush(list, &v2f, 3);  /* same bucket as v2i: FIFO after 2 */
    pddlOpenListPush(list, &v15, 4);
    pddlOpenListPush(list, &vneg, 5);
    checkTopNumVal(list, 1, &vneg, 5);
    checkTopNumVal(list, 1, &vneg, 5);
    checkPopNumVal(list, 1, &vneg, 5);
    checkPopNumVal(list, 1, &v15, 4);
    checkPopNumVal(list, 1, &v2i, 2);
    checkPopNumVal(list, 1, &v2f, 3);  /* 2 == 2.0 under pddlNumValCmp() */
    checkPopNumVal(list, 1, &v25, 1);
    checkEmpty(list);

    /* FIFO within the same value */
    pddlOpenListPush(list, &v15, 10);
    pddlOpenListPush(list, &v15, 11);
    pddlOpenListPush(list, &v15, 12);
    checkPopNumVal(list, 1, &v15, 10);
    checkPopNumVal(list, 1, &v15, 11);
    checkPopNumVal(list, 1, &v15, 12);
    checkEmpty(list);

    /* many distinct values, verify ascending order */
    for (int i = 0; i < 50; ++i){
        pddl_num_val_t v;
        if (i % 2 == 0){
            pddlNumValSetInt(&v, (i * 17) % 53 - 20);
        }else{
            pddlNumValSetFlt(&v, ((i * 17) % 53 - 20) + 0.25);
        }
        pddlOpenListPush(list, &v, (pddl_state_id_t)i);
    }
    pddl_num_val_t prev, cur;
    pddl_state_id_t id;
    int count = 0;
    while (pddlOpenListPop(list, &id, &cur) == 0){
        if (count > 0)
            assert(pddlNumValCmp(&prev, &cur) <= 0);
        prev = cur;
        ++count;
    }
    assert(count == 50);
    checkEmpty(list);

    /* clear */
    pddlOpenListPush(list, &v15, 1);
    pddlOpenListPush(list, &v25, 2);
    pddlOpenListClear(list);
    checkEmpty(list);

    pddlOpenListDel(list);
}

TEST_ONCE(open_list_num_val_2d)
{
    pddl_open_list_t *list = pddlOpenListNew(PDDL_OPEN_LIST_KEY_NUM_VAL, 2);

    checkEmpty(list);

    /* primary key dominates: (1.5, 100) < (2, -100) */
    pddl_num_val_t ka[2], kb[2];
    pddlNumValSetInt(&ka[0], 2);
    pddlNumValSetInt(&ka[1], -100);
    pddlNumValSetFlt(&kb[0], 1.5);
    pddlNumValSetInt(&kb[1], 100);
    pddlOpenListPush(list, ka, 1);
    pddlOpenListPush(list, kb, 2);
    checkPopNumVal(list, 2, kb, 2);
    checkPopNumVal(list, 2, ka, 1);
    checkEmpty(list);

    /* secondary tiebreaker on equal primary (int 2 vs float 2.0) */
    pddl_num_val_t k1[2], k2[2];
    pddlNumValSetInt(&k1[0], 2);
    pddlNumValSetFlt(&k1[1], 0.5);
    pddlNumValSetFlt(&k2[0], 2.0);
    pddlNumValSetInt(&k2[1], 1);
    pddlOpenListPush(list, k2, 20);
    pddlOpenListPush(list, k1, 10);
    checkTopNumVal(list, 2, k1, 10);
    checkPopNumVal(list, 2, k1, 10);
    checkPopNumVal(list, 2, k2, 20);
    checkEmpty(list);

    /* FIFO within identical keys */
    pddlOpenListPush(list, k1, 100);
    pddlOpenListPush(list, k1, 101);
    pddlOpenListPush(list, k1, 102);
    checkPopNumVal(list, 2, k1, 100);
    checkPopNumVal(list, 2, k1, 101);
    checkPopNumVal(list, 2, k1, 102);
    checkEmpty(list);

    /* clear */
    pddlOpenListPush(list, ka, 1);
    pddlOpenListPush(list, kb, 2);
    pddlOpenListClear(list);
    checkEmpty(list);

    pddlOpenListDel(list);
}
