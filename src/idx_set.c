/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the indexed set (pddl/idx_set.h) and of PDDL_FLEX_STRUCT_SIZE().
 */

#include "pddl/idx_set.h"
#include "pddl/rand.h"
#include "test.h"
#include <assert.h>

/* Parameters of hashDivMod(). */
struct div_mod {
    /* The key is divided by .div */
    int div;
    /* ... and then taken modulo .mod (if non-zero) */
    int mod;
};
typedef struct div_mod div_mod_t;

/* Weak hash of an int key: (key / div) % mod, so that many keys collide. */
static uint32_t hashDivMod(const void *key, void *ud)
{
    const div_mod_t *dm = ud;
    uint32_t v = (uint32_t)(*(const int *)key / dm->div);
    if (dm->mod > 0)
        v %= (uint32_t)dm->mod;
    return v;
}

/* Initializes a set of ints with the given hash parameters; DM == NULL
 * means bytewise defaults. */
static void initInts(pddl_idx_set_t *s, div_mod_t *dm, int init_buckets_log2)
{
    pddl_idx_set_config_t cfg = PDDL_IDX_SET_CONFIG_INIT;
    cfg.el_size = sizeof(int);
    cfg.first_segm_size_log2 = 2;
    cfg.exp = pddl_true;
    cfg.init_buckets_log2 = init_buckets_log2;
    if (dm != NULL){
        cfg.hash = hashDivMod;
        cfg.userdata = dm;
    }
    pddlIdxSetInit(s, &cfg);
}

/*
 * Reference model of a set of ints from [0, MODEL_RANGE): .val[id] is the
 * value with the ID id, and .id[val] is the ID of the value val or -1.
 */
#define MODEL_RANGE 60000
struct model {
    int val[MODEL_RANGE];
    int id[MODEL_RANGE];
    int size;
    /* Upper bound on the values ever added */
    int max_val;
};
typedef struct model model_t;

static void modelInit(model_t *m)
{
    m->size = 0;
    m->max_val = 0;
    for (int i = 0; i < MODEL_RANGE; ++i)
        m->id[i] = -1;
}

/* Adds V to both S and M and checks the result against M. */
static void modelAdd(model_t *m, pddl_idx_set_t *s, int v)
{
    assert(v >= 0 && v < MODEL_RANGE);
    if (v > m->max_val)
        m->max_val = v;
    // All pddlIdxSetInsert*() variants are used in turns
    int size = pddlIdxSetSize(s);
    pddl_bool_t is_new = -1;
    int id = -1;
    void *val = NULL;
    switch ((v + m->size) % 6){
        case 0:
            id = pddlIdxSetInsertFull(s, &v, &val, &is_new);
            break;
        case 1:
            id = pddlIdxSetInsertFull(s, &v, NULL, &is_new);
            break;
        case 2:
            is_new = pddlIdxSetInsert(s, &v);
            id = pddlIdxSetGetIdxOf(s, &v);
            break;
        case 3:
            id = pddlIdxSetInsertIdx(s, &v);
            is_new = (pddlIdxSetSize(s) > size);
            break;
        case 4:
            val = pddlIdxSetInsertVal(s, &v);
            is_new = (pddlIdxSetSize(s) > size);
            id = pddlIdxSetGetIdxOf(s, &v);
            break;
        case 5:
            id = pddlIdxSetInsertIdxVal(s, &v, &val);
            is_new = (pddlIdxSetSize(s) > size);
            break;
    }
    if (val != NULL)
        assert(val == pddlIdxSetAtConst(s, id));
    assert(*(const int *)pddlIdxSetAtConst(s, id) == v);
    if (m->id[v] >= 0){
        assert(!is_new);
        assert(id == m->id[v]);
    }else{
        assert(is_new);
        assert(id == m->size);
        m->id[v] = id;
        m->val[id] = v;
        ++m->size;
    }
    assert(pddlIdxSetSize(s) == m->size);
}

/* Replaces or inserts V in both S and M and checks the result against M;
 * VARIANT selects the pddlIdxSetReplace*() function. */
static void modelReplace(model_t *m, pddl_idx_set_t *s, int v, int variant)
{
    assert(v >= 0 && v < MODEL_RANGE);
    if (v > m->max_val)
        m->max_val = v;
    pddl_bool_t is_new = -1;
    int id = -1;
    int size = pddlIdxSetSize(s);
    void *val = NULL;
    switch (variant % 4){
        case 0:
            id = pddlIdxSetReplaceFull(s, &v, &val, &is_new);
            assert(val == pddlIdxSetAtConst(s, id));
            break;
        case 1:
            is_new = !pddlIdxSetReplace(s, &v);
            id = pddlIdxSetGetIdxOf(s, &v);
            break;
        case 2:
            id = pddlIdxSetReplaceIdx(s, &v);
            is_new = (m->id[v] < 0);
            break;
        case 3:
            val = pddlIdxSetReplaceVal(s, &v);
            is_new = (pddlIdxSetSize(s) > size);
            id = pddlIdxSetGetIdxOf(s, &v);
            assert(val == pddlIdxSetAtConst(s, id));
            break;
    }
    assert(*(const int *)pddlIdxSetAtConst(s, id) == v);
    if (m->id[v] >= 0){
        assert(!is_new);
        assert(id == m->id[v]);
    }else{
        assert(is_new);
        assert(id == m->size);
        m->id[v] = id;
        m->val[id] = v;
        ++m->size;
    }
    assert(pddlIdxSetSize(s) == m->size);
}

