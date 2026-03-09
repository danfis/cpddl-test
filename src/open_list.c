/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests for all three splay-tree open-list variants:
 *   pddlOpenListSplayTree1  -- compile-time cost_size = 1
 *   pddlOpenListSplayTree2  -- compile-time cost_size = 2
 *   pddlOpenListSplayTreeN  -- runtime cost_size (tested with 1, 2, and 3)
 *
 * All tests are TEST_ONCE (not per task).
 * Run with:  cd tests && make && ./test -T _ -s open_list
 */

#include "pddl/open_list.h"
#include "test.h"
#include <assert.h>

/*
 * Pop one element and check that both the cost vector and the state ID
 * match the expected values.  cost_size must be <= 3.
 */
static void checkPop(pddl_open_list_t *list, int cost_size,
                     const int *exp_cost, pddl_state_id_t exp_id)
{
    int got_cost[3];
    pddl_state_id_t got_id;
    assert(pddlOpenListPop(list, &got_id, got_cost) == 0);
    for (int i = 0; i < cost_size; ++i)
        assert(got_cost[i] == exp_cost[i]);
    assert(got_id == exp_id);
}

/*
 * Peek at the top element (without removing it) and verify cost and ID.
 */
static void checkTop(pddl_open_list_t *list, int cost_size,
                     const int *exp_cost, pddl_state_id_t exp_id)
{
    int got_cost[3];
    pddl_state_id_t got_id;
    assert(pddlOpenListTop(list, &got_id, got_cost) == 0);
    for (int i = 0; i < cost_size; ++i)
        assert(got_cost[i] == exp_cost[i]);
    assert(got_id == exp_id);
}

/*
 * Assert that both Top and Pop return -1 (list is empty).
 */
static void checkEmpty(pddl_open_list_t *list)
{
    int cost[3];
    pddl_state_id_t id;
    assert(pddlOpenListTop(list, &id, cost) == -1);
    assert(pddlOpenListPop(list, &id, cost) == -1);
}

/*
 * Pop every element from the list and assert that consecutive costs are
 * non-decreasing in lexicographic order.  Returns the total count popped.
 * cost_size must be <= 3.
 */
static int popAllCheckOrder(pddl_open_list_t *list, int cost_size)
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
            for (int i = 0; i < cost_size && (cmp = cur[i] - prev[i]) == 0; ++i);
            assert(cmp >= 0);
        }
        for (int i = 0; i < cost_size; ++i)
            prev[i] = cur[i];
        first = 0;
        ++count;
    }
    return count;
}


/* ---------------------------------------------------------------------- */
/* SplayTree1: 1-D cost                                                    */
/* ---------------------------------------------------------------------- */

TEST_ONCE(open_list_splaytree1)
{
    pddl_open_list_t *list = pddlOpenListSplayTree1();

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
    int costs[] = {7, 2, 9, 1, 4};
    for (int i = 0; i < 5; ++i){
        int c[] = {costs[i]};
        pddlOpenListPush(list, c, (pddl_state_id_t)i);
    }
    int n = popAllCheckOrder(list, 1);
    assert(n == 5);
    checkEmpty(list);

    /* FIFO within same cost: IDs pushed first must come out first */
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

    pddlOpenListDel(list);
}


/* ---------------------------------------------------------------------- */
/* SplayTree2: 2-D cost (primary + one tiebreaker)                        */
/* ---------------------------------------------------------------------- */

TEST_ONCE(open_list_splaytree2)
{
    pddl_open_list_t *list = pddlOpenListSplayTree2();

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
/* SplayTreeN(1): must behave identically to SplayTree1                   */
/* ---------------------------------------------------------------------- */

TEST_ONCE(open_list_splaytreeN_1d)
{
    pddl_open_list_t *list = pddlOpenListSplayTreeN(1);

    checkEmpty(list);

    int costs[] = {10, 3, 7, 1, 8};
    for (int i = 0; i < 5; ++i){
        int c[] = {costs[i]};
        pddlOpenListPush(list, c, (pddl_state_id_t)i);
    }
    int n = popAllCheckOrder(list, 1);
    assert(n == 5);
    checkEmpty(list);

    /* FIFO within same cost */
    int csame[] = {4};
    pddlOpenListPush(list, csame, 7);
    pddlOpenListPush(list, csame, 8);
    checkPop(list, 1, csame, 7);
    checkPop(list, 1, csame, 8);
    checkEmpty(list);

    /* top does not remove */
    int ct[] = {2};
    pddlOpenListPush(list, ct, 99);
    checkTop(list, 1, ct, 99);
    checkPop(list, 1, ct, 99);
    checkEmpty(list);

    /* clear */
    pddlOpenListPush(list, ct, 1);
    pddlOpenListClear(list);
    checkEmpty(list);

    pddlOpenListDel(list);
}


/* ---------------------------------------------------------------------- */
/* SplayTreeN(2): must behave identically to SplayTree2                   */
/* ---------------------------------------------------------------------- */

TEST_ONCE(open_list_splaytreeN_2d)
{
    pddl_open_list_t *list = pddlOpenListSplayTreeN(2);

    checkEmpty(list);

    /* primary key dominates */
    int ca[] = {5, 0};
    int cb[] = {3, 9};
    pddlOpenListPush(list, ca, 1);
    pddlOpenListPush(list, cb, 2);
    checkPop(list, 2, cb, 2);
    checkPop(list, 2, ca, 1);
    checkEmpty(list);

    /* secondary tiebreaker */
    int c1[] = {4, 1};
    int c2[] = {4, 5};
    pddlOpenListPush(list, c2, 20);
    pddlOpenListPush(list, c1, 10);
    checkPop(list, 2, c1, 10);  /* (4,1) before (4,5) */
    checkPop(list, 2, c2, 20);
    checkEmpty(list);

    /* FIFO within same (key0, key1) */
    int csame[] = {2, 2};
    pddlOpenListPush(list, csame, 11);
    pddlOpenListPush(list, csame, 12);
    pddlOpenListPush(list, csame, 13);
    checkPop(list, 2, csame, 11);
    checkPop(list, 2, csame, 12);
    checkPop(list, 2, csame, 13);
    checkEmpty(list);

    /* same as splaytree2: larger mix */
    int entries[][2] = {{2,5},{1,3},{1,1},{3,0},{2,2},{1,3},{2,2}};
    int n_entries = 7;
    for (int i = 0; i < n_entries; ++i)
        pddlOpenListPush(list, entries[i], (pddl_state_id_t)i);
    int n = popAllCheckOrder(list, 2);
    assert(n == n_entries);
    checkEmpty(list);

    pddlOpenListDel(list);
}


/* ---------------------------------------------------------------------- */
/* SplayTreeN(3): 3-D lexicographic ordering                              */
/* ---------------------------------------------------------------------- */

TEST_ONCE(open_list_splaytreeN_3d)
{
    pddl_open_list_t *list = pddlOpenListSplayTreeN(3);

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

    /* FIFO within identical 3-D cost */
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

    /* larger mix: 12 entries with various 3-D costs, verify global order */
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
