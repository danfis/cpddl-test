/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the slab allocator (pddl/slab.h).
 */

#include "pddl/slab.h"
#include "pddl/rand.h"
#include "test.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Configuration of an allocator every layout test is run with. */
struct cfg {
    /* Size of a slab of the class 0 */
    size_t slab_bytes;
    /* Maximum size of the first segment of an arena (0 = default) */
    size_t first_segm_bytes;
};
typedef struct cfg cfg_t;

/* The last configuration forces a single slab in every first segment. */
static const cfg_t cfgs[] = {
    { 4, 0 },
    { 8, 256 },
    { 12, 64 },
    { 300, 256 },
};
#define NUM_CFGS ((int)(sizeof(cfgs) / sizeof(cfgs[0])))

/* Number of classes used by the layout tests. */
#define NUM_CLASSES 4

/* Expected number of slabs in the first segment of an arena with slabs of
 * SLAB_BYTES bytes. */
static int expFirstSegmSize(size_t slab_bytes, size_t first_segm_bytes)
{
    if (first_segm_bytes == 0)
        first_segm_bytes = PDDL_SLAB_DEFAULT_FIRST_SEGM_BYTES;
    int n = 1;
    while (2 * (size_t)n * slab_bytes <= first_segm_bytes)
        n *= 2;
    return n;
}


/* ID of the slab with the index IDX of the class CLS. */
static uint32_t mkId(int cls, uint32_t idx)
{
    return (((uint32_t)cls) << PDDL_SLAB_ID_INDEX_BITS) | idx;
}

/* Byte I of a slab stamped with SEED. */
static unsigned char stampByte(unsigned seed, size_t i)
{
    return (unsigned char)((seed * 131u + i * 7u + 1u) & 0xffu);
}

/* Fills the slab ID with a pattern given by SEED. */
static void stamp(pddl_slab_t *s, uint32_t id, unsigned seed)
{
    unsigned char *b = pddlSlabAt(s, id);
    size_t size = pddlSlabBytes(s, id);
    for (size_t i = 0; i < size; ++i)
        b[i] = stampByte(seed, i);
}

/* Checks the pattern written by stamp(). */
static void checkStamp(const pddl_slab_t *s, uint32_t id, unsigned seed)
{
    const unsigned char *b = pddlSlabAt(s, id);
    size_t size = pddlSlabBytes(s, id);
    for (size_t i = 0; i < size; ++i)
        assert(b[i] == stampByte(seed, i));
}

/* Checks that the slab ID is zeroed. */
static void checkZero(const pddl_slab_t *s, uint32_t id)
{
    const unsigned char *b = pddlSlabAt(s, id);
    size_t size = pddlSlabBytes(s, id);
    for (size_t i = 0; i < size; ++i)
        assert(b[i] == 0);
}

/* Seed of the slab ID used by the layout tests. */
static unsigned seedOf(uint32_t id)
{
    return 1000u * (unsigned)pddlSlabIdClass(id) + pddlSlabIdIndex(id);
}

/* Allocates a slab of the class CLS, checks that it has the index IDX and
 * that it is zeroed, and returns its ID. */
static uint32_t allocCheck(pddl_slab_t *s, int cls, uint32_t idx)
{
    uint32_t id = pddlSlabAlloc(s, cls);
    assert(id == mkId(cls, idx));
    assert(pddlSlabIdClass(id) == cls);
    assert(pddlSlabIdIndex(id) == idx);
    checkZero(s, id);
    return id;
}

/* Expected value of pddlSlabAllocBytes() computed from the arenas. */
static size_t expAllocBytes(const pddl_slab_t *s)
{
    size_t bytes = sizeof(pddl_slab_arena_t) * (size_t)s->num_classes;
    for (int cls = 0; cls < s->num_classes; ++cls)
        bytes += pddlSegVecAllocBytes(&s->arena[cls].slab);
    return bytes;
}

/*
 * IDs encode the class in the top 5 bits and the index within the class in
 * the lower 27 bits; allocated slabs get IDs of their class, and the size
 * of a slab is derived from the class of its ID.
 */
