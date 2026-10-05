/***
 * Copyright (c)2026 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

/*
 * Tests of the FDR state space (pddl/fdr_state_space.h), in particular of
 * the compact storage of state nodes in segments of different types and
 * the promotion of segments.
 *
 * All tests are TEST_ONCE (not per task).
 * Run with:  cd tests && make && ./test -T _ -s fdr_state_space_
 */

#include "pddl/fdr_state_space.h"
#include "pddl/lp.h"
#include "test.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Segment types; these must mirror NODE_*_TYPE in src/fdr_state_space.c */
#define SMALL 0
#define INT 1
#define NUM 2

/** Largest parent ID representable in the small segment type */
#define SMALL_MAX_PARENT_ID ((pddl_state_id_t)0x7FFFFEu)
/** Largest valid state ID */
#define STATE_ID_MAX ((pddl_state_id_t)0x7FFFFFFEu)

/** Number of values of each of the two variables of the test state space */
#define VAL_SIZE 2048

/** State space over two variables together with a node */
struct space {
    pddl_err_t err;
    pddl_fdr_vars_t vars;
    pddl_fdr_state_space_t space;
    pddl_fdr_state_space_node_t node;
};
typedef struct space space_t;

static void spaceInit(space_t *s)
{
    memset(s, 0, sizeof(*s));
    pddlErrInit(&s->err);
    for (int var = 0; var < 2; ++var){
        pddl_fdr_var_t *v = pddlFDRVarsAdd(&s->vars, VAL_SIZE);
        assert(v->var_id == var);
    }
    pddl_fdr_state_packer_config_t packer_cfg
            = PDDL_FDR_STATE_PACKER_CONFIG_INIT;
    packer_cfg.vars = &s->vars;
    int ret = pddlFDRStateSpaceInit(&s->space, &packer_cfg, &s->err);
    assert(ret == 0);
    pddlFDRStateSpaceNodeInit(&s->node, &s->space);
}

static void spaceFree(space_t *s)
{
    pddlFDRStateSpaceNodeFree(&s->node);
    pddlFDRStateSpaceFree(&s->space);
    pddlFDRVarsFree(&s->vars);
}

/** Inserts the I-th state of the space (all states are distinct) and
 *  asserts it gets the ID I */
static void spaceInsert(space_t *s, int i)
{
    int state[2] = { i % VAL_SIZE, i / VAL_SIZE };
    pddl_bool_t is_new;
    pddl_state_id_t id = pddlFDRStateSpaceInsert(&s->space, state, &is_new);
    assert(is_new);
    assert(id == (pddl_state_id_t)i);
}

/** Inserts the states 0, ..., NUM - 1 */
static void spaceInsertN(space_t *s, int num)
{
    for (int i = 0; i < num; ++i)
        spaceInsert(s, i);
}

/** Returns the type of the segment storing the state ID */
static int segmType(const space_t *s, pddl_state_id_t id)
{
    size_t per_segm = pddlFDRStateSpaceNodesPerSegment(&s->space);
    return s->space.node_pool.segm_type[id / per_segm];
}

/** Stores the search data of the state ID */
static void nodeSet(space_t *s,
                    pddl_state_id_t id,
                    pddl_bool_t is_closed,
                    pddl_state_id_t parent_id,
                    const pddl_num_t *g)
{
    s->node.id = id;
    s->node.is_closed = is_closed;
    s->node.parent_id = parent_id;
    s->node.g_value = *g;
    pddlFDRStateSpaceSet(&s->space, &s->node);
}

/** Stores the search data of the state ID with an integer g-value G */
static void nodeSetInt(space_t *s,
                       pddl_state_id_t id,
                       pddl_bool_t is_closed,
                       pddl_state_id_t parent_id,
                       int g)
{
    pddl_num_t num;
    pddlNumSetInt(&num, g);
    nodeSet(s, id, is_closed, parent_id, &num);
}