/* Swaps the IDs ID1 and ID2 in both S and M and checks the result. */
static void modelSwap(model_t *m, pddl_idx_set_t *s, int id1, int id2)
{
    pddlIdxSetSwap(s, id1, id2);
    int v1 = m->val[id1];
    int v2 = m->val[id2];
    m->val[id1] = v2;
    m->val[id2] = v1;
    m->id[v1] = id2;
    m->id[v2] = id1;
    assert(*(const int *)pddlIdxSetAtConst(s, id1) == v2);
    assert(*(const int *)pddlIdxSetAtConst(s, id2) == v1);
    assert(pddlIdxSetGetIdxOf(s, &v1) == id2);
    assert(pddlIdxSetGetIdxOf(s, &v2) == id1);
    assert(pddlIdxSetSize(s) == m->size);
}

/* Swap-removes the ID ID from both S and M and checks the result. */
static void modelSwapRemove(model_t *m, pddl_idx_set_t *s, int id)
{
    int removed = -1;
    int last = m->size - 1;
    int ret = pddlIdxSetSwapRemove(s, id, &removed);
    assert(removed == m->val[id]);
    m->id[removed] = -1;
    if (id == last){
        assert(ret == -1);
    }else{
        assert(ret == last);
        m->val[id] = m->val[last];
        m->id[m->val[id]] = id;
    }
    --m->size;
    assert(pddlIdxSetSize(s) == m->size);
}

/* Truncates both S and M to SIZE. */
static void modelTruncate(model_t *m, pddl_idx_set_t *s, int size)
{
    for (int id = size; id < m->size; ++id)
        m->id[m->val[id]] = -1;
    m->size = size;
    pddlIdxSetTruncate(s, size);
    assert(pddlIdxSetSize(s) == m->size);
}

/* Checks that S holds exactly the values of M with the same IDs. */
static void modelCheck(const model_t *m, const pddl_idx_set_t *s)
{
    assert(pddlIdxSetSize(s) == m->size);
    assert(pddlIdxSetIsEmpty(s) == (m->size == 0));
    for (int id = 0; id < m->size; ++id){
        const int *el = pddlIdxSetAtConst(s, id);
        assert(el != NULL);
        assert(*el == m->val[id]);
        assert(pddlIdxSetEq(s, &m->val[id], el));
        assert(pddlIdxSetHash(s, el) == pddlIdxSetHash(s, &m->val[id]));
    }
    assert(pddlIdxSetAtConst(s, m->size) == NULL);
    assert(pddlIdxSetAtConst(s, -1) == NULL);

    int max_val = PDDL_MIN(m->max_val + 100, MODEL_RANGE);
    for (int v = 0; v < max_val; ++v){
        void *found = (void *)1;
        int id = pddlIdxSetGetFull(s, &v, &found);
        assert(id == m->id[v]);
        assert(pddlIdxSetGetFull(s, &v, NULL) == id);
        assert(pddlIdxSetGetIdxOf(s, &v) == id);
        assert(pddlIdxSetContains(s, &v) == (id >= 0));
        assert(pddlIdxSetGetVal(s, &v) == found);
        if (id >= 0){
            assert(found == pddlIdxSetAtConst(s, id));
        }else{
            assert(found == NULL);
        }
    }

    pddl_idx_set_stat_t st;
    pddlIdxSetStat(s, &st);
    assert(st.size == (size_t)m->size);
    assert(st.el_size == sizeof(int));
    assert((st.buckets & (st.buckets - 1)) == 0);
    assert(st.buckets > 0);
    assert(st.size < 2 * st.buckets);
    assert(st.overflow_buckets <= st.buckets);
    assert(st.buckets == pddlIdxSetNumBuckets(s));
    assert(st.el_segments == (size_t)pddlSegVecNumSegments(&s->el));
    assert(pddlIdxSetAllocBytes(s) == st.el_bytes + st.htable_bytes);
}

/*
 * Bytewise set of ints: dense IDs in the order of insertion, all Insert*
 * variants, Contains, Get* and At* on stored and missing keys and invalid IDs,
 * the bytewise Hash/Eq, the iteration macros, and statistics across many
 * resizes of the table.
 */
