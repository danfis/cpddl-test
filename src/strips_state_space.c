#include "test.h"
#include "context.h"
#include <pddl/strips_state_space.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/** Maximum number of states expanded by the BFS test */
#define BFS_MAX_EXPANSIONS 100

/** Asserts that the state with SID carries the default search data set by
 *  pddlStripsStateSpaceInsert() for newly inserted states. */
static void assertDefaults(pddl_strips_state_space_t *space,
                           pddl_state_id_t sid,
                           pddl_strips_state_space_node_t *node)
{
    pddlStripsStateSpaceGetNoState(space, sid, node);
    assert(node->id == sid);
    assert(node->parent_id == PDDL_NO_STATE_ID);
    assert(node->op_id == -1);
    assert(node->g_value == -1);
    assert(node->status == PDDL_STRIPS_STATE_SPACE_STATUS_NEW);
}

TEST(strips_state_space, strips)
{
    if (pddlHasNumericFluents(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }
}

TEST(strips_state_space_insert, strips_state_space)
{
    pddl_strips_state_space_t space;
    pddlStripsStateSpaceInit(&space, &C.err);
    pddl_strips_state_space_node_t node;
    pddlStripsStateSpaceNodeInit(&node, &space);

    // The first inserted state gets ID 0 and the default search data
    pddl_state_id_t init_id = pddlStripsStateSpaceInsert(&space, &C.strips.init);
    assert(init_id == 0);
    assert(space.num_states == 1);
    assertDefaults(&space, init_id, &node);

    // Insert the successor state of every operator applicable in the
    // initial state, keeping local copies for round-trip checks
    pddl_iset_t *state = calloc(C.strips.op.op_size + 1, sizeof(*state));
    pddl_state_id_t *state_id
            = calloc(C.strips.op.op_size + 1, sizeof(*state_id));
    int num = 0;
    pddlISetInit(&state[num]);
    pddlISetSet(&state[num], &C.strips.init);
    state_id[num++] = init_id;

    PDDL_ISET(succ);
    for (int opi = 0; opi < C.strips.op.op_size; ++opi){
        const pddl_strips_op_t *op = C.strips.op.op[opi];
        if (!pddlISetIsSubset(&op->pre, &C.strips.init))
            continue;
        pddlStripsOpApplyOnState(op, &C.strips.init, &succ);

        int prev_num_states = space.num_states;
        pddl_state_id_t sid = pddlStripsStateSpaceInsert(&space, &succ);
        int is_dup = 0;
        for (int i = 0; i < num && !is_dup; ++i){
            if (pddlISetEq(&state[i], &succ)){
                // Duplicates return the stored ID and do not add a state
                assert(sid == state_id[i]);
                assert(space.num_states == prev_num_states);
                is_dup = 1;
            }
        }
        if (!is_dup){
            // New states get consecutive IDs and the default search data
            assert(sid == (pddl_state_id_t)prev_num_states);
            assert(space.num_states == prev_num_states + 1);
            assertDefaults(&space, sid, &node);
            pddlISetInit(&state[num]);
            pddlISetSet(&state[num], &succ);
            state_id[num++] = sid;
        }
    }
    assert(space.num_states == num);

    // Re-inserting every state returns the same ID and adds nothing
    for (int i = 0; i < num; ++i){
        assert(pddlStripsStateSpaceInsert(&space, &state[i]) == state_id[i]);
    }
    assert(space.num_states == num);

    // Get() returns a copy of the stored state
    for (int i = 0; i < num; ++i){
        pddlStripsStateSpaceGet(&space, state_id[i], &node);
        assert(pddlISetEq(&node.state, &state[i]));
    }

    printf("states: %d\n", space.num_states);

    for (int i = 0; i < num; ++i)
        pddlISetFree(&state[i]);
    free(state);
    free(state_id);
    pddlISetFree(&succ);
    pddlStripsStateSpaceNodeFree(&node);
    pddlStripsStateSpaceFree(&space);
}

TEST(strips_state_space_node, strips_state_space)
{
    pddl_strips_state_space_t space;
    pddlStripsStateSpaceInit(&space, &C.err);
    pddl_strips_state_space_node_t node;
    pddlStripsStateSpaceNodeInit(&node, &space);

    pddl_state_id_t init_id = pddlStripsStateSpaceInsert(&space, &C.strips.init);
    assert(init_id == 0);

    // Set() and GetNoState() round-trip all four search-data fields
    pddlStripsStateSpaceGetNoState(&space, init_id, &node);
    node.g_value = 0;
    node.status = PDDL_STRIPS_STATE_SPACE_STATUS_OPEN;
    pddlStripsStateSpaceSet(&space, &node);
    pddlStripsStateSpaceGetNoState(&space, init_id, &node);
    assert(node.parent_id == PDDL_NO_STATE_ID);
    assert(node.op_id == -1);
    assert(node.g_value == 0);
    assert(node.status == PDDL_STRIPS_STATE_SPACE_STATUS_OPEN);

    node.status = PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED;
    pddlStripsStateSpaceSet(&space, &node);
    pddlStripsStateSpaceGetNoState(&space, init_id, &node);
    assert(node.status == PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED);

    // The same on a successor state, with a real parent/op/g-value
    int num_succ = 0;
    PDDL_ISET(succ);
    for (int opi = 0; opi < C.strips.op.op_size; ++opi){
        const pddl_strips_op_t *op = C.strips.op.op[opi];
        if (!pddlISetIsSubset(&op->pre, &C.strips.init))
            continue;
        pddlStripsOpApplyOnState(op, &C.strips.init, &succ);
        if (pddlISetEq(&succ, &C.strips.init))
            continue;

        pddl_state_id_t sid = pddlStripsStateSpaceInsert(&space, &succ);
        pddlStripsStateSpaceGetNoState(&space, sid, &node);
        if (node.status != PDDL_STRIPS_STATE_SPACE_STATUS_NEW)
            continue;
        ++num_succ;
        node.parent_id = init_id;
        node.op_id = opi;
        node.g_value = op->cost;
        node.status = PDDL_STRIPS_STATE_SPACE_STATUS_OPEN;
        pddlStripsStateSpaceSet(&space, &node);

        pddlStripsStateSpaceGet(&space, sid, &node);
        assert(node.id == sid);
        assert(node.parent_id == init_id);
        assert(node.op_id == opi);
        assert(node.g_value == op->cost);
        assert(node.status == PDDL_STRIPS_STATE_SPACE_STATUS_OPEN);
        assert(pddlISetEq(&node.state, &succ));

        // Re-inserting an existing state preserves its search data
        assert(pddlStripsStateSpaceInsert(&space, &succ) == sid);
        pddlStripsStateSpaceGetNoState(&space, sid, &node);
        assert(node.parent_id == init_id);
        assert(node.op_id == opi);
        assert(node.g_value == op->cost);
        assert(node.status == PDDL_STRIPS_STATE_SPACE_STATUS_OPEN);
    }

    // The node is only a snapshot: mutating node.state and writing it
    // back with Set() does not change the stored state
    pddlStripsStateSpaceGet(&space, init_id, &node);
    assert(pddlISetEq(&node.state, &C.strips.init));
    pddlISetAdd(&node.state, C.strips.fact.fact_size + 100);
    pddlStripsStateSpaceSet(&space, &node);
    pddlStripsStateSpaceGet(&space, init_id, &node);
    assert(pddlISetEq(&node.state, &C.strips.init));
    assert(pddlStripsStateSpaceInsert(&space, &C.strips.init) == init_id);

    printf("states: %d successors: %d\n", space.num_states, num_succ);

    pddlISetFree(&succ);
    pddlStripsStateSpaceNodeFree(&node);
    pddlStripsStateSpaceFree(&space);
}

TEST(strips_state_space_bfs, strips_state_space)
{
    pddl_strips_state_space_t space;
    pddlStripsStateSpaceInit(&space, &C.err);
    pddl_strips_state_space_node_t cur, next;
    pddlStripsStateSpaceNodeInit(&cur, &space);
    pddlStripsStateSpaceNodeInit(&next, &space);

    pddl_state_id_t init_id = pddlStripsStateSpaceInsert(&space, &C.strips.init);
    assert(init_id == 0);
    pddlStripsStateSpaceGetNoState(&space, init_id, &cur);
    cur.g_value = 0;
    cur.status = PDDL_STRIPS_STATE_SPACE_STATUS_OPEN;
    pddlStripsStateSpaceSet(&space, &cur);

    // Breadth-first search expanding states in the order of their IDs,
    // using the Insert/GetNoState/Set idiom of the lifted search
    int expanded = 0;
    pddl_state_id_t goal_id = PDDL_NO_STATE_ID;
    PDDL_ISET(succ);
    for (pddl_state_id_t sid = 0;
            sid < (pddl_state_id_t)space.num_states
                && expanded < BFS_MAX_EXPANSIONS;
            ++sid){
        pddlStripsStateSpaceGet(&space, sid, &cur);
        assert(cur.status == PDDL_STRIPS_STATE_SPACE_STATUS_OPEN);
        if (pddlISetIsSubset(&C.strips.goal, &cur.state)){
            goal_id = sid;
            break;
        }
        cur.status = PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED;
        pddlStripsStateSpaceSet(&space, &cur);
        ++expanded;

        for (int opi = 0; opi < C.strips.op.op_size; ++opi){
            const pddl_strips_op_t *op = C.strips.op.op[opi];
            if (!pddlISetIsSubset(&op->pre, &cur.state))
                continue;
            pddlStripsOpApplyOnState(op, &cur.state, &succ);
            pddl_state_id_t next_id = pddlStripsStateSpaceInsert(&space, &succ);
            pddlStripsStateSpaceGetNoState(&space, next_id, &next);
            if (next.status == PDDL_STRIPS_STATE_SPACE_STATUS_NEW){
                assert(next.parent_id == PDDL_NO_STATE_ID);
                next.parent_id = sid;
                next.op_id = opi;
                next.g_value = cur.g_value + op->cost;
                next.status = PDDL_STRIPS_STATE_SPACE_STATUS_OPEN;
                pddlStripsStateSpaceSet(&space, &next);
            }
        }
    }

    // Every inserted state was immediately marked open, every expanded
    // state closed, and every state except the initial one was first
    // reached from a state with a smaller ID
    int num_closed = 0;
    for (pddl_state_id_t sid = 0;
            sid < (pddl_state_id_t)space.num_states; ++sid){
        pddlStripsStateSpaceGetNoState(&space, sid, &next);
        assert(next.status != PDDL_STRIPS_STATE_SPACE_STATUS_NEW);
        if (next.status == PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED)
            ++num_closed;
        if (sid == 0){
            assert(next.parent_id == PDDL_NO_STATE_ID);
        }else{
            assert(next.parent_id < sid);
        }
    }
    assert(num_closed == expanded);

    printf("states: %d expanded: %d goal: %s\n",
           space.num_states, expanded,
           goal_id != PDDL_NO_STATE_ID ? "reached" : "not reached");

    if (goal_id != PDDL_NO_STATE_ID){
        // Backtrack the plan and verify every step of the path
        int *plan_op = calloc(space.num_states, sizeof(*plan_op));
        int plan_len = 0;
        int plan_cost = 0;
        pddlStripsStateSpaceGetNoState(&space, goal_id, &next);
        int goal_g = next.g_value;
        pddl_state_id_t sid = goal_id;
        while (sid != 0){
            pddlStripsStateSpaceGet(&space, sid, &next);
            assert(next.op_id >= 0 && next.op_id < C.strips.op.op_size);
            const pddl_strips_op_t *op = C.strips.op.op[next.op_id];
            pddlStripsStateSpaceGet(&space, next.parent_id, &cur);
            assert(pddlISetIsSubset(&op->pre, &cur.state));
            pddlStripsOpApplyOnState(op, &cur.state, &succ);
            assert(pddlISetEq(&succ, &next.state));
            assert(next.g_value == cur.g_value + op->cost);
            plan_op[plan_len++] = next.op_id;
            plan_cost += op->cost;
            sid = next.parent_id;
        }
        assert(plan_cost == goal_g);

        printf("plan length: %d cost: %d\n", plan_len, goal_g);
        for (int i = plan_len - 1; i >= 0; --i)
            printf("%s\n", C.strips.op.op[plan_op[i]]->name);
        free(plan_op);
    }

    pddlISetFree(&succ);
    pddlStripsStateSpaceNodeFree(&cur);
    pddlStripsStateSpaceNodeFree(&next);
    pddlStripsStateSpaceFree(&space);
}

TEST_ONCE(strips_state_space_once)
{
}

TEST(strips_state_space_once_basic, strips_state_space_once)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;

    // Init/Free round-trip of an empty state space
    pddlStripsStateSpaceInit(&space, &err);
    assert(space.num_states == 0);
    pddlStripsStateSpaceFree(&space);

    pddlStripsStateSpaceInit(&space, &err);
    pddl_strips_state_space_node_t node;
    pddlStripsStateSpaceNodeInit(&node, &space);

    // {}, {0}, {1}, {0,1}, {0,1,2} -- the empty set is a valid state
    pddl_iset_t set[5];
    for (int i = 0; i < 5; ++i)
        pddlISetInit(&set[i]);
    pddlISetAdd(&set[1], 0);
    pddlISetAdd(&set[2], 1);
    pddlISetAdd(&set[3], 0);
    pddlISetAdd(&set[3], 1);
    pddlISetAdd(&set[4], 0);
    pddlISetAdd(&set[4], 1);
    pddlISetAdd(&set[4], 2);

    for (int i = 0; i < 5; ++i){
        pddl_state_id_t sid = pddlStripsStateSpaceInsert(&space, &set[i]);
        assert(sid == (pddl_state_id_t)i);
        assert(space.num_states == i + 1);
        assertDefaults(&space, sid, &node);
    }
    for (int i = 0; i < 5; ++i){
        assert(pddlStripsStateSpaceInsert(&space, &set[i])
                    == (pddl_state_id_t)i);
    }
    assert(space.num_states == 5);

    // Round-trip of the search-data extremes: op_id is stored in a
    // signed 30-bit bitfield and status in a 2-bit bitfield
    pddlStripsStateSpaceGetNoState(&space, 3, &node);
    node.parent_id = 0;
    node.op_id = (1 << 29) - 1;
    node.g_value = 1 << 30;
    node.status = PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED;
    pddlStripsStateSpaceSet(&space, &node);
    pddlStripsStateSpaceGetNoState(&space, 3, &node);
    assert(node.parent_id == 0);
    assert(node.op_id == (1 << 29) - 1);
    assert(node.g_value == 1 << 30);
    assert(node.status == PDDL_STRIPS_STATE_SPACE_STATUS_CLOSED);

    node.parent_id = PDDL_NO_STATE_ID;
    node.op_id = -1;
    node.g_value = -1;
    node.status = PDDL_STRIPS_STATE_SPACE_STATUS_OPEN;
    pddlStripsStateSpaceSet(&space, &node);
    pddlStripsStateSpaceGetNoState(&space, 3, &node);
    assert(node.parent_id == PDDL_NO_STATE_ID);
    assert(node.op_id == -1);
    assert(node.g_value == -1);
    assert(node.status == PDDL_STRIPS_STATE_SPACE_STATUS_OPEN);

    node.status = PDDL_STRIPS_STATE_SPACE_STATUS_NEW;
    pddlStripsStateSpaceSet(&space, &node);
    pddlStripsStateSpaceGetNoState(&space, 3, &node);
    assert(node.status == PDDL_STRIPS_STATE_SPACE_STATUS_NEW);

    // The node is a snapshot: mutating node.state does not change the
    // stored state
    pddlStripsStateSpaceGet(&space, 1, &node);
    assert(pddlISetEq(&node.state, &set[1]));
    pddlISetAdd(&node.state, 100);
    pddlStripsStateSpaceSet(&space, &node);
    pddlStripsStateSpaceGet(&space, 1, &node);
    assert(pddlISetEq(&node.state, &set[1]));
    assert(pddlStripsStateSpaceInsert(&space, &set[1]) == 1);

    printf("states: %d\n", space.num_states);

    for (int i = 0; i < 5; ++i)
        pddlISetFree(&set[i]);
    pddlStripsStateSpaceNodeFree(&node);
    pddlStripsStateSpaceFree(&space);
}