/** Asserts the stored search data of the state ID */
static void assertNode(space_t *s,
                       pddl_state_id_t id,
                       pddl_bool_t is_closed,
                       pddl_state_id_t parent_id,
                       const pddl_num_t *g)
{
    memset(&s->node.g_value, 0xff, sizeof(s->node.g_value));
    pddlFDRStateSpaceGetNoState(&s->space, id, &s->node);
    assert(s->node.id == id);
    assert(s->node.is_closed == is_closed);
    assert(s->node.parent_id == parent_id);
    assert(pddlNumExactEq(&s->node.g_value, g));
}

/** Asserts the stored search data of the state ID with an integer g-value */
static void assertNodeInt(space_t *s,
                          pddl_state_id_t id,
                          pddl_bool_t is_closed,
                          pddl_state_id_t parent_id,
                          int g)
{
    pddl_num_t num;
    pddlNumSetInt(&num, g);
    assertNode(s, id, is_closed, parent_id, &num);
}

/*
 * Sets up the states 0, 1, 2 of a fresh small segment with distinct
 * search data that are checked by assertKeptNodes() after a promotion.
 */
static void setKeptNodes(space_t *s)
{
    spaceInsertN(s, 4);
    nodeSetInt(s, 0, pddl_true, PDDL_NO_STATE_ID, 0);
    nodeSetInt(s, 1, pddl_false, 0, 255);
    nodeSetInt(s, 2, pddl_true, SMALL_MAX_PARENT_ID, 7);
    assert(segmType(s, 0) == SMALL);
}

/** Asserts the nodes set by setKeptNodes() */
static void assertKeptNodes(space_t *s)
{
    assertNodeInt(s, 0, pddl_true, PDDL_NO_STATE_ID, 0);
    assertNodeInt(s, 1, pddl_false, 0, 255);
    assertNodeInt(s, 2, pddl_true, SMALL_MAX_PARENT_ID, 7);
}

/*
 * Newly inserted states are not closed, have no parent and zero g-value,
 * are stored in a small segment, and re-inserting a state returns the same
 * ID without touching its node.
 */
TEST_ONCE(fdr_state_space_node_default)
{
    space_t s;
    spaceInit(&s);
    spaceInsertN(&s, 3);
    for (int i = 0; i < 3; ++i)
        assertNodeInt(&s, i, pddl_false, PDDL_NO_STATE_ID, 0);
    assert(segmType(&s, 0) == SMALL);

    nodeSetInt(&s, 1, pddl_true, 0, 3);
    int state[2] = { 1, 0 };
    pddl_bool_t is_new;
    pddl_state_id_t id = pddlFDRStateSpaceInsert(&s.space, state, &is_new);
    assert(!is_new);
    assert(id == 1);
    assertNodeInt(&s, 1, pddl_true, 0, 3);

    pddlFDRStateSpaceGet(&s.space, 1, &s.node);
    assert(s.node.state[0] == 1 && s.node.state[1] == 0);
    spaceFree(&s);
}

/*
 * Values representable in the small segment type (g-values 0..255, parent
 * IDs up to 0x7FFFFE, and no parent) round-trip without promotion.
 */
TEST_ONCE(fdr_state_space_node_small)
{
    space_t s;
    spaceInit(&s);
    spaceInsertN(&s, 4);
    nodeSetInt(&s, 0, pddl_true, PDDL_NO_STATE_ID, 0);
    nodeSetInt(&s, 1, pddl_false, 0, 255);
    nodeSetInt(&s, 2, pddl_true, SMALL_MAX_PARENT_ID, 128);
    nodeSetInt(&s, 3, pddl_false, 2, 1);
    assert(segmType(&s, 0) == SMALL);
    assertNodeInt(&s, 0, pddl_true, PDDL_NO_STATE_ID, 0);
    assertNodeInt(&s, 1, pddl_false, 0, 255);
    assertNodeInt(&s, 2, pddl_true, SMALL_MAX_PARENT_ID, 128);
    assertNodeInt(&s, 3, pddl_false, 2, 1);
    spaceFree(&s);
}

