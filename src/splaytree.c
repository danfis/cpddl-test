/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the generic splay tree (pddl/splaytree.h).
 */

#include "pddl/splaytree.h"
#include "test.h"
#include <assert.h>

struct item {
    int key;
    pddl_splaytree_node_t node;
    /** Set by the clear callback */
    int cleared;
};
typedef struct item item_t;

#define ITEM(n) pddl_container_of((n), item_t, node)

static int itemCmp(const pddl_splaytree_node_t *n1,
                   const pddl_splaytree_node_t *n2,
                   void *_)
{
    int k1 = ITEM(n1)->key;
    int k2 = ITEM(n2)->key;
    return (k1 > k2) - (k1 < k2);
}

struct clear_ctx {
    int count;
    int last_key;
};
typedef struct clear_ctx clear_ctx_t;

static void itemClear(pddl_splaytree_node_t *n, void *data)
{
    clear_ctx_t *ctx = data;
    item_t *it = ITEM(n);
    assert(it->cleared == 0);
    // nodes must be cleared in ascending order
    assert(it->key > ctx->last_key);
    it->cleared = 1;
    ctx->last_key = it->key;
    ++ctx->count;
}

/* Walks the tree in ascending order, asserts strictly increasing keys and
 * returns the number of nodes. */
static int checkAscending(pddl_splaytree_t *tree)
{
    int count = 0;
    int prev = 0;

    PDDL_SPLAYTREE_FOR_EACH(tree, n){
        if (count > 0)
            assert(ITEM(n)->key > prev);
        prev = ITEM(n)->key;
        ++count;
    }
    return count;
}

/* Same as checkAscending() but in reverse. */
static int checkDescending(pddl_splaytree_t *tree)
{
    int count = 0;
    int prev = 0;

    PDDL_SPLAYTREE_FOR_EACH_REVERSE(tree, n){
        if (count > 0)
            assert(ITEM(n)->key < prev);
        prev = ITEM(n)->key;
        ++count;
    }
    return count;
}

/* Deterministic LCG so the random test is reproducible. */
static unsigned nextRand(unsigned *seed)
{
    *seed = *seed * 1103515245u + 12345u;
    return (*seed >> 16) & 0x7fff;
}

static void insertAll(pddl_splaytree_t *tree, item_t *items,
                      const int *keys, int size)
{
    for (int i = 0; i < size; ++i){
        items[i].key = keys[i];
        items[i].cleared = 0;
        pddl_splaytree_node_t *ret = pddlSplayTreeInsert(tree, &items[i].node);
        assert(ret == NULL);
    }
}

/* Returns the node with the given key or NULL. */
static pddl_splaytree_node_t *findKey(pddl_splaytree_t *tree, int key)
{
    item_t k = { .key = key };
    return pddlSplayTreeFind(tree, &k.node);
}

static const int keys[] = { 7, 2, 9, 1, 4, 8, 3 };
static const int keys_sorted[] = { 1, 2, 3, 4, 7, 8, 9 };
#define KEYS_SIZE 7


TEST_ONCE(splaytree_empty)
{
    pddl_splaytree_t *tree = pddlSplayTreeNew(itemCmp, NULL);
    item_t key = { .key = 1 };
    clear_ctx_t ctx = { 0, -1 };

    assert(pddlSplayTreeIsEmpty(tree));
    assert(pddlSplayTreeMin(tree) == NULL);
    assert(pddlSplayTreeMax(tree) == NULL);
    assert(pddlSplayTreeFind(tree, &key.node) == NULL);
    assert(pddlSplayTreeNext(tree, &key.node) == NULL);
    assert(pddlSplayTreePrev(tree, &key.node) == NULL);

    pddl_splaytree_node_t *ret = pddlSplayTreeExtractMin(tree);
    assert(ret == NULL);
    ret = pddlSplayTreeRemove(tree, &key.node);
    assert(ret == NULL);

    pddlSplayTreeClear(tree, itemClear, &ctx);
    assert(ctx.count == 0);
    assert(pddlSplayTreeIsEmpty(tree));
    assert(checkAscending(tree) == 0);
    assert(checkDescending(tree) == 0);

    pddlSplayTreeDel(tree);
}