TEST_ONCE(idx_set_ints_default)
{
    model_t *m = malloc(sizeof(*m));
    modelInit(m);
    pddl_idx_set_t s;
    initInts(&s, NULL, 1);

    // empty set: only the bucket array (2^1 buckets) is allocated
    assert(pddlIdxSetIsEmpty(&s));
    assert(pddlIdxSetAllocBytes(&s) == 2 * sizeof(uint64_t));
    int v = 1;
    assert(pddlIdxSetGetIdxOf(&s, &v) == -1);
    assert(!pddlIdxSetContains(&s, &v));
    assert(pddlIdxSetGetVal(&s, &v) == NULL);
    assert(pddlIdxSetAt(&s, 0) == NULL);

    // Hash and Eq of bytewise keys
    int w = 2;
    assert(pddlIdxSetHash(&s, &v) == pddlHash_32(&v, sizeof(int)));
    assert(pddlIdxSetEq(&s, &v, &v));
    assert(!pddlIdxSetEq(&s, &v, &w));

    for (int i = 0; i < 200000; ++i)
        modelAdd(m, &s, (int)(((long)i * 7919) % 50000));
    assert(m->size == 50000);
    modelCheck(m, &s);

    pddl_idx_set_stat_t st;
    pddlIdxSetStat(&s, &st);
    assert(st.buckets >= 32768);
    assert(st.max_bucket_size >= 2);

    int num = 0;
    PDDL_IDX_SET_FOR_EACH_ID(&s, id){
        assert(id == num);
        ++num;
    }
    assert(num == 50000);

    num = 0;
    PDDL_IDX_SET_FOR_EACH(&s, int, el){
        assert(el == pddlIdxSetAt(&s, num));
        assert(*el == m->val[num]);
        ++num;
    }
    assert(num == 50000);

    const pddl_idx_set_t *cs = &s;
    num = 0;
    PDDL_IDX_SET_FOR_EACH_CONST(cs, int, el){
        assert(el == pddlIdxSetAtConst(cs, num));
        assert(*el == m->val[num]);
        ++num;
    }
    assert(num == 50000);

    // pddlIdxSetAt() returns the same pointer as pddlIdxSetAtConst()
    assert(pddlIdxSetAt(&s, 123) == pddlIdxSetAtConst(&s, 123));

    pddlIdxSetFree(&s);
    free(m);
}

/*
 * Weak hash functions put many elements into the same bucket: the
 * overflowing buckets, promotions of slabs to bigger classes, splits that
 * convert buckets back to in-place buckets, and removals from overflowing
 * buckets work.
 */
TEST_ONCE(idx_set_collisions)
{
    // (div, mod, number of inserted values)
    static const int params[][3] = {
        { 1, 5, 20000 },          // 5 hash values -> huge buckets
        { 1 << 30, 0, 3000 },     // constant hash -> a single bucket
        { 2, 0, 20000 },          // pairs of keys -> splits to in-place
        { 3, 0, 20000 },          // triples -> overflowing buckets stay
        { 1, 64, 20000 },         // 64 hash values
    };
    model_t *m = malloc(sizeof(*m));
    for (size_t pi = 0; pi < sizeof(params) / sizeof(params[0]); ++pi){
        div_mod_t dm = { params[pi][0], params[pi][1] };
        int num = params[pi][2];
        modelInit(m);
        pddl_idx_set_t s;
        initInts(&s, &dm, 0);

        for (int i = 0; i < 2 * num; ++i)
            modelAdd(m, &s, i % num);
        modelCheck(m, &s);

        pddl_idx_set_stat_t st;
        pddlIdxSetStat(&s, &st);
        if (dm.mod > 0)
            assert(st.max_bucket_size >= (size_t)(num / dm.mod));
        if (dm.div == (1 << 30))
            assert(st.max_bucket_size == (size_t)num);
        if (dm.div == 2)
            assert(st.overflow_buckets == 0 && st.max_bucket_size == 2);
        if (dm.div == 3)
            assert(st.overflow_buckets > 0 && st.max_bucket_size == 3);

        // removals from overflowing buckets
        pddl_rand_t *rnd = pddlRandNew(1234 + pi);
        for (int i = 0; i < num / 2; ++i)
            modelSwapRemove(m, &s, pddlRandInt(rnd) % m->size);
        modelCheck(m, &s);
        for (int i = 0; i < num; ++i)
            modelAdd(m, &s, i);
        modelCheck(m, &s);
        pddlRandDel(rnd);

        pddlIdxSetFree(&s);
    }
    free(m);
}

/* Element of a set of strings: an owned copy of the string. */
struct str_el {
    char *str;
    int size;
};
typedef struct str_el str_el_t;

/* Counters of callbacks of the set of strings. */
struct str_ud {
    int copies;
    int frees;
};
typedef struct str_ud str_ud_t;

static uint32_t strHash(const void *key, void *ud)
{
    const str_el_t *k = key;
    return pddlHash_32(k->str, k->size);
}

static pddl_bool_t strEq(const void *key, const void *el, void *ud)
{
    const str_el_t *k = key;
    const str_el_t *e = el;
    return k->size == e->size && memcmp(k->str, e->str, k->size) == 0;
}

static void strInitCopy(void *el, const void *key, void *_ud)
{
    str_ud_t *ud = _ud;
    const str_el_t *k = key;
    str_el_t *e = el;
    e->size = k->size;
    e->str = malloc(k->size + 1);
    memcpy(e->str, k->str, k->size);
    e->str[k->size] = '\0';
    ++ud->copies;
}

static void strFree(void *el, void *_ud)
{
    str_ud_t *ud = _ud;
    str_el_t *e = el;
    free(e->str);
    ++ud->frees;
}

static void initStrs(pddl_idx_set_t *s, str_ud_t *ud)
{
    pddl_idx_set_config_t cfg = PDDL_IDX_SET_CONFIG_INIT;
    cfg.el_size = sizeof(str_el_t);
    cfg.exp = pddl_true;
    cfg.hash = strHash;
    cfg.eq = strEq;
    cfg.init_copy = strInitCopy;
    cfg.free = strFree;
    cfg.userdata = ud;
    memset(ud, 0, sizeof(*ud));
    pddlIdxSetInit(s, &cfg);
}

/* Adds the string "s<I>" (without the terminating zero in the key) and
 * returns its ID. */