TEST(strips_state_space_once_many, strips_state_space_once)
{
    const int num_states = 1000;
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    pddlStripsStateSpaceInit(&space, &err);
    pddl_strips_state_space_node_t node;
    pddlStripsStateSpaceNodeInit(&node, &space);

    // The sets are pairwise distinct because i is the minimal element
    // of the i-th set
    PDDL_ISET(state);
    for (int i = 0; i < num_states; ++i){
        pddlISetEmpty(&state);
        pddlISetAdd(&state, i);
        pddlISetAdd(&state, i + 1);
        pddlISetAdd(&state, 2 * i);
        assert(pddlStripsStateSpaceInsert(&space, &state)
                    == (pddl_state_id_t)i);
    }
    assert(space.num_states == num_states);

    for (int i = 0; i < num_states; ++i){
        pddlISetEmpty(&state);
        pddlISetAdd(&state, i);
        pddlISetAdd(&state, i + 1);
        pddlISetAdd(&state, 2 * i);
        assert(pddlStripsStateSpaceInsert(&space, &state)
                    == (pddl_state_id_t)i);
        if (i % 100 == 0){
            pddlStripsStateSpaceGet(&space, i, &node);
            assert(pddlISetEq(&node.state, &state));
        }
    }
    assert(space.num_states == num_states);

    printf("states: %d\n", space.num_states);

    pddlISetFree(&state);
    pddlStripsStateSpaceNodeFree(&node);
    pddlStripsStateSpaceFree(&space);
}