TEST_ONCE(splaytree_insert_find)
{
    pddl_splaytree_t tree;
    item_t items[KEYS_SIZE];

    pddlSplayTreeInit(&tree, itemCmp, NULL);
    insertAll(&tree, items, keys, KEYS_SIZE);
    assert(!pddlSplayTreeIsEmpty(&tree));
    assert(checkAscending(&tree) == KEYS_SIZE);
    assert(checkDescending(&tree) == KEYS_SIZE);

    // inserting a duplicate key returns the stored node and changes nothing
    item_t dup = { .key = 4 };
    pddl_splaytree_node_t *ret = pddlSplayTreeInsert(&tree, &dup.node);
    assert(ret == &items[4].node);
    assert(checkAscending(&tree) == KEYS_SIZE);

    // every stored key is found and the stored node is returned
    for (int i = 0; i < KEYS_SIZE; ++i){
        pddl_splaytree_node_t *found = findKey(&tree, keys[i]);
        assert(found == &items[i].node);
        assert(ITEM(found)->key == keys[i]);
    }

    // absent keys are not found
    const int absent[] = { -1, 0, 5, 6, 10 };
    for (int i = 0; i < 5; ++i){
        pddl_splaytree_node_t *found = findKey(&tree, absent[i]);
        assert(found == NULL);
    }
    // lookups do not change the content
    assert(checkAscending(&tree) == KEYS_SIZE);

    pddl_splaytree_node_t *n = pddlSplayTreeMin(&tree);
    assert(ITEM(n)->key == 1);
    n = pddlSplayTreeMax(&tree);
    assert(ITEM(n)->key == 9);

    pddlSplayTreeFree(&tree);
    assert(tree.root == NULL);
}

TEST_ONCE(splaytree_next_prev)
{
    pddl_splaytree_t tree;
    item_t items[KEYS_SIZE];

    pddlSplayTreeInit(&tree, itemCmp, NULL);
    insertAll(&tree, items, keys, KEYS_SIZE);

    // Next chain from the minimum follows the sorted order
    pddl_splaytree_node_t *n = pddlSplayTreeMin(&tree);
    for (int i = 0; i < KEYS_SIZE; ++i){
        assert(n != NULL);
        assert(ITEM(n)->key == keys_sorted[i]);
        n = pddlSplayTreeNext(&tree, n);
    }
    assert(n == NULL);

    // Prev chain from the maximum
    n = pddlSplayTreeMax(&tree);
    for (int i = KEYS_SIZE - 1; i >= 0; --i){
        assert(n != NULL);
        assert(ITEM(n)->key == keys_sorted[i]);
        n = pddlSplayTreePrev(&tree, n);
    }
    assert(n == NULL);

    // key nodes that are not in the tree: nearest greater/smaller
    item_t k5 = { .key = 5 };
    n = pddlSplayTreeNext(&tree, &k5.node);
    assert(n != NULL && ITEM(n)->key == 7);
    n = pddlSplayTreePrev(&tree, &k5.node);
    assert(n != NULL && ITEM(n)->key == 4);

    item_t k0 = { .key = 0 };
    n = pddlSplayTreePrev(&tree, &k0.node);
    assert(n == NULL);
    n = pddlSplayTreeNext(&tree, &k0.node);
    assert(n != NULL && ITEM(n)->key == 1);

    item_t k10 = { .key = 10 };
    n = pddlSplayTreeNext(&tree, &k10.node);
    assert(n == NULL);
    n = pddlSplayTreePrev(&tree, &k10.node);
    assert(n != NULL && ITEM(n)->key == 9);

    // key node equal to a stored key behaves like the stored node
    item_t k4 = { .key = 4 };
    n = pddlSplayTreeNext(&tree, &k4.node);
    assert(n != NULL && ITEM(n)->key == 7);
    n = pddlSplayTreePrev(&tree, &k4.node);
    assert(n != NULL && ITEM(n)->key == 3);

    // FOR_EACH_FROM / FOR_EACH_REVERSE_FROM starting at key 4
    int idx = 3;
    PDDL_SPLAYTREE_FOR_EACH_FROM(&tree, findKey(&tree, 4), m){
        assert(idx < KEYS_SIZE);
        assert(ITEM(m)->key == keys_sorted[idx]);
        ++idx;
    }
    assert(idx == KEYS_SIZE);

    idx = 3;
    PDDL_SPLAYTREE_FOR_EACH_REVERSE_FROM(&tree, findKey(&tree, 4), m){
        assert(idx >= 0);
        assert(ITEM(m)->key == keys_sorted[idx]);
        --idx;
    }
    assert(idx == -1);

    // starting from NULL never enters the loop
    PDDL_SPLAYTREE_FOR_EACH_FROM(&tree, NULL, m){
        assert(0);
    }
    PDDL_SPLAYTREE_FOR_EACH_REVERSE_FROM(&tree, NULL, m){
        assert(0);
    }

    assert(checkAscending(&tree) == KEYS_SIZE);
    pddlSplayTreeFree(&tree);
}