static int addStr(pddl_idx_set_t *s, int i, pddl_bool_t *is_new)
{
    char buf[32];
    // the key borrows the buffer, which is overwritten afterwards
    snprintf(buf, sizeof(buf), "s%dXXXX", i);
    str_el_t key = { buf, (int)strlen(buf) - 4 };
    int id = pddlIdxSetInsertFull(s, &key, NULL, is_new);
    memset(buf, 'Y', sizeof(buf));
    return id;
}

/* Checks that the string with the ID ID is "s<I>". */
static void checkStr(const pddl_idx_set_t *s, int id, int i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "s%d", i);
    const str_el_t *e = pddlIdxSetAtConst(s, id);
    assert(e != NULL);
    assert(e->size == (int)strlen(buf));
    assert(strcmp(e->str, buf) == 0);
}

/*
 * Set of strings with owned copies: the key is copied by .init_copy only
 * when it is new, the stored copies do not depend on the key's buffer,
 * and .free is called on every element by Free, SwapRemove (without
 * REMOVED), and Truncate.
 */
TEST_ONCE(idx_set_strings)
{
    str_ud_t ud;
    pddl_idx_set_t s;
    initStrs(&s, &ud);

    for (int i = 0; i < 10000; ++i){
        pddl_bool_t is_new;
        int id = addStr(&s, i % 3000, &is_new);
        assert(id == i % 3000);
        assert(is_new == (i < 3000));
    }
    assert(pddlIdxSetSize(&s) == 3000);
    assert(ud.copies == 3000);
    assert(ud.frees == 0);
    for (int i = 0; i < 3000; ++i)
        checkStr(&s, i, i);

    char buf[] = "s17";
    str_el_t key = { buf, 3 };
    assert(pddlIdxSetGetIdxOf(&s, &key) == 17);
    key.size = 2; // "s1"
    assert(pddlIdxSetGetIdxOf(&s, &key) == 1);

    // Hash and Eq relay to the callbacks
    assert(pddlIdxSetHash(&s, &key) == strHash(&key, &ud));
    assert(pddlIdxSetEq(&s, &key, pddlIdxSetAtConst(&s, 1)));
    assert(!pddlIdxSetEq(&s, &key, pddlIdxSetAtConst(&s, 17)));

    key.str = "x1";
    assert(pddlIdxSetGetIdxOf(&s, &key) == -1);

    // SwapRemove without REMOVED frees the element
    int ret = pddlIdxSetSwapRemove(&s, 10, NULL);
    assert(ret == 2999);
    assert(ud.frees == 1);
    checkStr(&s, 10, 2999);

    // SwapRemove with REMOVED moves the element out
    str_el_t removed;
    ret = pddlIdxSetSwapRemove(&s, 2998, &removed);
    assert(ret == -1);
    assert(ud.frees == 1);
    assert(strcmp(removed.str, "s2998") == 0);
    free(removed.str);
    assert(pddlIdxSetSize(&s) == 2998);

    // Truncate frees the removed elements
    pddlIdxSetTruncate(&s, 2990);
    assert(ud.frees == 9);
    pddlIdxSetTruncate(&s, 1000);
    assert(ud.frees == 1999);
    for (int i = 0; i < 1000; ++i)
        checkStr(&s, i, i == 10 ? 2999 : i);

    // Free frees the rest
    pddlIdxSetFree(&s);
    assert(ud.frees == 2999);
}

/*
 * SwapRemove moves the last element to the place of the removed one,
 * returns its former ID, and hands the removed element over via REMOVED;
 * the removed keys are not found, and they can be inserted again.
 */
TEST_ONCE(idx_set_swap_remove)
{
    model_t *m = malloc(sizeof(*m));
    modelInit(m);
    pddl_idx_set_t s;
    initInts(&s, NULL, 0);
    for (int i = 0; i < 5000; ++i)
        modelAdd(m, &s, i * 3);

    pddl_rand_t *rnd = pddlRandNew(4321);
    for (int i = 0; i < 4000; ++i){
        modelSwapRemove(m, &s, pddlRandInt(rnd) % m->size);
        if (i % 500 == 0)
            modelCheck(m, &s);
    }
    modelCheck(m, &s);

    // remove the last element and the first element
    modelSwapRemove(m, &s, m->size - 1);
    modelSwapRemove(m, &s, 0);
    modelCheck(m, &s);

    // re-insert everything
    for (int i = 0; i < 5000; ++i)
        modelAdd(m, &s, i * 3);
    assert(m->size == 5000);
    modelCheck(m, &s);

    // remove everything
    while (m->size > 0)
        modelSwapRemove(m, &s, pddlRandInt(rnd) % m->size);
    modelCheck(m, &s);
    for (int i = 0; i < 100; ++i)
        modelAdd(m, &s, i);
    modelCheck(m, &s);

    pddlRandDel(rnd);
    pddlIdxSetFree(&s);
    free(m);
}

/*
 * Truncate removes all elements with too high IDs both by re-hashing a few
 * removed elements and by scanning the table when many elements are
 * removed; Clear empties the set; the set is fully usable afterwards.
 */
