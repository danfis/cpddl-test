#include "test.h"
#include "context.h"
#include <assert.h>

static int isInMGroup(int fact, const pddl_mgroups_t *mgroups)
{
    for (int i = 0; i < mgroups->mgroup_size; ++i){
        if (pddlISetIn(fact, &mgroups->mgroup[i].mgroup))
            return 1;
    }
    return 0;
}

TEST(rse_invertible, strips_pruned)
{
    PDDL_ISET(invertible);
    pddlRSEInvertibleFacts(&C.strips, &C.mg, &invertible, &C.err);

    if (pddlISetSize(&invertible) > 0)
        printf("RSE-Invertible facts:\n");
    int fact;
    PDDL_ISET_FOR_EACH(&invertible, fact){
        printf("%d:(%s)", fact, C.strips.fact.fact[fact]->name);
        if (isInMGroup(fact, &C.mg))
            printf(" in-fam-group");
        printf("\n");
    }
    fflush(stdout);
    pddlISetFree(&invertible);
}