TEST_ONCE(slab_id_encoding)
{
    assert(PDDL_SLAB_ID_CLASS_BITS + PDDL_SLAB_ID_INDEX_BITS == 32);
    assert((1 << PDDL_SLAB_ID_CLASS_BITS) == PDDL_SLAB_MAX_CLASSES);
    assert(PDDL_SLAB_ID_INDEX_MASK == 0x07ffffffu);
    assert(PDDL_SLAB_MAX_CLASS_SLABS == 0x08000000u);

    assert(pddlSlabIdClass(0) == 0);
    assert(pddlSlabIdIndex(0) == 0);
    assert(pddlSlabIdClass(0xffffffffu) == 31);
    assert(pddlSlabIdIndex(0xffffffffu) == PDDL_SLAB_ID_INDEX_MASK);
    assert(pddlSlabIdClass((5u << 27) | 123u) == 5);
    assert(pddlSlabIdIndex((5u << 27) | 123u) == 123);
    assert(pddlSlabIdClass(PDDL_SLAB_ID_INDEX_MASK) == 0);
    assert(pddlSlabIdIndex(PDDL_SLAB_ID_INDEX_MASK)
            == PDDL_SLAB_ID_INDEX_MASK);
    assert(pddlSlabIdClass(1u << 27) == 1);
    assert(pddlSlabIdIndex(1u << 27) == 0);

    const int num_classes = 8;
    pddl_slab_t s;
    pddlSlabInit(&s, 12, num_classes, pddl_true, 0);
    void *ptr[8][5];
    for (int cls = 0; cls < num_classes; ++cls){
        for (uint32_t i = 0; i < 5; ++i){
            uint32_t id = allocCheck(&s, cls, i);
            assert(id == (((uint32_t)cls << 27) | i));
            assert(pddlSlabBytes(&s, id) == ((size_t)12 << cls));
            assert(pddlSlabBytes(&s, id) == pddlSlabClassBytes(&s, cls));
            ptr[cls][i] = pddlSlabAt(&s, id);
        }
    }
    // Slabs with the same index in different classes are different slabs
    for (int c1 = 0; c1 < num_classes; ++c1){
        for (int c2 = c1 + 1; c2 < num_classes; ++c2){
            for (uint32_t i = 0; i < 5; ++i)
                assert(ptr[c1][i] != ptr[c2][i]);
        }
    }
    pddlSlabFree(&s);
}

/*
 * A freshly initialized allocator initializes all arenas, but allocates
 * only the array of arenas (empty segmented vectors allocate nothing), and
 * it reports the configured number of classes and sizes of slabs; Reset and
 * Free work on an untouched allocator.
 */
TEST_ONCE(slab_init_empty)
{
    for (int ci = 0; ci < NUM_CFGS; ++ci){
        for (int exp = 0; exp <= 1; ++exp){
            pddl_slab_t s;
            pddlSlabInit(&s, cfgs[ci].slab_bytes, NUM_CLASSES, exp,
                         cfgs[ci].first_segm_bytes);
            // Only the array of arenas is allocated
            assert(s.arena != NULL);
            assert(s.exp == exp);
            assert(pddlSlabNumClasses(&s) == NUM_CLASSES);
            assert(pddlSlabAllocBytes(&s)
                    == sizeof(pddl_slab_arena_t) * NUM_CLASSES);
            for (int cls = 0; cls < NUM_CLASSES; ++cls){
                const pddl_segvec_t *sv = &s.arena[cls].slab;
                assert(pddlSegVecElSize(sv) == pddlSlabClassBytes(&s, cls));
                assert(sv->exp == exp);
                assert(pddlSegVecSegmentSize(sv, 0)
                        == expFirstSegmSize(pddlSlabClassBytes(&s, cls),
                                            cfgs[ci].first_segm_bytes));
                assert(pddlSegVecNumSegments(sv) == 0);
                assert(pddlSegVecAllocBytes(sv) == 0);
                assert(s.arena[cls].free_head == 0);
                assert(pddlSlabClassBytes(&s, cls)
                        == cfgs[ci].slab_bytes << cls);
                assert(pddlSlabNumSlabs(&s, cls) == 0);
            }
            if (cfgs[ci].first_segm_bytes == 0){
                assert(s.first_segm_bytes
                        == PDDL_SLAB_DEFAULT_FIRST_SEGM_BYTES);
            }else{
                assert(s.first_segm_bytes == cfgs[ci].first_segm_bytes);
            }

            pddlSlabReset(&s);
            assert(pddlSlabAllocBytes(&s)
                    == sizeof(pddl_slab_arena_t) * NUM_CLASSES);
            pddlSlabFree(&s);
        }
    }

    // The extreme numbers of classes are accepted
    pddl_slab_t s;
    pddlSlabInit(&s, sizeof(uint32_t), 1, pddl_false, 0);
    assert(pddlSlabNumClasses(&s) == 1);
    pddlSlabFree(&s);
    pddlSlabInit(&s, sizeof(uint32_t), PDDL_SLAB_MAX_CLASSES, pddl_true, 0);
    assert(pddlSlabNumClasses(&s) == PDDL_SLAB_MAX_CLASSES);
    assert(pddlSlabClassBytes(&s, PDDL_SLAB_MAX_CLASSES - 1)
            == sizeof(uint32_t) << (PDDL_SLAB_MAX_CLASSES - 1));
    pddlSlabFree(&s);
}