TEST_ONCE(idx_set_truncate)
{
    div_mod_t weak = { 1, 7 };
    model_t *m = malloc(sizeof(*m));
    for (int weak_hash = 0; weak_hash <= 1; ++weak_hash){
        modelInit(m);
        pddl_idx_set_t s;
        initInts(&s, weak_hash ? &weak : NULL, 1);
        for (int i = 0; i < 10000; ++i)
            modelAdd(m, &s, (i * 31) % 10007);

        // no-op
        modelTruncate(m, &s, 10000);
        modelCheck(m, &s);

        // a few elements -> re-hashing
        modelTruncate(m, &s, 9990);
        modelCheck(m, &s);
        modelTruncate(m, &s, 9900);
        modelCheck(m, &s);

        // many elements -> scanning
        modelTruncate(m, &s, 2000);
        modelCheck(m, &s);

        // insert again, including the removed elements
        for (int i = 0; i < 12000; ++i)
            modelAdd(m, &s, (i * 17) % 12007);
        modelCheck(m, &s);

        pddlIdxSetClear(&s);
        modelTruncate(m, &s, 0);
        modelCheck(m, &s);
        pddl_idx_set_stat_t st;
        pddlIdxSetStat(&s, &st);
        assert(st.overflow_buckets == 0);
        assert(st.max_bucket_size == 0);

        for (int i = 0; i < 5000; ++i)
            modelAdd(m, &s, i);
        modelCheck(m, &s);

        pddlIdxSetFree(&s);
    }
    free(m);
}

/*
 * Random sequences of Insert*, Replace*, Get*, Swap, SwapRemove, Truncate
 * and Clear agree with the reference model, both with the default and with
 * a weak hash.
 */
TEST_ONCE(idx_set_random_model)
{
    div_mod_t weak = { 1, 64 };
    model_t *m = malloc(sizeof(*m));
    for (int weak_hash = 0; weak_hash <= 1; ++weak_hash){
        modelInit(m);
        pddl_idx_set_t s;
        initInts(&s, weak_hash ? &weak : NULL, 0);
        pddl_rand_t *rnd = pddlRandNew(77 + weak_hash);
        for (int step = 0; step < 300000; ++step){
            uint32_t op = pddlRandInt(rnd) % 1000;
            int v = pddlRandInt(rnd) % 4000;
            if (op < 500){
                modelAdd(m, &s, v);
            }else if (op < 600){
                modelReplace(m, &s, v, (int)op);
            }else if (op < 700){
                assert(pddlIdxSetGetIdxOf(&s, &v) == m->id[v]);
            }else if (op < 800){
                if (m->size > 0){
                    modelSwap(m, &s, pddlRandInt(rnd) % m->size,
                              pddlRandInt(rnd) % m->size);
                }
            }else if (op < 990){
                if (m->size > 0)
                    modelSwapRemove(m, &s, pddlRandInt(rnd) % m->size);
            }else if (op < 999){
                modelTruncate(m, &s, pddlRandInt(rnd) % (m->size + 1));
            }else{
                pddlIdxSetClear(&s);
                modelTruncate(m, &s, 0);
            }
            if (step % 20000 == 0)
                modelCheck(m, &s);
        }
        modelCheck(m, &s);
        pddlRandDel(rnd);
        pddlIdxSetFree(&s);
    }
    free(m);
}

/*
 * pddlIdxSetInitSimpleExp() creates a bytewise set of structs; the
 * statistics of an empty set and of a set with a single element.
 */
TEST_ONCE(idx_set_simple_exp)
{
    struct pair {
        int a;
        int b;
    };
    pddl_idx_set_t s;
    pddlIdxSetInitSimpleExp(&s, sizeof(struct pair), 0);

    pddl_idx_set_stat_t st;
    pddlIdxSetStat(&s, &st);
    assert(st.size == 0);
    assert(st.el_size == sizeof(struct pair));
    assert(st.buckets == 8);
    assert(pddlIdxSetNumBuckets(&s) == 8);
    assert(st.el_bytes == 0);
    assert(st.el_segments == 0);
    assert(st.htable_bytes == 8 * sizeof(uint64_t));

    struct pair p = { 1, 2 };
    int id = pddlIdxSetInsertIdx(&s, &p);
    assert(id == 0);
    p.b = 3;
    id = pddlIdxSetInsertIdx(&s, &p);
    assert(id == 1);
    p.b = 2;
    id = pddlIdxSetInsertIdx(&s, &p);
    assert(id == 0);
    assert(pddlIdxSetSize(&s) == 2);
    const struct pair *q = pddlIdxSetAtConst(&s, 1);
    assert(q->a == 1 && q->b == 3);

    pddlIdxSetStat(&s, &st);
    assert(st.size == 2);
    assert(st.buckets == 8);
    assert(st.el_segments == 1);
    assert(st.htable_bytes == 8 * sizeof(uint64_t));
    assert(st.el_bytes >= (1u << PDDL_IDX_SET_DEFAULT_FIRST_SEGM_SIZE_LOG2)
                                * sizeof(struct pair));
    pddlIdxSetFree(&s);
}

/*
 * The log2 sizes of the configuration: values <= 0 mean the defaults, and
 * positive values are taken as they are.
 */
