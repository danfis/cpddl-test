#include <assert.h>
#include "test.h"
#include "context.h"

context_t context = { 0 };

TEST(root, _)
{
    bor_err_t err = BOR_ERR_INIT;
    if (pddlFiles(&context.files, "../test/pddl-data/", TEST_TASK, &err) != 0){
        borErrPrint(&err, 1, stderr);
        assert(0);
    }
}

TEST_TEAR_DOWN(root)
{
}