/*
 * A g-value above 255, a negative g-value, or a parent ID not fitting into
 * the small type promotes the segment to the int type while all other
 * nodes of the segment keep their values, and the int segment is not
 * demoted by writing small values.
 */
TEST_ONCE(fdr_state_space_node_promote_int)
{
    for (int c = 0; c < 4; ++c){
        space_t s;
        spaceInit(&s);
        setKeptNodes(&s);
        if (c == 0){
            nodeSetInt(&s, 3, pddl_false, 1, 256);
            assert(segmType(&s, 0) == INT);
            assertNodeInt(&s, 3, pddl_false, 1, 256);

        }else if (c == 1){
            nodeSetInt(&s, 3, pddl_true, 1, -1);
            assert(segmType(&s, 0) == INT);
            assertNodeInt(&s, 3, pddl_true, 1, -1);

        }else if (c == 2){
            nodeSetInt(&s, 3, pddl_false, SMALL_MAX_PARENT_ID + 1, 1);
            assert(segmType(&s, 0) == INT);
            assertNodeInt(&s, 3, pddl_false, SMALL_MAX_PARENT_ID + 1, 1);

        }else{
            nodeSetInt(&s, 3, pddl_true, STATE_ID_MAX, INT_MAX);
            assert(segmType(&s, 0) == INT);
            assertNodeInt(&s, 3, pddl_true, STATE_ID_MAX, INT_MAX);
        }
        assertKeptNodes(&s);

        nodeSetInt(&s, 3, pddl_false, PDDL_NO_STATE_ID, 2);
        assert(segmType(&s, 0) == INT);
        assertNodeInt(&s, 3, pddl_false, PDDL_NO_STATE_ID, 2);
        assertKeptNodes(&s);
        spaceFree(&s);
    }
}

/*
 * A float g-value, a g-value with a non-zero delta, or an infinite g-value
 * promotes the segment to the num type, both from the small and from the
 * int type, while all other nodes of the segment keep their values.
 */
TEST_ONCE(fdr_state_space_node_promote_num)
{
    for (int from_int = 0; from_int < 2; ++from_int){
        for (int c = 0; c < 3; ++c){
            space_t s;
            spaceInit(&s);
            setKeptNodes(&s);
            if (from_int){
                nodeSetInt(&s, 3, pddl_true, PDDL_NO_STATE_ID, -5);
                assert(segmType(&s, 0) == INT);
            }

            pddl_num_t g;
            if (c == 0){
                pddlNumSetFlt(&g, 1.5f);
            }else if (c == 1){
                pddlNumSetIntDelta(&g, 3, 1);
            }else{
                pddlNumSetInf(&g);
            }
            nodeSet(&s, 2, pddl_false, 1, &g);
            assert(segmType(&s, 0) == NUM);
            assertNode(&s, 2, pddl_false, 1, &g);
            assertNodeInt(&s, 0, pddl_true, PDDL_NO_STATE_ID, 0);
            assertNodeInt(&s, 1, pddl_false, 0, 255);
            if (from_int){
                assertNodeInt(&s, 3, pddl_true, PDDL_NO_STATE_ID, -5);
            }else{
                assertNodeInt(&s, 3, pddl_false, PDDL_NO_STATE_ID, 0);
            }

            nodeSetInt(&s, 2, pddl_true, STATE_ID_MAX, 4);
            assert(segmType(&s, 0) == NUM);
            assertNodeInt(&s, 2, pddl_true, STATE_ID_MAX, 4);
            spaceFree(&s);
        }
    }
}

/*
 * A newly allocated segment inherits the type of the previous segment and
 * states on both sides of the segment boundary keep their search data;
 * promoting the new segment does not change the previous one.
 */
