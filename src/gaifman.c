#include "test.h"
#include "context.h"
#include <assert.h>

TEST(gaifman, pddl_compile_away_cond_eff)
{
    int max_diameter = 0;
    for (int ai = 0; ai < C.pddl.action.action_size; ++ai){
        const pddl_action_t *a = C.pddl.action.action + ai;
        int diameter = pddlGaifmanActionPreDiameter(a);
        if (diameter < 0){
            printf("Diameter for %s: inf\n", a->name);
            max_diameter = -1;
        }else{
            printf("Diameter for %s: %d\n", a->name, diameter);
        }
        if (max_diameter >= 0)
            max_diameter = PDDL_MAX(max_diameter, diameter);
    }

    pddl_gaifman_t ginit;
    pddlGaifmanInit(&ginit, C.pddl.obj.obj_size);
    pddlGaifmanAddRelationsFromFm(&ginit, &C.pddl.init->fm);

    pddl_gaifman_t ggoal;
    pddlGaifmanInit(&ggoal, C.pddl.obj.obj_size);
    pddlGaifmanAddRelationsFromFm(&ggoal, C.pddl.goal);

    pddl_gaifman_t ggoal2;
    pddlGaifmanInit(&ggoal2, C.pddl.obj.obj_size);
    pddlGaifmanAddRelationsFromFm(&ggoal2, C.pddl.goal);
    pddl_fm_const_it_atom_t ait;
    PDDL_FM_FOR_EACH_ATOM(&C.pddl.init->fm, &ait, atom){
        if (pddlPredIsStatic(&C.pddl.pred.pred[atom->pred]))
            pddlGaifmanAddRelationsFromAtom(&ggoal2, atom);
    }

    for (int oi1 = 0; oi1 < C.pddl.obj.obj_size; ++oi1){
        for (int oi2 = oi1 + 1; oi2 < C.pddl.obj.obj_size; ++oi2){
            int dgoal = pddlGaifmanDistance(&ggoal, oi1, oi2);
            assert(dgoal >= -1);
            int dgoal2 = pddlGaifmanDistance(&ggoal2, oi1, oi2);
            assert(dgoal2 >= -1);
            int dinit = pddlGaifmanDistance(&ginit, oi1, oi2);
            assert(dinit >= -1);
            if (dgoal >= 0)
                assert(dgoal2 <= dgoal && dgoal2 >= 0);

            if (dgoal >= 0 && dinit >= 0){
                printf("Init-to-goal-distance %s - %s: %d",
                       C.pddl.obj.obj[oi1].name,
                       C.pddl.obj.obj[oi2].name,
                       dinit - dgoal);
                if (dgoal2 != dgoal)
                    printf(", w/ static: %d", dinit - dgoal2);
                if (max_diameter >= 0){
                    printf(", length bound: %d",
                           (int)ceil((dinit - dgoal2) / (float)max_diameter));
                }
                printf("\n");
                if (C.optimal_cost >= 0)
                    assert(dinit - dgoal <= C.optimal_cost);

            }else if (dgoal >= 0 && dinit < 0){
                printf("Init-to-goal-distance %s - %s: inf\n",
                       C.pddl.obj.obj[oi1].name,
                       C.pddl.obj.obj[oi2].name);
                fflush(stdout);
                if (C.optimal_cost >= 0)
                    assert(max_diameter < 0);
            }
        }
    }

    pddlGaifmanFree(&ggoal2);
    pddlGaifmanFree(&ggoal);
    pddlGaifmanFree(&ginit);
}