/** Accesses the state STATE_ID in SPACE using pddlStripsStateSpaceGet() for
 *  METHOD 0, pddlStripsStateSpaceGetNoState() for METHOD 1, and
 *  pddlStripsStateSpaceSet() for METHOD 2. */
static void accessState(pddl_strips_state_space_t *space,
                        int method,
                        pddl_state_id_t state_id)
{
    pddl_strips_state_space_node_t node;
    pddlStripsStateSpaceNodeInit(&node, space);
    if (method == 0){
        pddlStripsStateSpaceGet(space, state_id, &node);

    }else if (method == 1){
        pddlStripsStateSpaceGetNoState(space, state_id, &node);

    }else{
        node.id = state_id;
        pddlStripsStateSpaceSet(space, &node);
    }
    pddlStripsStateSpaceNodeFree(&node);
}

/** Initializes SPACE and inserts the states {} and {0} into it, i.e., the
 *  only valid state IDs afterwards are 0 and 1. */
static void initSpaceWithTwoStates(pddl_strips_state_space_t *space,
                                   pddl_err_t *err)
{
    pddlStripsStateSpaceInit(space, err);

    PDDL_ISET(s0);
    PDDL_ISET(s1);
    pddlISetAdd(&s1, 0);
    pddlStripsStateSpaceInsert(space, &s0);
    pddlStripsStateSpaceInsert(space, &s1);
    pddlISetFree(&s0);
    pddlISetFree(&s1);
}