TEST_ONCE(fdr_state_space_node_segm_inherit)
{
    space_t s;
    spaceInit(&s);
    int per_segm = pddlFDRStateSpaceNodesPerSegment(&s.space);
    assert(per_segm > 0 && per_segm < VAL_SIZE * VAL_SIZE);
    assert((per_segm & (per_segm - 1)) == 0);

    spaceInsertN(&s, per_segm);
    nodeSetInt(&s, per_segm - 1, pddl_true, 0, 1000);
    assert(segmType(&s, 0) == INT);
    assert(s.space.node_pool.segm_size == 1);

    spaceInsert(&s, per_segm);
    assert(s.space.node_pool.segm_size == 2);
    assert(segmType(&s, per_segm) == INT);
    assertNodeInt(&s, per_segm, pddl_false, PDDL_NO_STATE_ID, 0);
    nodeSetInt(&s, per_segm, pddl_false, per_segm - 1, 1);
    assert(segmType(&s, per_segm) == INT);

    pddl_num_t g;
    pddlNumSetFlt(&g, 0.25f);
    nodeSet(&s, per_segm, pddl_true, per_segm - 1, &g);
    assert(segmType(&s, 0) == INT);
    assert(segmType(&s, per_segm) == NUM);
    assertNode(&s, per_segm, pddl_true, per_segm - 1, &g);
    assertNodeInt(&s, per_segm - 1, pddl_true, 0, 1000);
    assertNodeInt(&s, 0, pddl_false, PDDL_NO_STATE_ID, 0);
    spaceFree(&s);
}

/** Asserts that the two state pool statistics are equal */
static void assertStatePoolStatEq(const pddl_fdr_state_pool_stat_t *a,
                                  const pddl_fdr_state_pool_stat_t *b)
{
    assert(a->num_states == b->num_states);
    assert(a->packed_state_size == b->packed_state_size);
    assert(a->segments == b->segments);
    assert(a->states_bytes == b->states_bytes);
    assert(a->htable_buckets == b->htable_buckets);
    assert(a->htable_overflow_buckets == b->htable_overflow_buckets);
    assert(a->htable_max_bucket_size == b->htable_max_bucket_size);
    assert(a->htable_bytes == b->htable_bytes);
}

/*
 * Statistics of the state pool count the stored states and the memory
 * allocated for them and for the hash table, and re-inserting an already
 * stored state does not change them.
 */
TEST_ONCE(fdr_state_pool_stat)
{
    space_t s;
    spaceInit(&s);
    const pddl_fdr_state_pool_t *sp = &s.space.state_pool;

    pddl_fdr_state_pool_stat_t stat;
    pddlFDRStatePoolStat(sp, &stat);
    assert(stat.num_states == 0);
    assert(stat.htable_overflow_buckets == 0);
    assert(stat.htable_max_bucket_size == 0);

    spaceInsertN(&s, 1000);
    pddlFDRStatePoolStat(sp, &stat);
    assert(stat.num_states == 1000);
    assert(stat.packed_state_size
            == (size_t)pddlFDRStatePackerBufSize(&sp->packer));
    // 1000 tiny states fit in a single segment
    assert(stat.segments == 1);
    assert(stat.states_bytes
            >= pddlSegVecCapacity(&sp->states.el) * stat.packed_state_size);
    assert(stat.states_bytes >= stat.num_states * stat.packed_state_size);
    assert(stat.htable_buckets == 4096);
    assert(stat.htable_max_bucket_size >= 1);
    assert(stat.htable_bytes >= stat.htable_buckets);

    int state[2] = { 1, 0 };
    pddl_bool_t is_new;
    pddl_state_id_t id = pddlFDRStateSpaceInsert(&s.space, state, &is_new);
    assert(!is_new);
    assert(id == 1);
    pddl_fdr_state_pool_stat_t stat2;
    pddlFDRStatePoolStat(sp, &stat2);
    assertStatePoolStatEq(&stat, &stat2);

    spaceFree(&s);
}

/** Sets STATE to the I-th state of fdr_state_pool_segments: the first 8
 *  variables encode I, the others are derived from I and the variable */