TEST_ONCE(splaytree_remove)
{
    pddl_splaytree_t tree;
    item_t items[KEYS_SIZE];
    pddl_splaytree_node_t *ret;
    int size = KEYS_SIZE;

    pddlSplayTreeInit(&tree, itemCmp, NULL);
    insertAll(&tree, items, keys, KEYS_SIZE);

    // remove the minimum (items[3] has key 1)
    ret = pddlSplayTreeRemove(&tree, &items[3].node);
    assert(ret == &items[3].node);
    assert(checkAscending(&tree) == --size);
    ret = findKey(&tree, 1);
    assert(ret == NULL);

    // remove the maximum (items[2] has key 9)
    ret = pddlSplayTreeRemove(&tree, &items[2].node);
    assert(ret == &items[2].node);
    assert(checkAscending(&tree) == --size);
    ret = findKey(&tree, 9);
    assert(ret == NULL);

    // remove an interior node (items[4] has key 4)
    ret = pddlSplayTreeRemove(&tree, &items[4].node);
    assert(ret == &items[4].node);
    assert(checkAscending(&tree) == --size);
    assert(checkDescending(&tree) == size);

    // removing an absent key does nothing
    item_t k5 = { .key = 5 };
    ret = pddlSplayTreeRemove(&tree, &k5.node);
    assert(ret == NULL);
    assert(checkAscending(&tree) == size);

    // removing through a search key node returns the stored node
    item_t k7 = { .key = 7 };
    ret = pddlSplayTreeRemove(&tree, &k7.node);
    assert(ret == &items[0].node);
    assert(checkAscending(&tree) == --size);

    // remove the current root: Find splays the found node to the root
    ret = findKey(&tree, 8);
    assert(ret == &items[5].node);
    assert(tree.root == &items[5].node);
    ret = pddlSplayTreeRemove(&tree, tree.root);
    assert(ret == &items[5].node);
    assert(checkAscending(&tree) == --size);

    // remaining keys are 2 and 3
    ret = pddlSplayTreeMin(&tree);
    assert(ITEM(ret)->key == 2);
    ret = pddlSplayTreeMax(&tree);
    assert(ITEM(ret)->key == 3);

    // removed nodes can be re-inserted
    ret = pddlSplayTreeInsert(&tree, &items[3].node);
    assert(ret == NULL);
    ret = pddlSplayTreeInsert(&tree, &items[2].node);
    assert(ret == NULL);
    size += 2;
    assert(checkAscending(&tree) == size);
    ret = pddlSplayTreeMin(&tree);
    assert(ITEM(ret)->key == 1);
    ret = pddlSplayTreeMax(&tree);
    assert(ITEM(ret)->key == 9);

    // remove everything while iterating
    int count = 0;
    PDDL_SPLAYTREE_FOR_EACH_SAFE(&tree, n){
        ret = pddlSplayTreeRemove(&tree, n);
        assert(ret == n);
        ++count;
    }
    assert(count == size);
    assert(pddlSplayTreeIsEmpty(&tree));

    // the same in reverse
    insertAll(&tree, items, keys, KEYS_SIZE);
    count = 0;
    PDDL_SPLAYTREE_FOR_EACH_REVERSE_SAFE(&tree, n){
        ret = pddlSplayTreeRemove(&tree, n);
        assert(ret == n);
        ++count;
    }
    assert(count == KEYS_SIZE);
    assert(pddlSplayTreeIsEmpty(&tree));

    pddlSplayTreeFree(&tree);
}

