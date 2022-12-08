#include "test.h"
#include "context.h"
#include <assert.h>

TEST(hff_strips, fdr)
{
    PDDL_IARR(plan);
    pddl_hff_t hff;
    pddlHFFInitStrips(&hff, &C.strips);
    int heur = pddlHFFStripsPlan(&hff, &C.strips.init, &plan);
    fprintf(stdout, "heur-ff: %d\n", heur);
    if (heur != PDDL_COST_DEAD_END){
        // TODO
        //assertTrue(pddlFDRIsRelaxedPlan(&fdr, fdr.init, &plan, &err));
    }
    pddlHFFFree(&hff);
    pddlIArrFree(&plan);

    pddl_hmax_t hmax;
    pddlHMaxInit(&hmax, &C.fdr);
    int heur_max = pddlHMax(&hmax, C.fdr.init, &C.fdr.var);
    fprintf(stdout, "heur-max: %d\n", heur_max);
    pddlHMaxFree(&hmax);

    pddl_hadd_t hadd;
    pddlHAddInit(&hadd, &C.fdr);
    int heur_add = pddlHAdd(&hadd, C.fdr.init, &C.fdr.var);
    fprintf(stdout, "heur-add: %d\n", heur_add);
    pddlHAddFree(&hadd);

    assert(heur_max <= heur);
    assert(heur <= heur_add);
}

TEST(hff_fdr, fdr)
{
    PDDL_IARR(plan);
    pddl_hff_t hff;
    pddlHFFInit(&hff, &C.fdr);
    int heur = pddlHFFPlan(&hff, C.fdr.init, &C.fdr.var, &plan);
    fprintf(stdout, "heur-ff: %d\n", heur);
    if (heur != PDDL_COST_DEAD_END){
        assert(pddlFDRIsRelaxedPlan(&C.fdr, C.fdr.init, &plan, &C.err));
    }
    pddlHFFFree(&hff);
    pddlIArrFree(&plan);

    pddl_hmax_t hmax;
    pddlHMaxInit(&hmax, &C.fdr);
    int heur_max = pddlHMax(&hmax, C.fdr.init, &C.fdr.var);
    fprintf(stdout, "heur-max: %d\n", heur_max);
    pddlHMaxFree(&hmax);

    pddl_hadd_t hadd;
    pddlHAddInit(&hadd, &C.fdr);
    int heur_add = pddlHAdd(&hadd, C.fdr.init, &C.fdr.var);
    fprintf(stdout, "heur-add: %d\n", heur_add);
    pddlHAddFree(&hadd);

    assert(heur_max <= heur);
    assert(heur <= heur_add);
}
