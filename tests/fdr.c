#include "test.h"
#include "context.h"
#include <assert.h>


static void _test_fdr(const pddl_fdr_config_t *cfg)
{
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInit(&mutex, C.strips.fact.fact_size);
    pddlMutexPairsAddMGroups(&mutex, &C.mg);

    int ret = pddlFDRInitFromStrips(&C.fdr, &C.strips, &C.mg, &mutex,
                                    cfg, &C.err);
    assert(ret == 0);
    C.fdr_set = 1;
    pddlFDRPrintFD(&C.fdr, &C.mg, 0, stdout);
    pddlMutexPairsFree(&mutex);
}

TEST(fdr_largest, strips_pruned)
{
    pddl_fdr_config_t cfg = PDDL_FDR_CONFIG_INIT;
    cfg.var.alg = PDDL_FDR_VARS_ALG_LARGEST_FIRST;
    _test_fdr(&cfg);
}

TEST(fdr_essential, strips_pruned)
{
    pddl_fdr_config_t cfg = PDDL_FDR_CONFIG_INIT;
    cfg.var.alg = PDDL_FDR_VARS_ALG_ESSENTIAL_FIRST;
    _test_fdr(&cfg);
}

TEST(fdr_largest_multi, strips_pruned)
{
    pddl_fdr_config_t cfg = PDDL_FDR_CONFIG_INIT;
    cfg.var.alg = PDDL_FDR_VARS_ALG_LARGEST_FIRST_MULTI;
    _test_fdr(&cfg);
}

TEST(fdr_h2, strips_pruned)
{
    pddl_fdr_config_t cfg = PDDL_FDR_CONFIG_INIT;
    cfg.var.alg = PDDL_FDR_VARS_ALG_LARGEST_FIRST;

    pddlFDRInitFromStrips(&C.fdr, &C.strips, &C.mg, &C.mutex, &cfg, &C.err);
    C.fdr_set = 1;

    PDDL_ISET(unreach_op);
    PDDL_ISET(unreach_fact);
    assert(C.mutex_set);
    pddl_mg_strips_t mg_strips;
    pddlMGStripsInitFDR(&mg_strips, &C.fdr);
    pddlMutexPairsFree(&C.mutex);
    pddlMutexPairsInitStrips(&C.mutex, &mg_strips.strips);
    pddlMutexPairsAddMGroups(&C.mutex, &mg_strips.mg);
    pddlH2(&mg_strips.strips, &C.mutex, &unreach_fact, &unreach_op, 0., &C.err);
    pddlMGStripsFree(&mg_strips);

    pddlFDRReduce(&C.fdr, NULL, &unreach_fact, &unreach_op);
    if (pddlISetSize(&unreach_fact) > 0){
        pddl_mg_strips_t mg_strips;
        pddlMGStripsInitFDR(&mg_strips, &C.fdr);
        pddlMutexPairsFree(&C.mutex);
        pddlMutexPairsInitStrips(&C.mutex, &mg_strips.strips);
        pddlMutexPairsAddMGroups(&C.mutex, &mg_strips.mg);
        pddlH2(&mg_strips.strips, &C.mutex, NULL, NULL, 0., &C.err);
        pddlMGStripsFree(&mg_strips);
    }

    pddlISetFree(&unreach_fact);
    pddlISetFree(&unreach_op);
}


static void findOps(const pddl_fdr_app_op_t *app_op,
                    const pddl_fdr_ops_t *ops,
                    const int *state,
                    int depth)
{
    PDDL_ISET(app);
    int ret = pddlFDRAppOpFind(app_op, state, &app);
    assert(ret == pddlISetSize(&app));

    /*
    printf("Init: %d:", ret);
    int op_id;
    PDDL_ISET_FOR_EACH(&app, op_id)
        printf(" %d", op_id);
    printf("\n");
    */

    PDDL_ISET(app2);
    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        const pddl_fdr_op_t *op = ops->op[op_id];
        int applicable = 1;
        for (int fi = 0; fi < op->pre.fact_size; ++fi){
            const pddl_fdr_fact_t *fact = op->pre.fact + fi;
            if (state[fact->var] != fact->val){
                applicable = 0;
                break;
            }
        }

        if (applicable)
            pddlISetAdd(&app2, op_id);
    }

    assert(pddlISetEq(&app, &app2));

    if (depth > 0){
        int *next_state = PDDL_ALLOC_ARR(int, app_op->var_size);
        int op_id;
        PDDL_ISET_FOR_EACH(&app, op_id){
            memcpy(next_state, state, sizeof(int) * app_op->var_size);

            const pddl_fdr_op_t *op = ops->op[op_id];
            for (int fi = 0; fi < op->eff.fact_size; ++fi)
                next_state[op->eff.fact[fi].var] = op->eff.fact[fi].val;
            findOps(app_op, ops, next_state, depth - 1);
        }
        PDDL_FREE(next_state);
    }

    pddlISetFree(&app2);
    pddlISetFree(&app);
}

