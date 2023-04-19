#include "test.h"
#include "context.h"
#include <assert.h>

TEST(gaifman, pddl_compile_away_cond_eff)
{
    pddl_bool_t has_disconnected_action = pddl_false;
    for (int ai = 0; ai < C.pddl.action.action_size; ++ai){
        const pddl_action_t *a = C.pddl.action.action + ai;
        int diameter = pddlGaifmanActionPreDiameter(a);
        if (diameter < 0){
            printf("Diameter for %s: inf\n", a->name);
            has_disconnected_action = pddl_true;
        }else{
            printf("Diameter for %s: %d\n", a->name, diameter);
        }
    }

    pddl_gaifman_t ginit;
    pddlGaifmanInit(&ginit, C.pddl.obj.obj_size);
    pddlGaifmanAddRelationsFromFm(&ginit, &C.pddl.init->fm);

    pddl_gaifman_t ggoal;
    pddlGaifmanInit(&ggoal, C.pddl.obj.obj_size);
    pddlGaifmanAddRelationsFromFm(&ggoal, C.pddl.goal);
    for (int oi1 = 0; oi1 < C.pddl.obj.obj_size; ++oi1){
        for (int oi2 = oi1 + 1; oi2 < C.pddl.obj.obj_size; ++oi2){
            int dgoal = pddlGaifmanDistance(&ggoal, oi1, oi2);
            int dinit = pddlGaifmanDistance(&ginit, oi1, oi2);
            if (dgoal >= 0 && dinit >= 0){
                printf("Init-to-goal-distance %s - %s: %d\n",
                       C.pddl.obj.obj[oi1].name,
                       C.pddl.obj.obj[oi2].name,
                       dinit - dgoal);
                if (C.optimal_cost >= 0)
                    assert(dinit - dgoal < C.optimal_cost);

            }else if (dgoal >= 0 && dinit < 0){
                printf("Init-to-goal-distance %s - %s: inf\n",
                       C.pddl.obj.obj[oi1].name,
                       C.pddl.obj.obj[oi2].name);
                if (C.optimal_cost >= 0)
                    assert(!has_disconnected_action);
            }
        }
    }

    pddlGaifmanFree(&ggoal);
    pddlGaifmanFree(&ginit);
}
