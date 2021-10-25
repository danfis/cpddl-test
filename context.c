#include <assert.h>
#include "test.h"
#include "context.h"

context_t C = { 0 };

TEST(root, _)
{
    borErrInit(&C.err);
    if (pddlFiles(&C.files, "../test/pddl-data/", TEST_TASK, &C.err) != 0){
        borErrPrint(&C.err, 1, stderr);
        assert(0);
    }
}

TEST_TEAR_DOWN(root)
{
}
