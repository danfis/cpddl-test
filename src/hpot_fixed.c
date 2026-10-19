#include "test.h"
#include "context.h"
#include <assert.h>
#include <string.h>

/*
 * Regression tests of pddlHPot() on small hard-wired IPC tasks: every
 * variant of the configuration is checked against the returned status, the
 * number of potential functions, and a lower bound on the sum of their
 * objective values.
 */

struct task {
    pddl_t pddl;
    pddl_lifted_mgroups_t lmg;
    pddl_strips_t strips;
    pddl_mgroups_t mg;
    pddl_mutex_pairs_t mutex;
    pddl_fdr_t fdr;
    pddl_mg_strips_t mg_strips;
    pddl_mutex_pairs_t fdr_mutex;
};
typedef struct task task_t;

static void loadTask(task_t *t, const char *domain_fn, const char *problem_fn)
{
    pddl_config_t pddl_cfg = PDDL_CONFIG_INIT;
    pddl_cfg.normalize = 1;
    pddl_cfg.force_adl = 1;
    int ret = pddlInit(&t->pddl, domain_fn, problem_fn, &pddl_cfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    pddl_lifted_mgroups_infer_limits_t infer_limit
                = PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT;
    pddlLiftedMGroupsInit(&t->lmg);
    pddlLiftedMGroupsInferFAMGroups(&t->pddl, &infer_limit, &t->lmg, &C.err);
    pddlLiftedMGroupsSetExactlyOne(&t->pddl, &t->lmg, &C.err);
    pddlLiftedMGroupsSetStatic(&t->pddl, &t->lmg, &C.err);

    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ret = pddlStripsGroundDatalog(&t->strips, &t->pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    assert(!t->strips.has_cond_eff);

    pddlMGroupsGround(&t->mg, &t->pddl, &t->lmg, &t->strips);
    pddlMGroupsSetExactlyOne(&t->mg, &t->strips);
    pddlMGroupsSetGoal(&t->mg, &t->strips);

    pddlMutexPairsInitStrips(&t->mutex, &t->strips);
    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    ret = pddlH2FwBwMutexFactsOps(&t->strips, &t->mg, &t->mutex, &rm_fact,
                                  &rm_op, &C.err);
    assert(ret == 0);
    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0){
        pddlStripsReduce(&t->strips, &rm_fact, &rm_op);
        if (pddlISetSize(&rm_fact) > 0){
            pddlMGroupsReduce(&t->mg, &rm_fact);
            pddlMGroupsSetExactlyOne(&t->mg, &t->strips);
            pddlMGroupsSetGoal(&t->mg, &t->strips);
            pddlMutexPairsReduce(&t->mutex, &rm_fact);
        }
    }
    pddlISetFree(&rm_fact);
    pddlISetFree(&rm_op);
    pddlStripsRemoveUselessDelEffs(&t->strips, &t->mutex, NULL, &C.err);
    assert(t->strips.op.op_size > 0);

    pddl_mutex_pairs_t mg_mutex;
    pddlMutexPairsInit(&mg_mutex, t->strips.fact.fact_size);
    pddlMutexPairsAddMGroups(&mg_mutex, &t->mg);
    pddl_fdr_config_t fdr_cfg = PDDL_FDR_CONFIG_INIT;
    fdr_cfg.var.alg = PDDL_FDR_VARS_ALG_LARGEST_FIRST;
    ret = pddlFDRInitFromStrips(&t->fdr, &t->strips, &t->mg, &mg_mutex,
                                &fdr_cfg, &C.err);
    assert(ret == 0);
    pddlMutexPairsFree(&mg_mutex);

    // Inputs of pddlHPot() prepared the same way as in the hpot test
    pddlMGStripsInitFDR(&t->mg_strips, &t->fdr);
    pddlMutexPairsInitStrips(&t->fdr_mutex, &t->mg_strips.strips);
    pddlMutexPairsAddMGroups(&t->fdr_mutex, &t->mg_strips.mg);
    pddlH2FwMutex(&t->mg_strips.strips, &t->fdr_mutex, &C.err);
    pddlStripsDisambiguatePres(&t->mg_strips.strips, &t->fdr_mutex,
                               &t->mg_strips.mg, NULL, NULL, &C.err);
    pddlStripsRemoveUselessDelEffs(&t->mg_strips.strips, &t->fdr_mutex,
                                   NULL, &C.err);
}

static void freeTask(task_t *t)
{
    pddlMutexPairsFree(&t->fdr_mutex);
    pddlMGStripsFree(&t->mg_strips);
    pddlFDRFree(&t->fdr);
    pddlMutexPairsFree(&t->mutex);
    pddlMGroupsFree(&t->mg);
    pddlStripsFree(&t->strips);
    pddlLiftedMGroupsFree(&t->lmg);
    pddlFree(&t->pddl);
}

/** Sets up one variant of the configuration; CFG has the task inputs set,
 *  disambiguation on, and .opt pointing to OPT (two elements initialized
 *  with PDDL_HPOT_CONFIG_OPT_INIT) with .opt_size = 1. */
typedef void (*variant_set_fn)(pddl_hpot_config_t *cfg,
                               pddl_hpot_config_opt_t *opt,
                               const pddl_fdr_t *fdr);

/** What is checked besides the returned status */
enum check {
    /** Only that at least one function was found, because the result
     *  depends on which of the optimal solutions the LP solver returns */
    CHECK_STATUS,
    /** The number of functions and a lower bound on the sum of their
     *  objective values */
    CHECK_OBJVAL,
};
typedef enum check check_t;

struct variant {
    const char *name;
    variant_set_fn set;
    check_t check;
};
typedef struct variant variant_t;

struct expected {
    const char *name;
    pddl_hpot_status_t status;
    int sol_size;
    /** Sum of the objective values rounded down */
    long objval_sum;
};
typedef struct expected expected_t;

static void vI(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
               const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_STATE_TYPE;
}

static void vIHMax0(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                    const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_STATE_TYPE;
    opt->hmax0 = pddl_true;
}

static void vICustom(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                     const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_STATE_TYPE;
    opt->state.type = PDDL_HPOT_STATE_CUSTOM;
    opt->state.fdr_state = fdr->init;
}

static void vIOpPotInt(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                       const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_STATE_TYPE;
    cfg->op_pot = PDDL_HPOT_OP_POT_INT;
}

static void vIOpPotReal(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                        const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_STATE_TYPE;
    cfg->op_pot = PDDL_HPOT_OP_POT_REAL;
}

static void vINoDisamb(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                       const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_STATE_TYPE;
    cfg->disambiguation = pddl_false;
}

static void vA(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
               const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
}

static void vAHMax0(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                    const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    opt->hmax0 = pddl_true;
}

static void vAI(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    opt->add_state_constr.state.type = PDDL_HPOT_STATE_INIT;
}

static void vAHMax0I(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                     const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    opt->hmax0 = pddl_true;
    opt->add_state_constr.state.type = PDDL_HPOT_STATE_INIT;
    opt->add_state_constr.ignore_hmax0 = pddl_true;
}

static void vAIHMax0(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                     const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    opt->hmax0 = pddl_true;
    opt->add_state_constr.state.type = PDDL_HPOT_STATE_INIT;
}

static void vAICoef(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                    const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    opt->add_state_constr.state.type = PDDL_HPOT_STATE_INIT;
    opt->add_state_constr.coef = 0.5;
}

static void vAICustom(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                      const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    opt->add_state_constr.state.type = PDDL_HPOT_STATE_CUSTOM;
    opt->add_state_constr.state.fdr_state = fdr->init;
}

static void vAINoDisamb(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                        const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    opt->add_state_constr.state.type = PDDL_HPOT_STATE_INIT;
    cfg->disambiguation = pddl_false;
}

static void vS(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
               const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_SAMPLED_STATES_TYPE;
    opt->num_samples = 50;
}

static void vSSyntactic(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                        const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_SAMPLED_STATES_TYPE;
    opt->num_samples = 50;
    opt->sampler.type = PDDL_HPOT_SAMPLER_SYNTACTIC;
}

static void vSMutex(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                    const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_SAMPLED_STATES_TYPE;
    opt->num_samples = 50;
    opt->sampler.type = PDDL_HPOT_SAMPLER_SYNTACTIC_MUTEX;
}

static void vSI(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_SAMPLED_STATES_TYPE;
    opt->num_samples = 50;
    opt->add_state_constr.state.type = PDDL_HPOT_STATE_INIT;
}

static void vM1(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE;
    opt->all_states_mutex.mutex_size = 1;
}

static void vM2(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE;
    opt->all_states_mutex.mutex_size = 2;
}

static void vMI1(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                 const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE;
    opt->all_states_mutex.mutex_size = 1;
    opt->add_state_constr.state.type = PDDL_HPOT_STATE_INIT;
}

static void vMI2(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                 const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE;
    opt->all_states_mutex.mutex_size = 2;
    opt->add_state_constr.state.type = PDDL_HPOT_STATE_INIT;
}

static void vM2NoDisamb(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                        const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ALL_STATES_MUTEX_TYPE;
    opt->all_states_mutex.mutex_size = 2;
    cfg->disambiguation = pddl_false;
}

static void vES(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE;
    opt->num_samples = 10;
}

static void vESAll(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                   const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE;
    opt->num_samples = 10;
    opt->ensemble_sampled_states.opt_all_states = pddl_true;
}

static void vESSyntactic(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                         const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_SAMPLED_STATES_TYPE;
    opt->num_samples = 10;
    opt->sampler.type = PDDL_HPOT_SAMPLER_SYNTACTIC;
}

static void vED(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_DIVERSIFICATION_TYPE;
    opt->num_samples = 10;
}

static void vEMCond1(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                     const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE;
    opt->ensemble_all_states_mutex.cond_size = 1;
    opt->ensemble_all_states_mutex.mutex_size = 1;
}

static void vEMCond2(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                     const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE;
    opt->ensemble_all_states_mutex.cond_size = 1;
    opt->ensemble_all_states_mutex.mutex_size = 2;
}

static void vEMCondRand(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                        const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE;
    opt->ensemble_all_states_mutex.cond_size = 1;
    opt->ensemble_all_states_mutex.mutex_size = 1;
    opt->ensemble_all_states_mutex.num_rand_samples = 10;
}

static void vEMCondRand2(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                         const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE;
    opt->ensemble_all_states_mutex.cond_size = 1;
    opt->ensemble_all_states_mutex.mutex_size = 2;
    opt->ensemble_all_states_mutex.num_rand_samples = 10;
}

static void vEMCond2Rand(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                         const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE;
    opt->ensemble_all_states_mutex.cond_size = 2;
    opt->ensemble_all_states_mutex.mutex_size = 1;
    opt->ensemble_all_states_mutex.num_rand_samples = 10;
}

static void vEMCond2Rand2(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                          const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_ENSEMBLE_ALL_STATES_MUTEX_TYPE;
    opt->ensemble_all_states_mutex.cond_size = 2;
    opt->ensemble_all_states_mutex.mutex_size = 2;
    opt->ensemble_all_states_mutex.num_rand_samples = 10;
}

static void vGC(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_GOAL_COUNT_TYPE;
}

static void vGCNoDisamb(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                        const pddl_fdr_t *fdr)
{
    opt->type = PDDL_HPOT_OPT_GOAL_COUNT_TYPE;
    cfg->disambiguation = pddl_false;
}

static void vMultiIA(pddl_hpot_config_t *cfg, pddl_hpot_config_opt_t *opt,
                     const pddl_fdr_t *fdr)
{
    opt[0].type = PDDL_HPOT_OPT_STATE_TYPE;
    opt[1].type = PDDL_HPOT_OPT_ALL_SYNTACTIC_STATES_TYPE;
    cfg->opt_size = 2;
}

static const variant_t variants[] = {
    { "I", vI, CHECK_OBJVAL },
    { "I-hmax0", vIHMax0, CHECK_OBJVAL },
    { "I-custom", vICustom, CHECK_OBJVAL },
    { "I-op-pot-int", vIOpPotInt, CHECK_OBJVAL },
    { "I-op-pot-real", vIOpPotReal, CHECK_OBJVAL },
    { "I-no-disamb", vINoDisamb, CHECK_OBJVAL },
    { "A", vA, CHECK_OBJVAL },
    { "A-hmax0", vAHMax0, CHECK_OBJVAL },
    { "A+I", vAI, CHECK_OBJVAL },
    { "A-hmax0+I", vAHMax0I, CHECK_OBJVAL },
    { "A+I-hmax0", vAIHMax0, CHECK_OBJVAL },
    { "A+I-coef", vAICoef, CHECK_OBJVAL },
    { "A+I-custom", vAICustom, CHECK_OBJVAL },
    { "A+I-no-disamb", vAINoDisamb, CHECK_OBJVAL },
    { "S", vS, CHECK_OBJVAL },
    { "S-syntactic", vSSyntactic, CHECK_OBJVAL },
    { "S-mutex", vSMutex, CHECK_OBJVAL },
    { "S+I", vSI, CHECK_OBJVAL },
    { "M1", vM1, CHECK_OBJVAL },
    { "M2", vM2, CHECK_OBJVAL },
    { "M+I1", vMI1, CHECK_OBJVAL },
    { "M+I2", vMI2, CHECK_OBJVAL },
    { "M2-no-disamb", vM2NoDisamb, CHECK_OBJVAL },
    { "ES", vES, CHECK_OBJVAL },
    { "ES-all", vESAll, CHECK_OBJVAL },
    { "ES-syntactic", vESSyntactic, CHECK_OBJVAL },
    { "ED", vED, CHECK_STATUS },
    { "EM-cond1", vEMCond1, CHECK_OBJVAL },
    { "EM-cond2", vEMCond2, CHECK_OBJVAL },
    { "EM-cond-rand", vEMCondRand, CHECK_OBJVAL },
    { "EM-cond-rand2", vEMCondRand2, CHECK_OBJVAL },
    { "EM-cond2-rand", vEMCond2Rand, CHECK_OBJVAL },
    { "EM-cond2-rand2", vEMCond2Rand2, CHECK_OBJVAL },
    { "gc", vGC, CHECK_OBJVAL },
    { "gc-no-disamb", vGCNoDisamb, CHECK_OBJVAL },
    { "I,A", vMultiIA, CHECK_OBJVAL },
};
#define VARIANTS_SIZE ((int)(sizeof(variants) / sizeof(variants[0])))
#define EXPECTED_SIZE(e) ((int)(sizeof(e) / sizeof(expected_t)))
/** Tolerance of the lower bound on the sum of objective values */
#define OBJVAL_EPS 1E-3

static const expected_t expected_gripper[] = {
    { "I", PDDL_HPOT_FOUND, 1, 8 },
    { "I-hmax0", PDDL_HPOT_FOUND, 1, 8 },
    { "I-custom", PDDL_HPOT_FOUND, 1, 8 },
    { "I-op-pot-int", PDDL_HPOT_FOUND, 1, 8 },
    { "I-op-pot-real", PDDL_HPOT_FOUND, 1, 8 },
    { "I-no-disamb", PDDL_HPOT_FOUND, 1, 8 },
    { "A", PDDL_HPOT_FOUND, 1, 9333336 },
    { "A-hmax0", PDDL_HPOT_FOUND, 1, 9333336 },
    { "A+I", PDDL_HPOT_FOUND, 1, 9333336 },
    { "A-hmax0+I", PDDL_HPOT_FOUND, 1, 9333336 },
    { "A+I-hmax0", PDDL_HPOT_FOUND, 1, 9333336 },
    { "A+I-coef", PDDL_HPOT_FOUND, 1, 9333336 },
    { "A+I-custom", PDDL_HPOT_FOUND, 1, 9333336 },
    { "A+I-no-disamb", PDDL_HPOT_FOUND, 1, 3 },
    { "S", PDDL_HPOT_FOUND, 1, 249 },
    { "S-syntactic", PDDL_HPOT_FOUND, 1, 630000135 },
    { "S-mutex", PDDL_HPOT_FOUND, 1, 1120000137 },
    { "S+I", PDDL_HPOT_FOUND, 1, 249 },
    { "M1", PDDL_HPOT_FOUND, 1, 12637241 },
    { "M2", PDDL_HPOT_FOUND, 1, 13852083 },
    { "M+I1", PDDL_HPOT_FOUND, 1, 12637241 },
    { "M+I2", PDDL_HPOT_FOUND, 1, 13852083 },
    { "M2-no-disamb", PDDL_HPOT_FOUND, 1, 13852083 },
    { "ES", PDDL_HPOT_FOUND, 10, 47 },
    { "ES-all", PDDL_HPOT_FOUND, 10, 93333362 },
    { "ES-syntactic", PDDL_HPOT_FOUND, 10, 665000003 },
    { "ED", PDDL_HPOT_FOUND, 1, 47 },
    { "EM-cond1", PDDL_HPOT_FOUND, 24, 401009087 },
    { "EM-cond2", PDDL_HPOT_FOUND, 24, 424638762 },
    { "EM-cond-rand", PDDL_HPOT_FOUND, 8, 145061862 },
    { "EM-cond-rand2", PDDL_HPOT_FOUND, 8, 152964182 },
    { "EM-cond2-rand", PDDL_HPOT_FOUND, 7, 168588669 },
    { "EM-cond2-rand2", PDDL_HPOT_FOUND, 7, 173801612 },
    { "gc", PDDL_HPOT_FOUND, 1, 0 },
    { "gc-no-disamb", PDDL_HPOT_FOUND, 1, 0 },
    { "I,A", PDDL_HPOT_FOUND, 2, 9333344 },
};

static const expected_t expected_depot[] = {
    { "I", PDDL_HPOT_FOUND, 1, 8 },
    { "I-hmax0", PDDL_HPOT_FOUND, 1, 8 },
    { "I-custom", PDDL_HPOT_FOUND, 1, 8 },
    { "I-op-pot-int", PDDL_HPOT_FOUND, 1, 8 },
    { "I-op-pot-real", PDDL_HPOT_FOUND, 1, 8 },
    { "I-no-disamb", PDDL_HPOT_FOUND, 1, 8 },
    { "A", PDDL_HPOT_FOUND, 1, 42777780 },
    { "A-hmax0", PDDL_HPOT_FOUND, 1, 42777780 },
    { "A+I", PDDL_HPOT_FOUND, 1, 42777780 },
    { "A-hmax0+I", PDDL_HPOT_FOUND, 1, 42777780 },
    { "A+I-hmax0", PDDL_HPOT_FOUND, 1, 42777780 },
    { "A+I-coef", PDDL_HPOT_FOUND, 1, 42777780 },
    { "A+I-custom", PDDL_HPOT_FOUND, 1, 42777780 },
    { "A+I-no-disamb", PDDL_HPOT_FOUND, 1, 38888891 },
    { "S", PDDL_HPOT_FOUND, 1, 311 },
    { "S-syntactic", PDDL_HPOT_FOUND, 1, 2240000128 },
    { "S-mutex", PDDL_HPOT_FOUND, 1, 2800000111 },
    { "S+I", PDDL_HPOT_FOUND, 1, 311 },
    { "M1", PDDL_HPOT_FOUND, 1, 36629173 },
    { "M2", PDDL_HPOT_FOUND, 1, 37479196 },
    { "M+I1", PDDL_HPOT_FOUND, 1, 36629173 },
    { "M+I2", PDDL_HPOT_FOUND, 1, 37479196 },
    { "M2-no-disamb", PDDL_HPOT_FOUND, 1, 37479196 },
    { "ES", PDDL_HPOT_FOUND, 10, 64 },
    { "ES-all", PDDL_HPOT_FOUND, 10, 427777806 },
    { "ES-syntactic", PDDL_HPOT_FOUND, 10, 1400000000 },
    { "ED", PDDL_HPOT_FOUND, 1, 64 },
    { "EM-cond1", PDDL_HPOT_FOUND, 48, 2194120300 },
    { "EM-cond2", PDDL_HPOT_FOUND, 48, 2212584148 },
    { "EM-cond-rand", PDDL_HPOT_FOUND, 9, 393237531 },
    { "EM-cond-rand2", PDDL_HPOT_FOUND, 9, 395186009 },
    { "EM-cond2-rand", PDDL_HPOT_FOUND, 8, 386918439 },
    { "EM-cond2-rand2", PDDL_HPOT_FOUND, 8, 386613959 },
    { "gc", PDDL_HPOT_FOUND, 1, 0 },
    { "gc-no-disamb", PDDL_HPOT_FOUND, 1, 0 },
    { "I,A", PDDL_HPOT_FOUND, 2, 42777788 },
};

static const expected_t expected_satellite[] = {
    { "I", PDDL_HPOT_FOUND, 1, 3 },
    { "I-hmax0", PDDL_HPOT_FOUND, 1, 4 },
    { "I-custom", PDDL_HPOT_FOUND, 1, 3 },
    { "I-op-pot-int", PDDL_HPOT_FOUND, 1, 3 },
    { "I-op-pot-real", PDDL_HPOT_FOUND, 1, 3 },
    { "I-no-disamb", PDDL_HPOT_FOUND, 1, 3 },
    { "A", PDDL_HPOT_FOUND, 1, 1 },
    { "A-hmax0", PDDL_HPOT_FOUND, 1, 1 },
    { "A+I", PDDL_HPOT_FOUND, 1, 1 },
    { "A-hmax0+I", PDDL_HPOT_FOUND, 1, 1 },
    { "A+I-hmax0", PDDL_HPOT_FOUND, 1, 0 },
    { "A+I-coef", PDDL_HPOT_FOUND, 1, 1 },
    { "A+I-custom", PDDL_HPOT_FOUND, 1, 1 },
    { "A+I-no-disamb", PDDL_HPOT_FOUND, 1, 1 },
    { "S", PDDL_HPOT_FOUND, 1, 150 },
    { "S-syntactic", PDDL_HPOT_FOUND, 1, 76 },
    { "S-mutex", PDDL_HPOT_FOUND, 1, 76 },
    { "S+I", PDDL_HPOT_FOUND, 1, 150 },
    { "M1", PDDL_HPOT_FOUND, 1, 1 },
    { "M2", PDDL_HPOT_FOUND, 1, 1 },
    { "M+I1", PDDL_HPOT_FOUND, 1, 1 },
    { "M+I2", PDDL_HPOT_FOUND, 1, 1 },
    { "M2-no-disamb", PDDL_HPOT_FOUND, 1, 1 },
    { "ES", PDDL_HPOT_FOUND, 10, 30 },
    { "ES-all", PDDL_HPOT_FOUND, 10, 15 },
    { "ES-syntactic", PDDL_HPOT_FOUND, 10, 16 },
    { "ED", PDDL_HPOT_FOUND, 1, 15 },
    { "EM-cond1", PDDL_HPOT_FOUND, 25, 37 },
    { "EM-cond2", PDDL_HPOT_FOUND, 25, 37 },
    { "EM-cond-rand", PDDL_HPOT_FOUND, 8, 11 },
    { "EM-cond-rand2", PDDL_HPOT_FOUND, 8, 11 },
    { "EM-cond2-rand", PDDL_HPOT_FOUND, 8, 12 },
    { "EM-cond2-rand2", PDDL_HPOT_FOUND, 8, 12 },
    { "gc", PDDL_HPOT_FOUND, 1, 0 },
    { "gc-no-disamb", PDDL_HPOT_FOUND, 1, 0 },
    { "I,A", PDDL_HPOT_FOUND, 2, 4 },
};

static void checkValues(const char *name,
                        const pddl_pot_solutions_t *sols,
                        const expected_t *e)
{
    if (sols->sol_size != e->sol_size){
        fprintf(stderr, "%s: %d functions, expected %d\n",
                name, sols->sol_size, e->sol_size);
    }
    assert(sols->sol_size == e->sol_size);

    double sum = 0.;
    for (int si = 0; si < sols->sol_size; ++si)
        sum += sols->sol[si].objval;
    if (sum < (double)e->objval_sum - OBJVAL_EPS){
        fprintf(stderr, "%s: sum of objvals %.6f, expected at least %ld\n",
                name, sum, e->objval_sum);
    }
    assert(sum >= (double)e->objval_sum - OBJVAL_EPS);
}

static void runVariants(const char *domain_fn, const char *problem_fn,
                        const expected_t *expected, int expected_size)
{
    assert(expected_size == VARIANTS_SIZE);
    task_t task;
    loadTask(&task, domain_fn, problem_fn);

    for (int vi = 0; vi < VARIANTS_SIZE; ++vi){
        const variant_t *v = variants + vi;

        pddl_hpot_config_opt_t opt[2] = { PDDL_HPOT_CONFIG_OPT_INIT,
                                          PDDL_HPOT_CONFIG_OPT_INIT };
        pddl_hpot_config_t cfg = PDDL_HPOT_CONFIG_INIT;
        cfg.fdr = &task.fdr;
        cfg.mg_strips = &task.mg_strips;
        cfg.mutex = &task.fdr_mutex;
        cfg.disambiguation = pddl_true;
        cfg.opt = opt;
        cfg.opt_size = 1;
        v->set(&cfg, opt, &task.fdr);

        pddl_pot_solutions_t sols;
        pddlPotSolutionsInit(&sols);
        pddl_hpot_status_t st = pddlHPot(&sols, &cfg, &C.err);
        if (st == PDDL_HPOT_ERR)
            pddlErrPrint(&C.err, 1, stderr);

        const expected_t *e = expected + vi;
        assert(strcmp(e->name, v->name) == 0);
        if (st != e->status){
            fprintf(stderr, "%s: status %d, expected %d\n",
                    v->name, (int)st, (int)e->status);
        }
        assert(st == e->status);

        if (v->check == CHECK_STATUS){
            assert(sols.sol_size > 0);
        }else{
            checkValues(v->name, &sols, e);
        }

        pddlPotSolutionsFree(&sols);
    }

    freeTask(&task);
}

/*
 * Parent of the per-task tests below; each of them runs all variants on one
 * small IPC task and checks the results against the hard-wired values.
 */
TEST_ONCE(hpot_fixed)
{
}

/*
 * All variants of pddlHPot() on ipc-1998/gripper/prob01.
 */
TEST_COND(hpot_fixed_gripper, hpot_fixed, LP)
{
    runVariants("pddl/ipc-1998/gripper/domain.pddl",
                "pddl/ipc-1998/gripper/prob01.pddl", expected_gripper,
                EXPECTED_SIZE(expected_gripper));
}

/*
 * All variants of pddlHPot() on ipc-2002/depot/pfile1.
 */
TEST_COND(hpot_fixed_depot, hpot_fixed, LP)
{
    runVariants("pddl/ipc-2002/depot/domain.pddl",
                "pddl/ipc-2002/depot/pfile1.pddl", expected_depot,
                EXPECTED_SIZE(expected_depot));
}

/*
 * All variants of pddlHPot() on ipc-2004/satellite/p01-pfile1.
 */
TEST_COND(hpot_fixed_satellite, hpot_fixed, LP)
{
    runVariants("pddl/ipc-2004/satellite/domain.pddl",
                "pddl/ipc-2004/satellite/p01-pfile1.pddl", expected_satellite,
                EXPECTED_SIZE(expected_satellite));
}
