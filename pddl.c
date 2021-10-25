#include <assert.h>
#include "test.h"
#include "context.h"

TEST(pddl, root)
{
    fprintf(stderr, "pddl\n");
}

TEST_TEAR_DOWN(pddl)
{
}

TEST(pddl_clone, pddl)
{
    fprintf(stderr, "pddl_clone\n");
    assert(0);
}

TEST_TEAR_DOWN(pddl_clone)
{
}

TEST(pddl_clone2, pddl)
{
    fprintf(stderr, "pddl_clone2\n");
}

TEST_TEAR_DOWN(pddl_clone2)
{
}
