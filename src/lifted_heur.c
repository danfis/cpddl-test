#include "test.h"
#include "context.h"
#include <assert.h>

TEST(lifted_heur_hadd_unit_cost, pddl_unit_cost)
{
    if (pddlHasNumericFluents(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }

    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.remove_static_facts = 0;
    pddl_strips_t strips;
    int ret = pddlStripsGroundDatalog(&strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    pddl_ground_atoms_t gatoms;
    pddlGroundAtomsInit(&gatoms);
    for (int fact = 0; fact < strips.fact.fact_size; ++fact){
        const pddl_ground_atom_t *ga = strips.fact.fact[fact]->ground_atom;
        assert(ga != NULL);
        pddlGroundAtomsAddPred(&gatoms, ga->pred, ga->arg, ga->arity);
    }

    pddl_hadd_t hadd;
    pddlHAddInitStrips(&hadd, &strips);

    pddl_lifted_heur_relaxed_t h;
    pddlLiftedHAddInit(&h, &C.pddl, 0, &C.err);

    PDDL_ISET(state);
    pddlISetUnion(&state, &strips.init);
    pddl_cost_t lc = pddlLiftedHeurRelaxedEvalState(&h, &state, &gatoms);
    int c = pddlHAddStrips(&hadd, &state);
    //fprintf(stderr, "lc: %d:%d , c: %d\n", lc.cost, lc.zero_cost, c);
    assert(lc.cost == c);
    int num_tested_states = 0;
    for (int i = 0; i < 10 && num_tested_states < 200; ++i){

        PDDL_ISET(app_ops);
        pddlStripsApplicableOps(&strips, &state, &app_ops);
        if (pddlISetSize(&app_ops) == 0)
            break;

        PDDL_ISET(next_state);
        PDDL_ISET_FOR_EACH(&app_ops, op_id){
            pddlStripsOpApplyOnState(strips.op.op[op_id], &state, &next_state);
            pddl_cost_t lc = pddlLiftedHeurRelaxedEvalState(&h, &next_state, &gatoms);
            int c = pddlHAddStrips(&hadd, &next_state);
            //fprintf(stderr, "lc: %d:%d , c: %d\n", lc.cost, lc.zero_cost, c);
            assert(lc.cost == c);
            num_tested_states++;
            if (num_tested_states >= 200)
                break;
        }
        pddlISetEmpty(&state);
        pddlISetUnion(&state, &next_state);
        pddlISetFree(&next_state);
        pddlISetFree(&app_ops);
    }

    pddlISetFree(&state);
    pddlLiftedHeurRelaxedFree(&h);
    pddlHAddFree(&hadd);
    pddlGroundAtomsFree(&gatoms);
    pddlStripsFree(&strips);
}

TEST(lifted_heur_hmax_unit_cost, pddl_unit_cost)
{
    if (pddlHasNumericFluents(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }

    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.remove_static_facts = 0;
    pddl_strips_t strips;
    int ret = pddlStripsGroundDatalog(&strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    pddl_ground_atoms_t gatoms;
    pddlGroundAtomsInit(&gatoms);
    for (int fact = 0; fact < strips.fact.fact_size; ++fact){
        const pddl_ground_atom_t *ga = strips.fact.fact[fact]->ground_atom;
        pddlGroundAtomsAddPred(&gatoms, ga->pred, ga->arg, ga->arity);
    }

    pddl_hmax_t hmax;
    pddlHMaxInitStrips(&hmax, &strips);

    pddl_lifted_heur_relaxed_t h;
    pddlLiftedHMaxInit(&h, &C.pddl, 0, &C.err);

    PDDL_ISET(state);
    pddlISetUnion(&state, &strips.init);
    pddl_cost_t lc = pddlLiftedHeurRelaxedEvalState(&h, &state, &gatoms);
    int c = pddlHMaxStrips(&hmax, &state);
    assert(lc.cost == c);
    int num_tested_states = 0;
    for (int i = 0; i < 10 && num_tested_states < 200; ++i){

        PDDL_ISET(app_ops);
        pddlStripsApplicableOps(&strips, &state, &app_ops);
        if (pddlISetSize(&app_ops) == 0)
            break;

        PDDL_ISET(next_state);
        PDDL_ISET_FOR_EACH(&app_ops, op_id){
            pddlStripsOpApplyOnState(strips.op.op[op_id], &state, &next_state);
            pddl_cost_t lc = pddlLiftedHeurRelaxedEvalState(&h, &next_state, &gatoms);
            int c = pddlHMaxStrips(&hmax, &next_state);
            assert(lc.cost == c);
            num_tested_states++;
            if (num_tested_states >= 200)
                break;
        }
        pddlISetEmpty(&state);
        pddlISetUnion(&state, &next_state);
        pddlISetFree(&next_state);
        pddlISetFree(&app_ops);
    }

    pddlISetFree(&state);
    pddlLiftedHeurRelaxedFree(&h);
    pddlHMaxFree(&hmax);
    pddlGroundAtomsFree(&gatoms);
    pddlStripsFree(&strips);
}

TEST(lifted_heur_hff_add_unit_cost, pddl_unit_cost)
{
    if (pddlHasNumericFluents(&C.pddl)){
        TEST_SKIP_CHILDREN;
        return;
    }

    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.remove_static_facts = 0;
    pddl_strips_t strips;
    int ret = pddlStripsGroundDatalog(&strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    pddl_ground_atoms_t gatoms;
    pddlGroundAtomsInit(&gatoms);
    for (int fact = 0; fact < strips.fact.fact_size; ++fact){
        const pddl_ground_atom_t *ga = strips.fact.fact[fact]->ground_atom;
        assert(ga != NULL);
        pddlGroundAtomsAddPred(&gatoms, ga->pred, ga->arg, ga->arity);
    }

    pddl_lifted_heur_relaxed_t h;
    pddlLiftedHFFAddInit(&h, &C.pddl, &C.err);

    PDDL_ISET(state);
    pddlISetUnion(&state, &strips.init);
    pddlLiftedHeurRelaxedEvalState(&h, &state, &gatoms);
    int num_tested_states = 0;
    for (int i = 0; i < 10 && num_tested_states < 200; ++i){

        PDDL_ISET(app_ops);
        pddlStripsApplicableOps(&strips, &state, &app_ops);
        if (pddlISetSize(&app_ops) == 0)
            break;

        PDDL_ISET(next_state);
        PDDL_ISET_FOR_EACH(&app_ops, op_id){
            pddlStripsOpApplyOnState(strips.op.op[op_id], &state, &next_state);
            /*
            int fact_id;
            printf("State:");
            PDDL_ISET_FOR_EACH(&next_state, fact_id){
                printf(" (%s)", strips.fact.fact[fact_id]->name);
            }
            printf("\n");
            */
            pddlLiftedHeurRelaxedEvalState(&h, &next_state, &gatoms);
            num_tested_states++;
            if (num_tested_states >= 200)
                break;
        }
        pddlISetEmpty(&state);
        pddlISetUnion(&state, &next_state);
        pddlISetFree(&next_state);
        pddlISetFree(&app_ops);
    }

    pddlISetFree(&state);
    pddlLiftedHeurRelaxedFree(&h);
    pddlGroundAtomsFree(&gatoms);
    pddlStripsFree(&strips);
}
