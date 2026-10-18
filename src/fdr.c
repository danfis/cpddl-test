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
    pddlH2FwMutexFactsOps(&mg_strips.strips, &C.mutex, &unreach_fact,
                          &unreach_op, &C.err);
    pddlMGStripsFree(&mg_strips);

    pddlFDRReduce(&C.fdr, NULL, &unreach_fact, &unreach_op);
    if (pddlISetSize(&unreach_fact) > 0){
        pddl_mg_strips_t mg_strips;
        pddlMGStripsInitFDR(&mg_strips, &C.fdr);
        pddlMutexPairsFree(&C.mutex);
        pddlMutexPairsInitStrips(&C.mutex, &mg_strips.strips);
        pddlMutexPairsAddMGroups(&C.mutex, &mg_strips.mg);
        pddlH2FwMutex(&mg_strips.strips, &C.mutex, &C.err);
        pddlMGStripsFree(&mg_strips);
    }

    pddlISetFree(&unreach_fact);
    pddlISetFree(&unreach_op);
}


TEST(fdr_app_op, fdr_largest)
{
    pddlFDRAppOpInitDefault(&C.fdr_app_op, &C.fdr.var, &C.fdr.op, &C.fdr.goal);
    C.fdr_app_op_set = 1;
}

TEST(fdr, fdr_app_op)
{
}

TEST(fdr_app_op_essential, fdr_essential)
{
    pddlFDRAppOpInitDefault(&C.fdr_app_op, &C.fdr.var, &C.fdr.op, &C.fdr.goal);
    C.fdr_app_op_set = 1;
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

static pddl_fdr_conj_exact_t fdrpc;
static int fdrpc_set = 0;
TEST(fdr_conj_exact, fdr_h2)
{
    pddl_fdr_conj_exact_config_t cfg;
    pddlFDRConjExactConfigInit(&cfg);

    int f0 = -1, f1 = -1, f2 = -1, f3 = -1;
    for (f0 = 0; f0 < C.fdr.var.global_id_size && f1 < 0; ++f0){
        PDDL_ISET(notmutex);
        pddlMutexPairsGetNotMutexWith(&C.mutex, f0, &notmutex);
        if (pddlISetSize(&notmutex) >= 2){
            f1 = pddlISetGet(&notmutex, 0);
            f2 = pddlISetGet(&notmutex, pddlISetSize(&notmutex) - 1);
            pddlISetFree(&notmutex);
            break;
        }
        pddlISetFree(&notmutex);
    }

    for (f3 = 0; f3 < C.fdr.var.global_id_size; ++f3){
        if (f3 == f0 || f3 == f1 || f3 == f2)
            continue;
        if (!pddlMutexPairsIsMutex(&C.mutex, f1, f3))
            break;
    }
    if (f3 >= C.fdr.var.global_id_size)
        f3 = -1;

    if (f1 < 0)
        return;

    PDDL_ISET(set);
    pddlISetAdd(&set, f0);
    pddlISetAdd(&set, f1);
    pddlFDRConjExactConfigAddConj(&cfg, &set);
    if (pddlMutexPairsIsMutexSet(&C.mutex, &set))
        fprintf(stderr, "mutex f0, f1: (%s) (%s)\n",
                C.fdr.var.global_id_to_val[f0]->name,
                C.fdr.var.global_id_to_val[f1]->name);

    pddlISetClear(&set);
    pddlISetAdd(&set, f0);
    pddlISetAdd(&set, f2);
    pddlFDRConjExactConfigAddConj(&cfg, &set);
    if (pddlMutexPairsIsMutexSet(&C.mutex, &set))
        fprintf(stderr, "mutex f0, f2: (%s) (%s)\n",
                C.fdr.var.global_id_to_val[f0]->name,
                C.fdr.var.global_id_to_val[f2]->name);

    pddlISetClear(&set);
    pddlISetAdd(&set, f0);
    pddlISetAdd(&set, f1);
    pddlISetAdd(&set, f2);
    if (!pddlMutexPairsIsMutexSet(&C.mutex, &set))
        pddlFDRConjExactConfigAddConj(&cfg, &set);

    if (f3 >= 0 && f3 < C.fdr.var.global_id_size){
        pddlISetClear(&set);
        pddlISetAdd(&set, f1);
        pddlISetAdd(&set, f3);
        pddlFDRConjExactConfigAddConj(&cfg, &set);
        if (pddlMutexPairsIsMutexSet(&C.mutex, &set))
            fprintf(stderr, "mutex f1, f3: (%s) (%s)\n",
                    C.fdr.var.global_id_to_val[f1]->name,
                    C.fdr.var.global_id_to_val[f3]->name);

        pddlISetClear(&set);
        pddlISetAdd(&set, f0);
        pddlISetAdd(&set, f1);
        pddlISetAdd(&set, f2);
        pddlISetAdd(&set, f3);
        if (!pddlMutexPairsIsMutexSet(&C.mutex, &set))
            pddlFDRConjExactConfigAddConj(&cfg, &set);
    }
    pddlISetFree(&set);

    cfg.mutex = &C.mutex;
    int ret = pddlFDRConjExactInit(&fdrpc, &C.fdr, &cfg, &C.err);
    assert(ret == 0);
    fdrpc_set = 1;

    //pddlFDRPrintFD(&C.fdr, NULL, 0, stdout);
    //fprintf(stdout, "=============================\n");
    pddlFDRPrintFD(&fdrpc.fdr, NULL, 0, stdout);

    pddlFDRConjExactConfigFree(&cfg);
}

TEST_TEAR_DOWN(fdr_conj_exact)
{
    if (fdrpc_set)
        pddlFDRConjExactFree(&fdrpc);
}
