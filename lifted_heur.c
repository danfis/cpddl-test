#include <assert.h>
#include "test.h"
#include "context.h"

TEST(lifted_heur_hadd_unit_cost, pddl_unit_cost)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.remove_static_facts = 0;
    pddl_strips_t strips;
    int ret = pddlStripsGroundDatalog(&strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    if (strips.has_cond_eff){
        pddlStripsFree(&strips);
        return;
    }

    pddl_ground_atoms_t gatoms;
    pddlGroundAtomsInit(&gatoms);
    for (int fact = 0; fact < strips.fact.fact_size; ++fact){
        const pddl_ground_atom_t *ga = strips.fact.fact[fact]->ground_atom;
        pddlGroundAtomsAddPred(&gatoms, ga->pred, ga->arg, ga->arg_size);
    }

    pddl_hadd_t hadd;
    pddlHAddInitStrips(&hadd, &strips);

    pddl_lifted_hadd_t h;
    pddlLiftedHAddInit(&h, &C.pddl, &C.err);

    PDDL_ISET(state);
    pddlISetUnion(&state, &strips.init);
    pddl_cost_t lc = pddlLiftedHAdd(&h, &state, &gatoms);
    int c = pddlHAddStrips(&hadd, &state);
    assert(lc.cost == c);
    for (int i = 0; i < 10; ++i){

        PDDL_ISET(app_ops);
        pddlStripsApplicableOps(&strips, &state, &app_ops);
        if (pddlISetSize(&app_ops) == 0)
            break;

        int op_id;
        PDDL_ISET(next_state);
        PDDL_ISET_FOR_EACH(&app_ops, op_id){
            pddlStripsOpApplyOnState(strips.op.op[op_id], &state, &next_state);
            pddl_cost_t lc = pddlLiftedHAdd(&h, &next_state, &gatoms);
            int c = pddlHAddStrips(&hadd, &next_state);
            assert(lc.cost == c);
        }
        pddlISetEmpty(&state);
        pddlISetUnion(&state, &next_state);
        pddlISetFree(&next_state);
        pddlISetFree(&app_ops);
    }

    pddlISetFree(&state);
    pddlLiftedHAddFree(&h);
    pddlHAddFree(&hadd);
    pddlGroundAtomsFree(&gatoms);
    pddlStripsFree(&strips);
}

TEST(lifted_heur_hmax_unit_cost, pddl_unit_cost)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.remove_static_facts = 0;
    pddl_strips_t strips;
    int ret = pddlStripsGroundDatalog(&strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    if (strips.has_cond_eff){
        pddlStripsFree(&strips);
        return;
    }

    pddl_ground_atoms_t gatoms;
    pddlGroundAtomsInit(&gatoms);
    for (int fact = 0; fact < strips.fact.fact_size; ++fact){
        const pddl_ground_atom_t *ga = strips.fact.fact[fact]->ground_atom;
        pddlGroundAtomsAddPred(&gatoms, ga->pred, ga->arg, ga->arg_size);
    }

    pddl_hmax_t hmax;
    pddlHMaxInitStrips(&hmax, &strips);

    pddl_lifted_hmax_t h;
    pddlLiftedHMaxInit(&h, &C.pddl, &C.err);

    PDDL_ISET(state);
    pddlISetUnion(&state, &strips.init);
    pddl_cost_t lc = pddlLiftedHMax(&h, &state, &gatoms);
    int c = pddlHMaxStrips(&hmax, &state);
    assert(lc.cost == c);
    for (int i = 0; i < 10; ++i){

        PDDL_ISET(app_ops);
        pddlStripsApplicableOps(&strips, &state, &app_ops);
        if (pddlISetSize(&app_ops) == 0)
            break;

        int op_id;
        PDDL_ISET(next_state);
        PDDL_ISET_FOR_EACH(&app_ops, op_id){
            pddlStripsOpApplyOnState(strips.op.op[op_id], &state, &next_state);
            pddl_cost_t lc = pddlLiftedHMax(&h, &next_state, &gatoms);
            int c = pddlHMaxStrips(&hmax, &next_state);
            assert(lc.cost == c);
        }
        pddlISetEmpty(&state);
        pddlISetUnion(&state, &next_state);
        pddlISetFree(&next_state);
        pddlISetFree(&app_ops);
    }

    pddlISetFree(&state);
    pddlLiftedHMaxFree(&h);
    pddlHMaxFree(&hmax);
    pddlGroundAtomsFree(&gatoms);
    pddlStripsFree(&strips);
}