static void segmState(int *state, int num_vars, int i)
{
    for (int var = 0; var < num_vars; ++var){
        if (var < 8){
            state[var] = (i >> (4 * var)) & 15;
        }else{
            state[var] = (i * 7 + var) % 16;
        }
    }
}

/*
 * Packed states are stored in segments of 2^K states: states of 4096 bytes
 * are stored 1024 per segment (4 MB), segments are allocated only as
 * needed, and the states on the segment boundaries are stored and
 * retrieved correctly.
 */
TEST_ONCE(fdr_state_pool_segments)
{
    // 8 variables with 16 values are packed in each 32-bit word
    const int num_vars = 8192;
    const int num = 2 * 1024 + 500;
    pddl_err_t err;
    pddlErrInit(&err);
    pddl_fdr_vars_t vars;
    memset(&vars, 0, sizeof(vars));
    for (int var = 0; var < num_vars; ++var)
        pddlFDRVarsAdd(&vars, 16);

    pddl_fdr_state_pool_t sp;
    pddl_fdr_state_packer_config_t packer_cfg
            = PDDL_FDR_STATE_PACKER_CONFIG_INIT;
    packer_cfg.vars = &vars;
    int ret = pddlFDRStatePoolInit(&sp, &packer_cfg, &err);
    assert(ret == 0);
    assert(pddlIdxSetElSize(&sp.states) == 4096);
    assert(pddlSegVecNumSegments(&sp.states.el) == 0);

    int *state = calloc(num_vars, sizeof(int));
    int *state2 = calloc(num_vars, sizeof(int));
    pddl_fdr_state_pool_stat_t stat;
    for (int i = 0; i < num; ++i){
        segmState(state, num_vars, i);
        pddl_state_id_t id = pddlFDRStatePoolInsert(&sp, state);
        assert(id == (pddl_state_id_t)i);
        pddlFDRStatePoolStat(&sp, &stat);
        assert(stat.segments == (size_t)(i / 1024 + 1));
        assert(pddlSegVecCapacity(&sp.states.el) == stat.segments * 1024);
        assert(stat.states_bytes >= stat.segments * (4096ul << 10));
    }

    for (int i = 0; i < num; ++i){
        segmState(state, num_vars, i);
        pddl_state_id_t id = pddlFDRStatePoolInsert(&sp, state);
        assert(id == (pddl_state_id_t)i);
        pddlFDRStatePoolGet(&sp, id, state2);
        assert(memcmp(state, state2, sizeof(int) * num_vars) == 0);
    }

    pddl_fdr_state_pool_stat_t stat2;
    pddlFDRStatePoolStat(&sp, &stat2);
    assert(stat2.num_states == (size_t)num);
    assert(stat2.segments == 3);
    assertStatePoolStatEq(&stat, &stat2);

    free(state);
    free(state2);
    pddlFDRStatePoolFree(&sp);
    pddlFDRVarsFree(&vars);
}

/*
 * The hash table of the state pool starts with 4096 buckets and doubles its
 * size whenever the load would reach 2; some buckets overflow into slabs,
 * the resizing neither loses nor duplicates any state, and the stored
 * states are unchanged.
 */
