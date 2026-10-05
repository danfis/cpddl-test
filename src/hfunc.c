/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/pddl.h"
#include "test.h"
#include <assert.h>

#define XXH3_BUF_SIZE 5000

/* Deterministic input buffer (the same used for computing the reference
 * values with the upstream xxHash library). */
static void xxh3Buf(unsigned char *buf)
{
    for (int i = 0; i < XXH3_BUF_SIZE; ++i)
        buf[i] = (unsigned char)((i * 2654435761u) >> 13);
}

/* Hashes BUF[0:SIZE] with STATE in chunks of pseudo-random sizes (including
 * zero-length chunks). */
static void xxh3Stream(pddl_xxh3_state_t *state,
                       const unsigned char *buf, size_t size,
                       uint32_t *rnd)
{
    size_t pos = 0;
    while (pos < size){
        *rnd = *rnd * 1103515245u + 12345u;
        size_t len = (*rnd >> 16) % 300;
        if (len > size - pos)
            len = size - pos;
        pddlXXH3StateUpdate(state, buf + pos, len);
        pos += len;
    }
}

/*
 * XXH3 one-shot and streaming hashes agree with reference values computed
 * by the upstream xxHash v0.8.4 for inputs covering all code paths, the
 * streaming variant agrees with the one-shot variant for any chunking, and
 * copy, reset, and digest of the state behave correctly regardless of the
 * (mis)alignment of the state in memory.
 */
TEST_ONCE(hfunc_xxh3)
{
    static const struct {
        size_t size;
        uint64_t hash;
    } ref[] = {
        { 0, 0x2d06800538d394c2ULL },
        { 1, 0xc44bdff4074eecdbULL },
        { 3, 0xa1c4a8259b827291ULL },
        { 16, 0x222e9aead6bddd51ULL },
        { 17, 0x47aad6b375eb4bbaULL },
        { 128, 0x421a9c905c6e66baULL },
        { 129, 0x9e2414800f83768aULL },
        { 240, 0xb714c5fd22744964ULL },
        { 241, 0xbc424a2c480dd281ULL },
        { 1024, 0x1fd15e7d36f5e1bcULL },
        { 5000, 0x853377ef13cec7bdULL },
    };
    int ref_size = sizeof(ref) / sizeof(ref[0]);

    unsigned char buf[XXH3_BUF_SIZE];
    xxh3Buf(buf);
    uint32_t rnd = 1;

    // Reference values of one-shot and streaming variants
    for (int i = 0; i < ref_size; ++i){
        assert(pddlXXH3_64(buf, ref[i].size) == ref[i].hash);
        assert(pddlXXH3_32(buf, ref[i].size) == (uint32_t)ref[i].hash);

        pddl_xxh3_state_t state;
        pddlXXH3StateInit(&state);
        xxh3Stream(&state, buf, ref[i].size, &rnd);
        assert(pddlXXH3StateDigest_64(&state) == ref[i].hash);
        assert(pddlXXH3StateDigest_32(&state) == (uint32_t)ref[i].hash);
        pddlXXH3StateFree(&state);
    }

    // Reference values of seeded streaming variant
    pddl_xxh3_state_t state;
    pddlXXH3StateInit(&state);
    pddlXXH3StateReset(&state, 0xdeadbeefu);
    xxh3Stream(&state, buf, 1000, &rnd);
    assert(pddlXXH3StateDigest_64(&state) == 0x67947038a3a2c612ULL);
    // Digest does not modify the state
    assert(pddlXXH3StateDigest_64(&state) == 0x67947038a3a2c612ULL);

    pddlXXH3StateReset(&state, 7);
    xxh3Stream(&state, buf, 100, &rnd);
    assert(pddlXXH3StateDigest_64(&state) == 0xc819088e86e5f418ULL);

    // Reset to seed 0 equals the one-shot variant
    pddlXXH3StateReset(&state, 0);
    xxh3Stream(&state, buf, 777, &rnd);
    assert(pddlXXH3StateDigest_64(&state) == pddlXXH3_64(buf, 777));
    pddlXXH3StateFree(&state);

    // Streaming equals one-shot for all sizes up to 2100
    for (size_t size = 0; size <= 2100; ++size){
        pddl_xxh3_state_t st;
        pddlXXH3StateInit(&st);
        xxh3Stream(&st, buf, size, &rnd);
        assert(pddlXXH3StateDigest_64(&st) == pddlXXH3_64(buf, size));
        pddlXXH3StateFree(&st);
    }

    // States at different offsets modulo 64: on stack after a char member,
    // and in a heap-allocated array.
    struct {
        char c;
        pddl_xxh3_state_t state;
    } shifted;
    pddl_xxh3_state_t *arr = PDDL_ALLOC_ARR(pddl_xxh3_state_t, 8);
    for (int i = 0; i < 8; ++i){
        pddlXXH3StateInit(arr + i);
        pddlXXH3StateReset(arr + i, 11);
        xxh3Stream(arr + i, buf, 3000, &rnd);
    }
    pddlXXH3StateInit(&shifted.state);
    pddlXXH3StateReset(&shifted.state, 11);
    xxh3Stream(&shifted.state, buf, 3000, &rnd);
    uint64_t hash = pddlXXH3StateDigest_64(&shifted.state);
    for (int i = 0; i < 8; ++i)
        assert(pddlXXH3StateDigest_64(arr + i) == hash);

    // Copy mid-stream between states at different offsets and continue
    // hashing both
    for (int i = 0; i < 8; ++i){
        pddlXXH3StateReset(arr + i, 13);
        pddlXXH3StateUpdate(arr + i, buf, 1500 + i);
        pddlXXH3StateFree(&shifted.state);
        pddlXXH3StateInitCopy(&shifted.state, arr + i);
        pddlXXH3StateUpdate(arr + i, buf + 1500 + i, 2000);
        pddlXXH3StateUpdate(&shifted.state, buf + 1500 + i, 2000);
        assert(pddlXXH3StateDigest_64(arr + i)
                == pddlXXH3StateDigest_64(&shifted.state));

        pddl_xxh3_state_t st;
        pddlXXH3StateInit(&st);
        pddlXXH3StateReset(&st, 13);
        pddlXXH3StateUpdate(&st, buf, 3500 + i);
        assert(pddlXXH3StateDigest_64(&st)
                == pddlXXH3StateDigest_64(&shifted.state));
        pddlXXH3StateFree(&st);
    }
    pddlXXH3StateFree(&shifted.state);
    for (int i = 0; i < 8; ++i)
        pddlXXH3StateFree(arr + i);
    PDDL_FREE(arr);
}