static void findOpsRand(const pddl_fdr_app_op_t *app_op,
                        const pddl_fdr_vars_t *vars,
                        const pddl_fdr_ops_t *ops,
                        int num_samples)
{
    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);

    int *state = PDDL_ALLOC_ARR(int, vars->var_size);
    for (int sample = 0; sample < num_samples; ++sample){
        for (int var = 0; var < vars->var_size; ++var){
            int val = pddlRand(&rnd, 0, vars->var[var].val_size);
            val = PDDL_MIN(val, vars->var[var].val_size - 1);
            state[var] = val;
        }
        findOps(app_op, ops, state, 0);
    }
    PDDL_FREE(state);
}

TEST(fdr_app_op, fdr_largest)
{
    pddlFDRAppOpInit(&C.fdr_app_op, &C.fdr.var, &C.fdr.op, &C.fdr.goal);
    C.fdr_app_op_set = 1;
}

TEST(fdr_app_op_search, fdr_app_op)
{
    findOps(&C.fdr_app_op, &C.fdr.op, C.fdr.init, 2);
}

TEST(fdr_app_op_rand, fdr_app_op)
{
    findOpsRand(&C.fdr_app_op, &C.fdr.var, &C.fdr.op, 5000);
}

TEST(fdr, fdr_app_op)
{
}

TEST(fdr_app_op_essential, fdr_essential)
{
    pddlFDRAppOpInit(&C.fdr_app_op, &C.fdr.var, &C.fdr.op, &C.fdr.goal);
    C.fdr_app_op_set = 1;
}

TEST(fdr_app_op_search_essential, fdr_app_op_essential)
{
    findOps(&C.fdr_app_op, &C.fdr.op, C.fdr.init, 2);
}

TEST(fdr_app_op_rand_essential, fdr_app_op_essential)
{
    findOpsRand(&C.fdr_app_op, &C.fdr.var, &C.fdr.op, 5000);
}


static void _tnf_flow_eq(const pddl_fdr_t *fdr, const pddl_fdr_t *tnf)
{
    pddl_hflow_t fdr_flow;
    pddlHFlowInit(&fdr_flow, fdr, 0);

    pddl_hflow_t tnf_flow;
    pddlHFlowInit(&tnf_flow, tnf, 0);

    int fdr_cost = pddlHFlow(&fdr_flow, fdr->init, NULL);
    int tnf_cost = pddlHFlow(&tnf_flow, tnf->init, NULL);
    assert(fdr_cost == tnf_cost);
    if (fdr_cost != tnf_cost)
        fprintf(stderr, "Flow failed. fdr: %d tnf: %d\n", fdr_cost, tnf_cost);

    pddlHFlowFree(&fdr_flow);
    pddlHFlowFree(&tnf_flow);
}

static void _tnf_flow_lt(const pddl_fdr_t *fdr, const pddl_fdr_t *tnf)
{
    pddl_hflow_t fdr_flow;
    pddlHFlowInit(&fdr_flow, fdr, 0);

    pddl_hflow_t tnf_flow;
    pddlHFlowInit(&tnf_flow, tnf, 0);

    int fdr_cost = pddlHFlow(&fdr_flow, fdr->init, NULL);
    int tnf_cost = pddlHFlow(&tnf_flow, tnf->init, NULL);
    if (fdr_cost > tnf_cost)
        fprintf(stderr, "Flow failed. fdr: %d tnf: %d\n", fdr_cost, tnf_cost);
    assert(fdr_cost <= tnf_cost);

    pddlHFlowFree(&fdr_flow);
    pddlHFlowFree(&tnf_flow);
}

static pddl_fdr_t tnf;

TEST(tnf, fdr)
{
    pddlFDRInitTransitionNormalForm(&tnf, &C.fdr, NULL, 0, &C.err);
    pddlFDRPrintFD(&tnf, &C.mg, 0, stdout);
}

TEST_TEAR_DOWN(tnf)
{
    pddlFDRFree(&tnf);
}

TEST_COND(tnf_heur, tnf, LP)
{
    _tnf_flow_eq(&C.fdr, &tnf);
}

TEST(mtnf, fdr)
{
    unsigned flag = PDDL_FDR_TNF_MULTIPLY_OPS;
    pddlFDRInitTransitionNormalForm(&tnf, &C.fdr, NULL, flag, &C.err);
    pddlFDRPrintFD(&tnf, &C.mg, 0, stdout);
}

TEST_TEAR_DOWN(mtnf)
{
    pddlFDRFree(&tnf);
}

TEST_COND(mtnf_heur, mtnf, LP)
{
    _tnf_flow_lt(&C.fdr, &tnf);
}