TEST_ONCE(idx_set_config_log2)
{
    static const int vals[] = { 0, -1, 1, 5 };
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); ++i){
        pddl_idx_set_config_t cfg = PDDL_IDX_SET_CONFIG_INIT;
        cfg.el_size = sizeof(int);
        cfg.first_segm_size_log2 = vals[i];
        cfg.init_buckets_log2 = vals[i];
        pddl_idx_set_t s;
        pddlIdxSetInit(&s, &cfg);

        int log2 = vals[i];
        int segm_log2 = vals[i];
        if (log2 <= 0){
            log2 = PDDL_IDX_SET_DEFAULT_INIT_BUCKETS_LOG2;
            segm_log2 = PDDL_IDX_SET_DEFAULT_FIRST_SEGM_SIZE_LOG2;
        }
        pddl_idx_set_stat_t st;
        pddlIdxSetStat(&s, &st);
        assert(st.buckets == (1u << log2));
        assert(st.htable_bytes == (1u << log2) * sizeof(uint64_t));

        int v = 7;
        int id = pddlIdxSetInsertIdx(&s, &v);
        assert(id == 0);
        assert(pddlSegVecCapacity(&s.el) == (1u << segm_log2));
        pddlIdxSetFree(&s);
    }
}

/* Element with a key and a payload that is not a part of the key. */
struct kv {
    int key;
    int payload;
};
typedef struct kv kv_t;

static uint32_t kvHash(const void *key, void *ud)
{
    const kv_t *k = key;
    return pddlHash_32(&k->key, sizeof(int));
}

static pddl_bool_t kvEq(const void *key, const void *el, void *ud)
{
    const kv_t *k = key;
    const kv_t *e = el;
    return k->key == e->key;
}

/* Returns the payload of the element with the ID ID. */
static int kvPayload(const pddl_idx_set_t *s, int id)
{
    const kv_t *e = pddlIdxSetAtConst(s, id);
    assert(e != NULL);
    return e->payload;
}

/*
 * Replace* replaces a stored equal element in place (the ID is kept and
 * the payload is updated, unlike with Insert*) and inserts a missing key;
 * the return values and out-parameters of all variants.
 */
TEST_ONCE(idx_set_replace)
{
    pddl_idx_set_config_t cfg = PDDL_IDX_SET_CONFIG_INIT;
    cfg.el_size = sizeof(kv_t);
    cfg.hash = kvHash;
    cfg.eq = kvEq;
    pddl_idx_set_t s;
    pddlIdxSetInit(&s, &cfg);

    for (int i = 0; i < 100; ++i){
        kv_t kv = { i, i };
        int id = pddlIdxSetInsertIdx(&s, &kv);
        assert(id == i);
    }

    // Insert keeps the stored element
    kv_t kv = { 5, 1000 };
    void *val = NULL;
    pddl_bool_t is_new = pddl_true;
    int id = pddlIdxSetInsertFull(&s, &kv, &val, &is_new);
    assert(id == 5);
    assert(!is_new);
    assert(val == pddlIdxSetAt(&s, 5));
    assert(kvPayload(&s, 5) == 5);

    // Replace of stored keys
    kv.payload = 1001;
    pddl_bool_t replaced = pddlIdxSetReplace(&s, &kv);
    assert(replaced);
    assert(kvPayload(&s, 5) == 1001);

    kv.key = 7;
    kv.payload = 1002;
    id = pddlIdxSetReplaceIdx(&s, &kv);
    assert(id == 7);
    assert(kvPayload(&s, 7) == 1002);

    kv.key = 9;
    kv.payload = 1003;
    val = NULL;
    is_new = pddl_true;
    id = pddlIdxSetReplaceFull(&s, &kv, &val, &is_new);
    assert(id == 9);
    assert(!is_new);
    assert(val == pddlIdxSetAt(&s, 9));
    assert(kvPayload(&s, 9) == 1003);
    kv.payload = 1004;
    id = pddlIdxSetReplaceFull(&s, &kv, NULL, NULL);
    assert(id == 9);
    assert(kvPayload(&s, 9) == 1004);

    kv.key = 11;
    kv.payload = 1005;
    val = pddlIdxSetReplaceVal(&s, &kv);
    assert(val == pddlIdxSetAt(&s, 11));
    assert(kvPayload(&s, 11) == 1005);
    assert(pddlIdxSetSize(&s) == 100);

    // Replace of missing keys
    kv.key = 100;
    kv.payload = 2000;
    replaced = pddlIdxSetReplace(&s, &kv);
    assert(!replaced);
    assert(kvPayload(&s, 100) == 2000);

    kv.key = 101;
    id = pddlIdxSetReplaceIdx(&s, &kv);
    assert(id == 101);

    kv.key = 102;
    val = NULL;
    is_new = pddl_false;
    id = pddlIdxSetReplaceFull(&s, &kv, &val, &is_new);
    assert(id == 102);
    assert(is_new);
    assert(val == pddlIdxSetAt(&s, 102));

    kv.key = 103;
    val = pddlIdxSetReplaceVal(&s, &kv);
    assert(val == pddlIdxSetAt(&s, 103));
    assert(kvPayload(&s, 103) == 2000);
    assert(pddlIdxSetSize(&s) == 104);

    // The hash table is intact
    for (int i = 0; i < 104; ++i){
        kv.key = i;
        assert(pddlIdxSetGetIdxOf(&s, &kv) == i);
    }
    pddlIdxSetFree(&s);
}

/*
 * Replace* of elements with owned parts: the new element is constructed
 * by .init_copy and the replaced one is freed by .free, also when the key
 * borrows the string of the replaced element; inserting a missing key
 * frees nothing.
 */