TEST_ONCE(splaytree_extract_min)
{
    pddl_splaytree_t *tree = pddlSplayTreeNew(itemCmp, NULL);
    item_t items[KEYS_SIZE];

    insertAll(tree, items, keys, KEYS_SIZE);
    for (int i = 0; i < KEYS_SIZE; ++i){
        pddl_splaytree_node_t *n = pddlSplayTreeExtractMin(tree);
        assert(n != NULL);
        assert(ITEM(n)->key == keys_sorted[i]);
        assert(checkAscending(tree) == KEYS_SIZE - i - 1);
    }
    pddl_splaytree_node_t *n = pddlSplayTreeExtractMin(tree);
    assert(n == NULL);
    assert(pddlSplayTreeIsEmpty(tree));

    pddlSplayTreeDel(tree);
}

TEST_ONCE(splaytree_clear)
{
    pddl_splaytree_t *tree = pddlSplayTreeNew(itemCmp, NULL);
    item_t items[KEYS_SIZE];
    clear_ctx_t ctx = { 0, -1 };

    insertAll(tree, items, keys, KEYS_SIZE);
    pddlSplayTreeClear(tree, itemClear, &ctx);
    assert(ctx.count == KEYS_SIZE);
    assert(ctx.last_key == 9);
    for (int i = 0; i < KEYS_SIZE; ++i)
        assert(items[i].cleared == 1);
    assert(pddlSplayTreeIsEmpty(tree));
    assert(checkAscending(tree) == 0);

    // the tree is usable after clearing
    insertAll(tree, items, keys, KEYS_SIZE);
    assert(checkAscending(tree) == KEYS_SIZE);
    ctx.count = 0;
    ctx.last_key = -1;
    pddlSplayTreeClear(tree, itemClear, &ctx);
    assert(ctx.count == KEYS_SIZE);
    assert(pddlSplayTreeIsEmpty(tree));

    pddlSplayTreeDel(tree);
}

#define RAND_KEYS 500
#define RAND_STEPS 20000

TEST_ONCE(splaytree_random)
{
    static item_t items[RAND_KEYS];
    static int present[RAND_KEYS];
    pddl_splaytree_t tree;
    unsigned seed = 12345;
    int count = 0;

    pddlSplayTreeInit(&tree, itemCmp, NULL);
    for (int i = 0; i < RAND_KEYS; ++i){
        items[i].key = i;
        items[i].cleared = 0;
        present[i] = 0;
    }

    for (int step = 0; step < RAND_STEPS; ++step){
        int k = nextRand(&seed) % RAND_KEYS;
        int op = nextRand(&seed) % 3;
        item_t key = { .key = k };
        pddl_splaytree_node_t *ret;

        if (op == 0){
            ret = pddlSplayTreeInsert(&tree, &items[k].node);
            if (present[k]){
                assert(ret == &items[k].node);
            }else{
                assert(ret == NULL);
                present[k] = 1;
                ++count;
            }

        }else if (op == 1){
            ret = pddlSplayTreeRemove(&tree, &key.node);
            if (present[k]){
                assert(ret == &items[k].node);
                present[k] = 0;
                --count;
            }else{
                assert(ret == NULL);
            }

        }else{
            ret = pddlSplayTreeFind(&tree, &key.node);
            if (present[k]){
                assert(ret == &items[k].node);
            }else{
                assert(ret == NULL);
            }
        }

        if (step % 100 == 0)
            assert(checkAscending(&tree) == count);
    }

    // the final content must match the reference bitmap exactly
    int visited = 0;
    PDDL_SPLAYTREE_FOR_EACH(&tree, n){
        assert(present[ITEM(n)->key]);
        assert(n == &items[ITEM(n)->key].node);
        ++visited;
    }
    assert(visited == count);
    assert(checkDescending(&tree) == count);

    // drain in ascending order
    pddl_splaytree_node_t *n;
    int last = -1;
    while ((n = pddlSplayTreeExtractMin(&tree)) != NULL){
        assert(ITEM(n)->key > last);
        last = ITEM(n)->key;
        present[ITEM(n)->key] = 0;
        --count;
    }
    assert(count == 0);
    for (int i = 0; i < RAND_KEYS; ++i)
        assert(present[i] == 0);
    assert(pddlSplayTreeIsEmpty(&tree));

    pddlSplayTreeFree(&tree);
}
