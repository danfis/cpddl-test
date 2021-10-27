#include <assert.h>
#include "test.h"
#include "context.h"

TEST(ground_lifted_mgroup, strips)
{
    pddlMGroupsGround(&C.mg, &C.pddl, &C.lmg, &C.strips);
    C.mg_set = 1;
    pddlMGroupsSetExactlyOne(&C.mg, &C.strips);
    pddlMGroupsSetGoal(&C.mg, &C.strips);
    pddlMGroupsPrint(&C.pddl, &C.strips, &C.mg, stdout);
}

TEST(ground_lifted_mgroup_noce, strips_noce)
{
    test_ground_lifted_mgroup();
}