/*
 * New slabs get consecutive indexes per class, they are zeroed, and they do
 * not overlap (a pattern written into every slab of every class survives);
 * only the arenas of the used classes allocate memory.
 */
TEST_ONCE(slab_zeroed_disjoint)
{
    const uint32_t n = 100;
    for (int ci = 0; ci < NUM_CFGS; ++ci){
        for (int exp = 0; exp <= 1; ++exp){
            pddl_slab_t s;
            pddlSlabInit(&s, cfgs[ci].slab_bytes, NUM_CLASSES, exp,
                         cfgs[ci].first_segm_bytes);

            // Only the class 0 is used so far
            uint32_t id = allocCheck(&s, 0, 0);
            assert(pddlSegVecAllocBytes(&s.arena[0].slab) > 0);
            for (int cls = 1; cls < NUM_CLASSES; ++cls){
                assert(pddlSegVecAllocBytes(&s.arena[cls].slab) == 0);
                assert(pddlSlabNumSlabs(&s, cls) == 0);
            }
            assert(pddlSlabAllocBytes(&s) == expAllocBytes(&s));
            stamp(&s, id, seedOf(id));

            // The classes are interleaved
            for (uint32_t i = 1; i < n; ++i){
                for (int cls = 0; cls < NUM_CLASSES; cls += 2){
                    uint32_t j = (cls == 0 ? i : i - 1);
                    id = allocCheck(&s, cls, j);
                    assert(pddlSlabNumSlabs(&s, cls) == j + 1);
                    stamp(&s, id, seedOf(id));
                }
            }
            id = allocCheck(&s, 2, n - 1);
            stamp(&s, id, seedOf(id));

            for (int cls = 0; cls < NUM_CLASSES; ++cls){
                if (cls % 2 == 1){
                    assert(pddlSegVecAllocBytes(&s.arena[cls].slab) == 0);
                    assert(pddlSlabNumSlabs(&s, cls) == 0);
                    continue;
                }
                assert(pddlSlabNumSlabs(&s, cls) == n);
                for (uint32_t i = 0; i < n; ++i)
                    checkStamp(&s, mkId(cls, i), seedOf(mkId(cls, i)));
            }
            assert(pddlSlabAllocBytes(&s) == expAllocBytes(&s));
            assert(pddlSlabAllocBytes(&s)
                    >= n * (pddlSlabClassBytes(&s, 0)
                                + pddlSlabClassBytes(&s, 2)));
            pddlSlabFree(&s);
        }
    }
}

/*
 * The first segment of each arena holds the largest power of 2 number of
 * slabs fitting in the configured number of bytes (at least one); the
 * linear layout uses segments of the same size, the exponential layout
 * doubles them.
 */
TEST_ONCE(slab_segments)
{
    const uint32_t n = 50;
    for (int ci = 0; ci < NUM_CFGS; ++ci){
        for (int exp = 0; exp <= 1; ++exp){
            pddl_slab_t s;
            pddlSlabInit(&s, cfgs[ci].slab_bytes, NUM_CLASSES, exp,
                         cfgs[ci].first_segm_bytes);
            for (int cls = 0; cls < NUM_CLASSES; ++cls){
                for (uint32_t i = 0; i < n; ++i){
                    uint32_t id = pddlSlabAlloc(&s, cls);
                    assert(id == mkId(cls, i));
                }
            }

            for (int cls = 0; cls < NUM_CLASSES; ++cls){
                const pddl_segvec_t *sv = &s.arena[cls].slab;
                int first = expFirstSegmSize(pddlSlabClassBytes(&s, cls),
                                             cfgs[ci].first_segm_bytes);
                assert(first >= 1);
                assert(first == 1
                        || (size_t)first * pddlSlabClassBytes(&s, cls)
                                <= s.first_segm_bytes);
                assert(pddlSegVecElSize(sv) == pddlSlabClassBytes(&s, cls));
                assert(sv->exp == exp);
                assert(pddlSegVecSegmentSize(sv, 0) == first);

                // The minimal number of segments holding n slabs
                int num_segm = 0;
                size_t cap = 0;
                while (cap < n){
                    int size = pddlSegVecSegmentSize(sv, num_segm);
                    if (exp){
                        assert(size == (num_segm == 0
                                            ? first
                                            : first << (num_segm - 1)));
                    }else{
                        assert(size == first);
                    }
                    cap += (size_t)size;
                    ++num_segm;
                }
                assert(pddlSegVecNumSegments(sv) == num_segm);
                assert(pddlSegVecCapacity(sv) == cap);
                if (!exp)
                    assert(num_segm == (int)((n + first - 1) / first));
            }
            assert(pddlSlabAllocBytes(&s) == expAllocBytes(&s));
            pddlSlabFree(&s);
        }
    }
}