TEST_ONCE(idx_set_replace_owned)
{
    str_ud_t ud;
    pddl_idx_set_t s;
    initStrs(&s, &ud);
    for (int i = 0; i < 100; ++i){
        pddl_bool_t is_new;
        int id = addStr(&s, i, &is_new);
        assert(id == i);
    }
    assert(ud.copies == 100 && ud.frees == 0);

    // The key borrows the string of the replaced element
    const str_el_t *e = pddlIdxSetAtConst(&s, 42);
    str_el_t key = { e->str, e->size };
    pddl_bool_t replaced = pddlIdxSetReplace(&s, &key);
    assert(replaced);
    assert(ud.copies == 101 && ud.frees == 1);
    checkStr(&s, 42, 42);

    // ReplaceVal returns the new stored element
    char buf[] = "s43";
    key.str = buf;
    key.size = 3;
    const str_el_t *val = pddlIdxSetReplaceVal(&s, &key);
    assert(val == pddlIdxSetAtConst(&s, 43));
    assert(val->str != buf);
    assert(ud.copies == 102 && ud.frees == 2);
    checkStr(&s, 43, 43);

    // A missing key is inserted
    char buf2[] = "s100";
    key.str = buf2;
    key.size = 4;
    int id = pddlIdxSetReplaceIdx(&s, &key);
    assert(id == 100);
    assert(ud.copies == 103 && ud.frees == 2);
    checkStr(&s, 100, 100);

    pddlIdxSetFree(&s);
    assert(ud.frees == 103);
}

/* Element bigger than the stack buffers of Replace and Swap. */
#define BIG_EL_DATA_SIZE 300
struct big_el {
    int key;
    int payload;
    char data[BIG_EL_DATA_SIZE];
};
typedef struct big_el big_el_t;

static uint32_t bigHash(const void *key, void *ud)
{
    const big_el_t *k = key;
    return pddlHash_32(&k->key, sizeof(int));
}

static pddl_bool_t bigEq(const void *key, const void *el, void *ud)
{
    const big_el_t *k = key;
    const big_el_t *e = el;
    return k->key == e->key;
}

static void bigFree(void *el, void *ud)
{
    ++*(int *)ud;
}

/* Fills B with KEY, PAYLOAD and the data derived from them. */
static void bigSet(big_el_t *b, int key, int payload)
{
    b->key = key;
    b->payload = payload;
    for (int i = 0; i < BIG_EL_DATA_SIZE; ++i)
        b->data[i] = (char)(key + 3 * payload + i);
}

/* Checks that B was filled by bigSet(B, KEY, PAYLOAD). */
static void bigCheck(const big_el_t *b, int key, int payload)
{
    assert(b != NULL);
    assert(b->key == key);
    assert(b->payload == payload);
    for (int i = 0; i < BIG_EL_DATA_SIZE; ++i)
        assert(b->data[i] == (char)(key + 3 * payload + i));
}

/*
 * Replace and Swap of elements bigger than the stack buffers used by them
 * keep all bytes of the elements and call .free only on the replaced
 * elements.
 */
TEST_ONCE(idx_set_big_elements)
{
    int frees = 0;
    pddl_idx_set_config_t cfg = PDDL_IDX_SET_CONFIG_INIT;
    cfg.el_size = sizeof(big_el_t);
    cfg.hash = bigHash;
    cfg.eq = bigEq;
    cfg.free = bigFree;
    cfg.userdata = &frees;
    pddl_idx_set_t s;
    pddlIdxSetInit(&s, &cfg);

    big_el_t b;
    for (int i = 0; i < 50; ++i){
        bigSet(&b, i, 0);
        int id = pddlIdxSetInsertIdx(&s, &b);
        assert(id == i);
    }

    bigSet(&b, 7, 1);
    pddl_bool_t replaced = pddlIdxSetReplace(&s, &b);
    assert(replaced);
    assert(frees == 1);
    bigCheck(pddlIdxSetAtConst(&s, 7), 7, 1);

    bigSet(&b, 8, 2);
    const big_el_t *val = pddlIdxSetReplaceVal(&s, &b);
    assert(val == pddlIdxSetAtConst(&s, 8));
    assert(frees == 2);
    bigCheck(val, 8, 2);

    pddlIdxSetSwap(&s, 3, 40);
    bigCheck(pddlIdxSetAtConst(&s, 3), 40, 0);
    bigCheck(pddlIdxSetAtConst(&s, 40), 3, 0);
    b.key = 3;
    assert(pddlIdxSetGetIdxOf(&s, &b) == 40);
    b.key = 40;
    assert(pddlIdxSetGetIdxOf(&s, &b) == 3);
    assert(frees == 2);

    pddlIdxSetFree(&s);
    assert(frees == 52);
}

/*
 * Swap exchanges two elements and their IDs in the hash table with the
 * default hash, with pairs of keys sharing in-place buckets, with a single
 * overflowing bucket, and with a few overflowing buckets; swapping an
 * element with itself does nothing; SwapRemove, Truncate and Insert work
 * afterwards.
 */
