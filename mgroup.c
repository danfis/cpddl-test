#include <assert.h>
#include "test.h"
#include "context.h"

TEST(ground_lifted_mgroup, strips)
{
    pddlMGroupsGround(&C.mg, &C.pddl, &C.lmg, &C.strips);
    pddlMGroupsSetExactlyOne(&C.mg, &C.strips);
    pddlMGroupsSetGoal(&C.mg, &C.strips);
    pddlMGroupsPrint(&C.pddl, &C.strips, &C.mg, stdout);
}

TEST_TEAR_DOWN(ground_lifted_mgroup)
{
    pddlMGroupsFree(&C.mg);
}

TEST(ground_lifted_mgroup_noce, strips_noce)
{
    pddlMGroupsGround(&C.mg, &C.pddl, &C.lmg, &C.strips);
    pddlMGroupsSetExactlyOne(&C.mg, &C.strips);
    pddlMGroupsSetGoal(&C.mg, &C.strips);
    pddlMGroupsPrint(&C.pddl, &C.strips, &C.mg, stdout);
}

TEST_TEAR_DOWN(ground_lifted_mgroup_noce)
{
    pddlMGroupsFree(&C.mg);
}