/*
 * Released slabs are reused in the LIFO order, they come back zeroed, the
 * number of slabs taken from the arena does not grow while free slabs are
 * available, and the other slabs keep their content.
 */
TEST_ONCE(slab_release_reuse)
{
    for (int ci = 0; ci < NUM_CFGS; ++ci){
        for (int exp = 0; exp <= 1; ++exp){
            pddl_slab_t s;
            pddlSlabInit(&s, cfgs[ci].slab_bytes, NUM_CLASSES, exp,
                         cfgs[ci].first_segm_bytes);
            for (int cls = 0; cls < 2; ++cls){
                for (uint32_t i = 0; i < 10; ++i){
                    uint32_t id = allocCheck(&s, cls, i);
                    stamp(&s, id, seedOf(id));
                }
            }

            pddlSlabRelease(&s, mkId(1, 3));
            pddlSlabRelease(&s, mkId(1, 7));
            pddlSlabRelease(&s, mkId(1, 5));
            // Releasing in another class does not interfere
            pddlSlabRelease(&s, mkId(0, 9));
            assert(pddlSlabNumSlabs(&s, 0) == 10);
            assert(pddlSlabNumSlabs(&s, 1) == 10);
            size_t bytes = pddlSlabAllocBytes(&s);

            const uint32_t exp_idx[] = { 5, 7, 3 };
            for (int i = 0; i < 3; ++i){
                uint32_t id = allocCheck(&s, 1, exp_idx[i]);
                stamp(&s, id, seedOf(id));
                assert(pddlSlabNumSlabs(&s, 1) == 10);
            }
            assert(pddlSlabAllocBytes(&s) == bytes);

            // The free list of the class 1 is empty now
            uint32_t id = allocCheck(&s, 1, 10);
            stamp(&s, id, seedOf(id));
            assert(pddlSlabNumSlabs(&s, 1) == 11);

            id = allocCheck(&s, 0, 9);
            stamp(&s, id, seedOf(id));
            id = allocCheck(&s, 0, 10);
            stamp(&s, id, seedOf(id));

            for (int cls = 0; cls < 2; ++cls){
                for (uint32_t i = 0; i <= 10; ++i)
                    checkStamp(&s, mkId(cls, i), seedOf(mkId(cls, i)));
            }

            // Releasing all slabs and allocating them again reverses the
            // order of the indexes
            for (uint32_t i = 0; i <= 10; ++i)
                pddlSlabRelease(&s, mkId(0, i));
            for (uint32_t i = 0; i <= 10; ++i)
                allocCheck(&s, 0, 10 - i);
            assert(pddlSlabNumSlabs(&s, 0) == 11);
            for (uint32_t i = 0; i <= 10; ++i)
                checkStamp(&s, mkId(1, i), seedOf(mkId(1, i)));

            pddlSlabFree(&s);
        }
    }
}

/*
 * Slabs never move: the pointers to slabs stay the same and the slabs keep
 * their content while many more slabs are allocated and released.
 */