TEST(strips_state_space_once_valid_id, strips_state_space_once)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    initSpaceWithTwoStates(&space, &err);

    // Valid IDs do not panic with any of the access methods
    for (int method = 0; method < 3; ++method){
        accessState(&space, method, 0);
        accessState(&space, method, 1);
    }

    pddlStripsStateSpaceFree(&space);
}

// Get(), GetNoState(), and Set() panic on any ID in an empty space
TEST_PANIC_ONCE(strips_state_space_get_empty)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    pddlStripsStateSpaceInit(&space, &err);
    accessState(&space, 0, 0);
}

TEST_PANIC_ONCE(strips_state_space_get_no_state_empty)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    pddlStripsStateSpaceInit(&space, &err);
    accessState(&space, 1, 0);
}

TEST_PANIC_ONCE(strips_state_space_set_empty)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    pddlStripsStateSpaceInit(&space, &err);
    accessState(&space, 2, 0);
}

// The first unassigned ID panics with any of the access methods
TEST_PANIC_ONCE(strips_state_space_get_unassigned)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    initSpaceWithTwoStates(&space, &err);
    accessState(&space, 0, space.num_states);
}

TEST_PANIC_ONCE(strips_state_space_get_no_state_unassigned)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    initSpaceWithTwoStates(&space, &err);
    accessState(&space, 1, space.num_states);
}

TEST_PANIC_ONCE(strips_state_space_set_unassigned)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    initSpaceWithTwoStates(&space, &err);
    accessState(&space, 2, space.num_states);
}

// PDDL_NO_STATE_ID panics with any of the access methods
TEST_PANIC_ONCE(strips_state_space_get_no_state_id)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    initSpaceWithTwoStates(&space, &err);
    accessState(&space, 0, PDDL_NO_STATE_ID);
}

TEST_PANIC_ONCE(strips_state_space_get_no_state_no_state_id)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    initSpaceWithTwoStates(&space, &err);
    accessState(&space, 1, PDDL_NO_STATE_ID);
}

TEST_PANIC_ONCE(strips_state_space_set_no_state_id)
{
    pddl_err_t err = PDDL_ERR_INIT;
    pddl_strips_state_space_t space;
    initSpaceWithTwoStates(&space, &err);
    accessState(&space, 2, PDDL_NO_STATE_ID);
}