TEST_ONCE(fdr_state_pool_resize)
{
    space_t s;
    spaceInit(&s);
    const pddl_fdr_state_pool_t *sp = &s.space.state_pool;
    const int num = 40000;

    pddl_fdr_state_pool_stat_t stat;
    spaceInsertN(&s, 8191);
    pddlFDRStatePoolStat(sp, &stat);
    assert(stat.htable_buckets == 4096);
    spaceInsert(&s, 8191);
    pddlFDRStatePoolStat(sp, &stat);
    assert(stat.htable_buckets == 8192);

    for (int i = 8192; i < num; ++i)
        spaceInsert(&s, i);
    pddlFDRStatePoolStat(sp, &stat);
    assert(stat.num_states == (size_t)num);
    assert(stat.htable_buckets == 32768);
    // Buckets with more than two states are stored in slabs
    assert(stat.htable_overflow_buckets > 0);
    assert(stat.htable_max_bucket_size >= 3);
    assert(stat.htable_bytes >= stat.htable_buckets);

    for (int i = 0; i < num; ++i){
        int state[2] = { i % VAL_SIZE, i / VAL_SIZE };
        pddl_bool_t is_new;
        pddl_state_id_t id = pddlFDRStateSpaceInsert(&s.space, state, &is_new);
        assert(!is_new);
        assert(id == (pddl_state_id_t)i);

        pddlFDRStateSpaceGet(&s.space, id, &s.node);
        assert(s.node.state[0] == state[0] && s.node.state[1] == state[1]);
    }

    pddl_fdr_state_pool_stat_t stat2;
    pddlFDRStatePoolStat(sp, &stat2);
    assertStatePoolStatEq(&stat, &stat2);

    spaceFree(&s);
}

/*
 * The state space uses the packer configured by the given config: with the
 * ILP layout, it packs states into fewer words than with FFD on variables
 * of widths {15,13,12,10,7,5}. Skipped without an LP solver.
 */
TEST_ONCE(fdr_state_space_packer_cfg)
{
    if (!pddlLPSolverAvailable(PDDL_LP_DEFAULT))
        return;

    static const int width[6] = { 15, 13, 12, 10, 7, 5 };
    pddl_err_t err;
    pddlErrInit(&err);
    pddl_fdr_vars_t vars;
    memset(&vars, 0, sizeof(vars));
    for (int i = 0; i < 6; ++i)
        pddlFDRVarsAdd(&vars, 1 << width[i]);

    pddl_fdr_state_packer_layout_t layouts[2] = {
        PDDL_FDR_STATE_PACKER_LAYOUT_FFD,
        PDDL_FDR_STATE_PACKER_LAYOUT_ILP,
    };
    const int exp_bufsize[2] = { 12, 8 };
    for (int li = 0; li < 2; ++li){
        pddl_fdr_state_packer_config_t packer_cfg
                = PDDL_FDR_STATE_PACKER_CONFIG_INIT;
        packer_cfg.vars = &vars;
        packer_cfg.layout = layouts[li];
        pddl_fdr_state_space_t space;
        int ret = pddlFDRStateSpaceInit(&space, &packer_cfg, &err);
        assert(ret == 0);
        assert(pddlFDRStatePackerBufSize(&space.state_pool.packer)
                == exp_bufsize[li]);

        int state[6];
        for (int i = 0; i < 6; ++i)
            state[i] = (1 << width[i]) - 1 - i;
        pddl_bool_t is_new;
        pddl_state_id_t id = pddlFDRStateSpaceInsert(&space, state, &is_new);
        assert(is_new);
        assert(id == 0);

        pddl_fdr_state_space_node_t node;
        pddlFDRStateSpaceNodeInit(&node, &space);
        pddlFDRStateSpaceGet(&space, id, &node);
        assert(memcmp(node.state, state, sizeof(state)) == 0);
        pddlFDRStateSpaceNodeFree(&node);
        pddlFDRStateSpaceFree(&space);
    }

    pddlFDRVarsFree(&vars);
}

/*
 * Initializing a state space (and its state pool) with an invalid packer
 * config (no .vars) fails with an error.
 */
TEST_ONCE(fdr_state_space_init_err)
{
    pddl_fdr_state_packer_config_t packer_cfg
            = PDDL_FDR_STATE_PACKER_CONFIG_INIT;

    pddl_err_t err;
    pddlErrInit(&err);
    pddl_fdr_state_space_t space;
    int ret = pddlFDRStateSpaceInit(&space, &packer_cfg, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));

    pddlErrInit(&err);
    pddl_fdr_state_pool_t pool;
    ret = pddlFDRStatePoolInit(&pool, &packer_cfg, &err);
    assert(ret == -1);
    assert(pddlErrIsSet(&err));
}