TEST_ONCE(slab_pointer_stability)
{
    for (int ci = 0; ci < NUM_CFGS; ++ci){
        for (int exp = 0; exp <= 1; ++exp){
            pddl_slab_t s;
            pddlSlabInit(&s, cfgs[ci].slab_bytes, 2, exp,
                         cfgs[ci].first_segm_bytes);
            void *ptr[2][20];
            for (int cls = 0; cls < 2; ++cls){
                for (uint32_t i = 0; i < 20; ++i){
                    uint32_t id = allocCheck(&s, cls, i);
                    ptr[cls][i] = pddlSlabAt(&s, id);
                    stamp(&s, id, seedOf(id));
                }
            }

            for (uint32_t i = 0; i < 2000; ++i){
                int cls = (int)(i % 2);
                uint32_t id = pddlSlabAlloc(&s, cls);
                assert(pddlSlabIdClass(id) == cls);
                stamp(&s, id, seedOf(id));
                if (i % 3 == 0)
                    pddlSlabRelease(&s, id);
            }

            for (int cls = 0; cls < 2; ++cls){
                for (uint32_t i = 0; i < 20; ++i){
                    uint32_t id = mkId(cls, i);
                    assert(ptr[cls][i] == pddlSlabAt(&s, id));
                    checkStamp(&s, id, seedOf(id));
                }
            }
            pddlSlabFree(&s);
        }
    }
}

/*
 * Reset releases all slabs but keeps the allocated memory: the indexes
 * start from zero again, the free lists are empty, and the reused slabs are
 * zeroed.
 */
TEST_ONCE(slab_reset)
{
    for (int ci = 0; ci < NUM_CFGS; ++ci){
        for (int exp = 0; exp <= 1; ++exp){
            pddl_slab_t s;
            pddlSlabInit(&s, cfgs[ci].slab_bytes, NUM_CLASSES, exp,
                         cfgs[ci].first_segm_bytes);
            for (int cls = 0; cls < 3; ++cls){
                for (uint32_t i = 0; i < 30; ++i){
                    uint32_t id = allocCheck(&s, cls, i);
                    stamp(&s, id, seedOf(id));
                }
                pddlSlabRelease(&s, mkId(cls, 4));
                pddlSlabRelease(&s, mkId(cls, 17));
            }
            size_t bytes = pddlSlabAllocBytes(&s);
            void *ptr0 = pddlSlabAt(&s, mkId(0, 0));

            pddlSlabReset(&s);
            assert(pddlSlabAllocBytes(&s) == bytes);
            for (int cls = 0; cls < NUM_CLASSES; ++cls){
                assert(pddlSlabNumSlabs(&s, cls) == 0);
                assert(s.arena[cls].free_head == 0);
            }

            for (int cls = 0; cls < 3; ++cls){
                for (uint32_t i = 0; i < 30; ++i)
                    allocCheck(&s, cls, i);
            }
            // The memory is reused
            assert(pddlSlabAt(&s, mkId(0, 0)) == ptr0);
            assert(pddlSlabAllocBytes(&s) == bytes);

            pddlSlabFree(&s);
        }
    }
}

/* Model of a single class for slab_random_model. */
struct model_cls {
    /* IDs of the allocated slabs */
    uint32_t *live;
    int num_live;
    /* Stack of the indexes of the released slabs (the top is the last
     * one) */
    uint32_t *free;
    int num_free;
    /* Number of slabs taken from the arena */
    uint32_t num_slabs;
    /* Seed of the content of each slab indexed by the index of the slab */
    unsigned *seed;
};
typedef struct model_cls model_cls_t;

/* Checks the content of all allocated slabs against the model M. */
static void modelCheck(const pddl_slab_t *s, const model_cls_t *m,
                       int num_classes)
{
    for (int cls = 0; cls < num_classes; ++cls){
        assert(pddlSlabNumSlabs(s, cls) == m[cls].num_slabs);
        assert(s->arena[cls].free_head
                == (m[cls].num_free == 0
                        ? 0u : m[cls].free[m[cls].num_free - 1] + 1u));
        for (int i = 0; i < m[cls].num_live; ++i){
            uint32_t id = m[cls].live[i];
            assert(pddlSlabIdClass(id) == cls);
            checkStamp(s, id, m[cls].seed[pddlSlabIdIndex(id)]);
        }
    }
}

/*
 * Random sequence of allocations and releases over all classes checked
 * against a model of the allocated slabs and the free lists.
 */
