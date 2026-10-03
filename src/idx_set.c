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
    pddl_bool_t is_new = -1;
    int id = pddlIdxSetAdd(s, &v, &is_new);
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
        const int *el = pddlIdxSetGetConst(s, id);
        assert(el != NULL);
        assert(*el == m->val[id]);
    }
    assert(pddlIdxSetGetConst(s, m->size) == NULL);
    assert(pddlIdxSetGetConst(s, -1) == NULL);

    int max_val = PDDL_MIN(m->max_val + 100, MODEL_RANGE);
    for (int v = 0; v < max_val; ++v){
        void *found = NULL;
        int id = pddlIdxSetFindFull(s, &v, &found);
        assert(id == m->id[v]);
        if (id >= 0){
            assert(found == pddlIdxSetGetConst(s, id));
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
    assert(pddlIdxSetAllocBytes(s) == st.el_bytes + st.htable_bytes);
}

/*
 * Bytewise set of ints: dense IDs in the order of insertion, is_new,
 * Find/FindFull/Get on stored and missing keys and invalid IDs, the
 * iteration macros, and statistics across many resizes of the table.
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
    assert(pddlIdxSetFind(&s, &v) == -1);
    assert(pddlIdxSetGet(&s, 0) == NULL);

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
        assert(el == pddlIdxSetGet(&s, num));
        assert(*el == m->val[num]);
        ++num;
    }
    assert(num == 50000);

    const pddl_idx_set_t *cs = &s;
    num = 0;
    PDDL_IDX_SET_FOR_EACH_CONST(cs, int, el){
        assert(el == pddlIdxSetGetConst(cs, num));
        assert(*el == m->val[num]);
        ++num;
    }
    assert(num == 50000);

    // pddlIdxSetGet() returns the same pointer as pddlIdxSetGetConst()
    assert(pddlIdxSetGet(&s, 123) == pddlIdxSetGetConst(&s, 123));

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
    int id = pddlIdxSetAdd(s, &key, is_new);
    memset(buf, 'Y', sizeof(buf));
    return id;
}

/* Checks that the string with the ID ID is "s<I>". */
static void checkStr(const pddl_idx_set_t *s, int id, int i)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "s%d", i);
    const str_el_t *e = pddlIdxSetGetConst(s, id);
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
    assert(pddlIdxSetFind(&s, &key) == 17);
    key.size = 2; // "s1"
    assert(pddlIdxSetFind(&s, &key) == 1);
    key.str = "x1";
    assert(pddlIdxSetFind(&s, &key) == -1);

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
 * Random sequences of Add, Find, SwapRemove, Truncate and Clear agree with
 * the reference model, both with the default and with a weak hash.
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
            if (op < 600){
                modelAdd(m, &s, v);
            }else if (op < 800){
                assert(pddlIdxSetFind(&s, &v) == m->id[v]);
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
    assert(st.el_bytes == 0);
    assert(st.htable_bytes == 8 * sizeof(uint64_t));

    struct pair p = { 1, 2 };
    assert(pddlIdxSetAdd(&s, &p, NULL) == 0);
    p.b = 3;
    assert(pddlIdxSetAdd(&s, &p, NULL) == 1);
    p.b = 2;
    assert(pddlIdxSetAdd(&s, &p, NULL) == 0);
    assert(pddlIdxSetSize(&s) == 2);
    const struct pair *q = pddlIdxSetGetConst(&s, 1);
    assert(q->a == 1 && q->b == 3);

    pddlIdxSetStat(&s, &st);
    assert(st.size == 2);
    assert(st.buckets == 8);
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
        assert(pddlIdxSetAdd(&s, &v, NULL) == 0);
        assert(pddlSegVecCapacity(&s.el) == (1u << segm_log2));
        pddlIdxSetFree(&s);
    }
}

/* Removing an invalid ID panics. */
TEST_PANIC_ONCE(idx_set_panic_swap_remove_invalid_id)
{
    pddl_idx_set_t s;
    pddlIdxSetInitSimpleExp(&s, sizeof(int), 0);
    int v = 5;
    (void)pddlIdxSetAdd(&s, &v, NULL);
    (void)pddlIdxSetSwapRemove(&s, 1, NULL);
}

/* Truncating to a size greater than the size of the set panics. */
TEST_PANIC_ONCE(idx_set_panic_truncate_too_big)
{
    pddl_idx_set_t s;
    pddlIdxSetInitSimpleExp(&s, sizeof(int), 0);
    int v = 5;
    (void)pddlIdxSetAdd(&s, &v, NULL);
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
