#include <assert.h>
#include "test.h"
#include "context.h"

static void checkOpChangeState(const pddl_fdr_t *fdr,
                               const int *state,
                               const pddl_pot_solution_t *pot,
                               int depth)
{
    size_t state_size = sizeof(int) * fdr->var.var_size;
    double heur = pddlPotSolutionEvalFDRStateFlt(pot, &fdr->var, state);

    for (int opi = 0; opi < fdr->op.op_size; ++opi){
        const pddl_fdr_op_t *op = fdr->op.op[opi];
        if (pddlFDROpIsApplicable(op, state)){
            int *state2 = alloca(state_size);
            pddlFDROpApplyOnState(op, fdr->var.var_size, state, state2);
            double heur2;
            heur2 = pddlPotSolutionEvalFDRStateFlt(pot, &fdr->var, state2);
            double heur_op = heur + pot->op_pot[opi];
            assert((int)(heur - heur_op) <= op->cost);
            assert(heur_op <= (heur2 + 1E-4));

            if (depth > 0){
                checkOpChangeState(fdr, state2, pot, depth - 1);
            }
        }
    }

}

static void checkOpChange(const pddl_fdr_t *fdr, const pddl_pot_solution_t *pot)
{
    size_t state_size = sizeof(int) * fdr->var.var_size;
    int *state = alloca(state_size);
    memcpy(state, fdr->init, state_size);
    checkOpChangeState(fdr, state, pot, 2);
}

TEST_COND(pot, fdr, LP)
{
}

static int pot_value_init = -1;

TEST(pot_init, pot)
{
    pddl_pot_t pot;
    pddlPotInitFDR(&pot, &C.fdr);
    pddlPotEnableOpPot(&pot, 1, 0);
    pddlPotSetObjFDRState(&pot, &C.fdr.var, C.fdr.init);

    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    int ret = pddlPotSolve(&pot, &sol);
    assert(ret == 0);
    checkOpChange(&C.fdr, &sol);

    int val = pddlPotSolutionEvalFDRState(&sol, &C.fdr.var, C.fdr.init);
    pot_value_init = val;
    assert(val <= C.optimal_cost);
    fprintf(stdout, "Value: %d\n", val);
    pddlPotSolutionFree(&sol);
    pddlPotFree(&pot);
}

TEST(pot_all_states, pot_init)
{
    pddl_pot_t pot;
    pddlPotInitFDR(&pot, &C.fdr);
    pddlPotEnableOpPot(&pot, 1, 0);
    pddlPotSetObjFDRAllSyntacticStates(&pot, &C.fdr.var);

    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    int ret = pddlPotSolve(&pot, &sol);
    assert(ret == 0);
    checkOpChange(&C.fdr, &sol);

    int val = pddlPotSolutionEvalFDRState(&sol, &C.fdr.var, C.fdr.init);
    assert(val <= pot_value_init);
    assert(val <= C.optimal_cost);
    fprintf(stdout, "Value: %d\n", val);
    pddlPotSolutionFree(&sol);
    pddlPotFree(&pot);
}

static int pot_value_mg_strips_init = -1;
TEST(pot_mg_strips_init, pot_init)
{
    pddl_mg_strips_t mg_strips;
    pddlMGStripsInitFDR(&mg_strips, &C.fdr);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);
    pddlMutexPairsAddMGroups(&mutex, &mg_strips.mg);

    pddl_pot_t pot;
    pddlPotInitMGStrips(&pot, &mg_strips, &mutex);
    pddlPotSetObjStripsState(&pot, &mg_strips.strips.init);

    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    pddlPotSolve(&pot, &sol);
    int val = pddlPotSolutionEvalStripsState(&sol, &mg_strips.strips.init);
    assert(val >= pot_value_init);
    assert(val <= C.optimal_cost);
    pot_value_mg_strips_init = val;
    fprintf(stdout, "Value: %d\n", val);
    pddlPotSolutionFree(&sol);
    pddlPotFree(&pot);
    pddlMutexPairsFree(&mutex);
    pddlMGStripsFree(&mg_strips);
}

TEST(pot_mg_strips_mutex_init, pot_mg_strips_init)
{
    pddl_mg_strips_t mg_strips;
    pddlMGStripsInitFDR(&mg_strips, &C.fdr);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);
    pddlMutexPairsAddMGroups(&mutex, &mg_strips.mg);
    pddlH2(&mg_strips.strips, &mutex, NULL, NULL, 0., &C.err);

    pddl_pot_t pot;
    pddlPotInitMGStrips(&pot, &mg_strips, &mutex);
    pddlPotSetObjStripsState(&pot, &mg_strips.strips.init);

    pddl_pot_solution_t sol;
    pddlPotSolutionInit(&sol);
    pddlPotSolve(&pot, &sol);
    int val = pddlPotSolutionEvalStripsState(&sol, &mg_strips.strips.init);
    assert(val >= pot_value_init);
    assert(val >= pot_value_mg_strips_init);
    assert(val <= C.optimal_cost);
    fprintf(stdout, "Value: %d\n", val);
    pddlPotSolutionFree(&sol);
    pddlPotFree(&pot);
    pddlMutexPairsFree(&mutex);
    pddlMGStripsFree(&mg_strips);
}