TEST_ONCE(slab_random_model)
{
    const int num_classes = 5;
    const int num_ops = 20000;
    for (int exp = 0; exp <= 1; ++exp){
        pddl_slab_t s;
        pddlSlabInit(&s, 8, num_classes, exp, 128);
        model_cls_t m[5];
        for (int cls = 0; cls < num_classes; ++cls){
            m[cls].live = malloc(sizeof(uint32_t) * num_ops);
            m[cls].num_live = 0;
            m[cls].free = malloc(sizeof(uint32_t) * num_ops);
            m[cls].num_free = 0;
            m[cls].num_slabs = 0;
            m[cls].seed = malloc(sizeof(unsigned) * num_ops);
        }

        pddl_rand_t *rnd = pddlRandNew(2468 + exp);
        for (int op = 0; op < num_ops; ++op){
            int cls = (int)(pddlRandInt(rnd) % (uint32_t)num_classes);
            model_cls_t *mc = m + cls;
            if (mc->num_live == 0 || pddlRandInt(rnd) % 100 < 60){
                uint32_t exp_idx;
                if (mc->num_free > 0){
                    exp_idx = mc->free[--mc->num_free];
                }else{
                    exp_idx = mc->num_slabs++;
                }
                uint32_t id = allocCheck(&s, cls, exp_idx);
                mc->seed[exp_idx] = pddlRandInt(rnd);
                stamp(&s, id, mc->seed[exp_idx]);
                mc->live[mc->num_live++] = id;

            }else{
                int i = (int)(pddlRandInt(rnd) % (uint32_t)mc->num_live);
                uint32_t id = mc->live[i];
                mc->live[i] = mc->live[--mc->num_live];
                pddlSlabRelease(&s, id);
                mc->free[mc->num_free++] = pddlSlabIdIndex(id);
            }

            if (op % 1000 == 0)
                modelCheck(&s, m, num_classes);
        }
        modelCheck(&s, m, num_classes);
        assert(pddlSlabAllocBytes(&s) == expAllocBytes(&s));
        pddlRandDel(rnd);

        for (int cls = 0; cls < num_classes; ++cls){
            free(m[cls].live);
            free(m[cls].free);
            free(m[cls].seed);
        }
        pddlSlabFree(&s);
    }
}

/* Slabs smaller than sizeof(uint32_t) cannot hold the free-list link. */
TEST_PANIC_ONCE(slab_panic_small_slab)
{
    pddl_slab_t s;
    pddlSlabInit(&s, sizeof(uint32_t) - 1, 4, pddl_false, 0);
}

/* At least one class is required. */
TEST_PANIC_ONCE(slab_panic_zero_classes)
{
    pddl_slab_t s;
    pddlSlabInit(&s, 8, 0, pddl_false, 0);
}

/* At most PDDL_SLAB_MAX_CLASSES classes are allowed. */
TEST_PANIC_ONCE(slab_panic_too_many_classes)
{
    pddl_slab_t s;
    pddlSlabInit(&s, 8, PDDL_SLAB_MAX_CLASSES + 1, pddl_true, 0);
}

/* The size of the slabs of the last class must fit in size_t. */
TEST_PANIC_ONCE(slab_panic_class_overflow)
{
    pddl_slab_t s;
    pddlSlabInit(&s, ((size_t)1) << (8 * sizeof(size_t) - 1), 2,
                 pddl_true, 0);
}

/* Allocating a slab of a class past the last class panics. */
TEST_PANIC_ONCE(slab_panic_alloc_bad_class)
{
    pddl_slab_t s;
    pddlSlabInit(&s, 8, 4, pddl_false, 0);
    uint32_t id = pddlSlabAlloc(&s, 3);
    assert(id == mkId(3, 0));
    id = pddlSlabAlloc(&s, 4);
    assert(id == 0);
}

/* Allocating a slab of a negative class panics. */
TEST_PANIC_ONCE(slab_panic_alloc_negative_class)
{
    pddl_slab_t s;
    pddlSlabInit(&s, 8, 4, pddl_false, 0);
    uint32_t id = pddlSlabAlloc(&s, -1);
    assert(id == 0);
}

/* Releasing an ID with a class past the last class panics. */
TEST_PANIC_ONCE(slab_panic_release_bad_class)
{
    pddl_slab_t s;
    pddlSlabInit(&s, 8, 4, pddl_false, 0);
    uint32_t id = pddlSlabAlloc(&s, 3);
    assert(id == mkId(3, 0));
    pddlSlabRelease(&s, mkId(4, 0));
}

/* Releasing an ID with an index that was never allocated panics. */
TEST_PANIC_ONCE(slab_panic_release_bad_index)
{
    pddl_slab_t s;
    pddlSlabInit(&s, 8, 4, pddl_false, 0);
    uint32_t id = pddlSlabAlloc(&s, 2);
    assert(id == mkId(2, 0));
    id = pddlSlabAlloc(&s, 2);
    assert(id == mkId(2, 1));
    pddlSlabRelease(&s, mkId(2, 2));
}