TEST_ONCE(idx_set_swap)
{
    div_mod_t dms[] = { { 2, 0 }, { 1 << 30, 0 }, { 1, 7 } };
    model_t *m = malloc(sizeof(*m));
    for (int di = -1; di < 3; ++di){
        modelInit(m);
        pddl_idx_set_t s;
        initInts(&s, di < 0 ? NULL : &dms[di], 0);
        int num = 2000;
        for (int i = 0; i < num; ++i)
            modelAdd(m, &s, i);

        modelSwap(m, &s, 5, 5);
        modelCheck(m, &s);

        // Neighbors share in-place buckets with the weak hash { 2, 0 }
        for (int i = 0; i + 1 < num; i += 2)
            modelSwap(m, &s, i, i + 1);
        modelCheck(m, &s);

        modelSwap(m, &s, 0, m->size - 1);
        modelCheck(m, &s);

        pddl_rand_t *rnd = pddlRandNew(99 + di);
        for (int i = 0; i < 5000; ++i){
            modelSwap(m, &s, pddlRandInt(rnd) % m->size,
                      pddlRandInt(rnd) % m->size);
        }
        modelCheck(m, &s);

        for (int i = 0; i < 500; ++i)
            modelSwapRemove(m, &s, pddlRandInt(rnd) % m->size);
        modelCheck(m, &s);
        modelTruncate(m, &s, m->size / 2);
        modelCheck(m, &s);
        for (int i = 0; i < num; ++i)
            modelAdd(m, &s, i);
        modelCheck(m, &s);

        pddlRandDel(rnd);
        pddlIdxSetFree(&s);
    }
    free(m);
}

/* Swapping with an ID equal to the size of the set panics. */
TEST_PANIC_ONCE(idx_set_panic_swap_invalid_id)
{
    pddl_idx_set_t s;
    pddlIdxSetInitSimpleExp(&s, sizeof(int), 0);
    int v = 5;
    pddlIdxSetInsertIdx(&s, &v);
    pddlIdxSetSwap(&s, 0, 1);
}

/* Swapping with a negative ID panics. */
TEST_PANIC_ONCE(idx_set_panic_swap_negative_id)
{
    pddl_idx_set_t s;
    pddlIdxSetInitSimpleExp(&s, sizeof(int), 0);
    int v = 5;
    pddlIdxSetInsertIdx(&s, &v);
    pddlIdxSetSwap(&s, -1, 0);
}

/* Removing an invalid ID panics. */
TEST_PANIC_ONCE(idx_set_panic_swap_remove_invalid_id)
{
    pddl_idx_set_t s;
    pddlIdxSetInitSimpleExp(&s, sizeof(int), 0);
    int v = 5;
    pddlIdxSetInsertIdx(&s, &v);
    (void)pddlIdxSetSwapRemove(&s, 1, NULL);
}

/* Truncating to a size greater than the size of the set panics. */
TEST_PANIC_ONCE(idx_set_panic_truncate_too_big)
{
    pddl_idx_set_t s;
    pddlIdxSetInitSimpleExp(&s, sizeof(int), 0);
    int v = 5;
    pddlIdxSetInsertIdx(&s, &v);
    pddlIdxSetTruncate(&s, 2);
}

/* Structs with flexible array members for flex_struct_size. */
struct flex_int {
    int a;
    char b;
    int arr[];
};
struct flex_dbl {
    double d;
    int n;
    int arr[];
};
struct flex_char {
    char c;
    char arr[];
};

/* Checks the general properties of PDDL_FLEX_STRUCT_SIZE() of TYPE with
 * the flexible array member MEMBER and N elements. */
#define CHECK_FLEX(TYPE, MEMBER, N) \
    do { \
        size_t sz = PDDL_FLEX_STRUCT_SIZE(TYPE, MEMBER, (N)); \
        size_t min = offsetof(TYPE, MEMBER) \
                        + sizeof(((TYPE *)0)->MEMBER[0]) * (N); \
        assert(sz >= sizeof(TYPE)); \
        assert(sz >= min); \
        assert(sz < min + PDDL_ALIGNOF(TYPE)); \
        assert(sz % PDDL_ALIGNOF(TYPE) == 0); \
    } while (0)

/*
 * PDDL_ALIGN_UP() rounds up to a multiple, and PDDL_FLEX_STRUCT_SIZE() is
 * the smallest multiple of the alignment of the struct that holds the
 * flexible array member with the given number of elements.
 */
TEST_ONCE(flex_struct_size)
{
    assert(PDDL_ALIGN_UP(0, 8) == 0);
    assert(PDDL_ALIGN_UP(1, 8) == 8);
    assert(PDDL_ALIGN_UP(8, 8) == 8);
    assert(PDDL_ALIGN_UP(9, 4) == 12);
    assert(PDDL_ALIGN_UP(5, 1) == 5);

    for (int n = 0; n < 20; ++n){
        CHECK_FLEX(struct flex_int, arr, n);
        CHECK_FLEX(struct flex_dbl, arr, n);
        CHECK_FLEX(struct flex_char, arr, n);
    }

    assert(PDDL_FLEX_STRUCT_SIZE(struct flex_int, arr, 0) == 8);
    assert(PDDL_FLEX_STRUCT_SIZE(struct flex_int, arr, 3) == 20);
    assert(PDDL_FLEX_STRUCT_SIZE(struct flex_char, arr, 0) == 1);
    assert(PDDL_FLEX_STRUCT_SIZE(struct flex_char, arr, 5) == 6);
    if (PDDL_ALIGNOF(double) == 8){
        // the trailing padding of struct flex_dbl is used by the array
        assert(sizeof(struct flex_dbl) == 16);
        assert(PDDL_FLEX_STRUCT_SIZE(struct flex_dbl, arr, 0) == 16);
        assert(PDDL_FLEX_STRUCT_SIZE(struct flex_dbl, arr, 1) == 16);
        assert(PDDL_FLEX_STRUCT_SIZE(struct flex_dbl, arr, 2) == 24);
    }
}
