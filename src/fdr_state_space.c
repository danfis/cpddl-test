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
#include "test.h"
#include <assert.h>
#include <stdio.h>
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
    pddlFDRStateSpaceInit(&s->space, &s->vars, &s->err);
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

/*
 * Statistics of the node pool report the number of nodes, the number of
 * segments of each type, and the memory allocated for the segments.
 */
TEST_ONCE(fdr_state_space_log_stats)
{
    space_t s;
    spaceInit(&s);
    setKeptNodes(&s);
    nodeSetInt(&s, 3, pddl_false, 1, 1000);
    assert(segmType(&s, 0) == INT);

    FILE *fout = tmpfile();
    assert(fout != NULL);
    pddlErrLogEnable(&s.err, fout);
    pddlFDRStateSpaceLogStats(&s.space, &s.err);
    pddlErrFlush(&s.err);

    char buf[1024];
    rewind(fout);
    size_t len = fread(buf, 1, sizeof(buf) - 1, fout);
    buf[len] = 0;
    fclose(fout);

    size_t alloc = pddlFDRStateSpaceNodesPerSegment(&s.space) * 8;
    char expected[128];
    snprintf(expected, sizeof(expected), "allocated: %lu bytes",
             (unsigned long)alloc);
    assert(strstr(buf, "Node pool: nodes: 4,") != NULL);
    assert(strstr(buf, "segments: 1 (small: 0, int: 1, num: 0)") != NULL);
    assert(strstr(buf, expected) != NULL);
    spaceFree(&s);
}
