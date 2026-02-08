#include "test.h"
#include "context.h"
#include <assert.h>

static pddl_mutex_pairs_t h2;
static pddl_iset_t h2_unreachable_op;
static pddl_iset_t h2_unreachable_fact;

TEST(h2, strips_pruned)
{
    pddlMutexPairsInitStrips(&h2, &C.strips);
    pddlISetInit(&h2_unreachable_op);
    pddlISetInit(&h2_unreachable_fact);

    //pddlErrInfoEnable(&err, stdout);
    int ret = pddlH2FwMutexFactsOps(&C.strips, &h2, &h2_unreachable_fact,
                                    &h2_unreachable_op, &C.err);
    assert(ret == 0);

    if (h2.num_mutex_pairs > 0)
        fprintf(stdout, "Mutex pairs: %lu\n", (unsigned long)h2.num_mutex_pairs);

    if (pddlISetSize(&h2_unreachable_fact) > 0){
        fprintf(stdout, "Unreachable facts [%d/%d]:\n",
                pddlISetSize(&h2_unreachable_fact), C.strips.fact.fact_size);
        int fact;
        PDDL_ISET_FOR_EACH(&h2_unreachable_fact, fact){
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
        }
    }
    if (pddlISetSize(&h2_unreachable_op) > 0){
        fprintf(stdout, "Unreachable ops [%d/%d]:\n",
                pddlISetSize(&h2_unreachable_op), C.strips.op.op_size);
        int op;
        PDDL_ISET_FOR_EACH(&h2_unreachable_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
}

TEST_TEAR_DOWN(h2)
{
    pddlMutexPairsFree(&h2);
    pddlISetFree(&h2_unreachable_op);
    pddlISetFree(&h2_unreachable_fact);
}

TEST(h2_bitset, h2)
{
    pddl_mutex_pairs_t h2bs;
    pddl_iset_t h2bs_unreachable_op;
    pddl_iset_t h2bs_unreachable_fact;
    pddlMutexPairsInitStrips(&h2bs, &C.strips);
    pddlISetInit(&h2bs_unreachable_op);
    pddlISetInit(&h2bs_unreachable_fact);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.alg = PDDL_HM_MUTEX_ALG_BITSET;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.mutex_pairs = &h2bs;
    cfg.unreachable_facts = &h2bs_unreachable_fact;
    cfg.unreachable_ops = &h2bs_unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &h2bs;
    res.unreachable_facts = &h2bs_unreachable_fact;
    res.unreachable_ops = &h2bs_unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    assert(pddlISetEq(&h2_unreachable_fact, &h2bs_unreachable_fact));
    assert(pddlISetEq(&h2_unreachable_op, &h2bs_unreachable_op));

    assert(h2.num_mutex_pairs == h2bs.num_mutex_pairs);
    for (int f1 = 0; f1 < C.strips.fact.fact_size; ++f1){
        for (int f2 = 0; f2 < C.strips.fact.fact_size; ++f2){
            assert(pddlMutexPairsIsMutex(&h2, f1, f2)
                    == pddlMutexPairsIsMutex(&h2bs, f1, f2));
            assert(pddlMutexPairsIsFwMutex(&h2, f1, f2)
                    == pddlMutexPairsIsFwMutex(&h2bs, f1, f2));
            assert(pddlMutexPairsIsBwMutex(&h2, f1, f2)
                    == pddlMutexPairsIsBwMutex(&h2bs, f1, f2));
        }
    }

    pddlMutexPairsFree(&h2bs);
    pddlISetFree(&h2bs_unreachable_op);
    pddlISetFree(&h2bs_unreachable_fact);
}

TEST(h2_arr, h2)
{
    pddl_mutex_pairs_t h2bs;
    pddl_iset_t h2bs_unreachable_op;
    pddl_iset_t h2bs_unreachable_fact;
    pddlMutexPairsInitStrips(&h2bs, &C.strips);
    pddlISetInit(&h2bs_unreachable_op);
    pddlISetInit(&h2bs_unreachable_fact);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.alg = PDDL_HM_MUTEX_ALG_ARR;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.mutex_pairs = &h2bs;
    cfg.unreachable_facts = &h2bs_unreachable_fact;
    cfg.unreachable_ops = &h2bs_unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &h2bs;
    res.unreachable_facts = &h2bs_unreachable_fact;
    res.unreachable_ops = &h2bs_unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    assert(pddlISetEq(&h2_unreachable_fact, &h2bs_unreachable_fact));
    assert(pddlISetEq(&h2_unreachable_op, &h2bs_unreachable_op));

    assert(h2.num_mutex_pairs == h2bs.num_mutex_pairs);
    for (int f1 = 0; f1 < C.strips.fact.fact_size; ++f1){
        for (int f2 = 0; f2 < C.strips.fact.fact_size; ++f2){
            assert(pddlMutexPairsIsMutex(&h2, f1, f2)
                    == pddlMutexPairsIsMutex(&h2bs, f1, f2));
            assert(pddlMutexPairsIsFwMutex(&h2, f1, f2)
                    == pddlMutexPairsIsFwMutex(&h2bs, f1, f2));
            assert(pddlMutexPairsIsBwMutex(&h2, f1, f2)
                    == pddlMutexPairsIsBwMutex(&h2bs, f1, f2));
        }
    }

    pddlMutexPairsFree(&h2bs);
    pddlISetFree(&h2bs_unreachable_op);
    pddlISetFree(&h2bs_unreachable_fact);
}

TEST(h2_cmp_inoutargs, strips_pruned)
{
    int rnd_ops[5], rnd_facts[9];
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 1109);
    for (int i = 0; i < 5; ++i)
        rnd_ops[i] = pddlRand(&rnd, 0, C.strips.op.op_size);
    for (int i = 0; i < 9; ++i)
        rnd_facts[i] = pddlRand(&rnd, 0, C.strips.fact.fact_size);
    pddlRandFree(&rnd);

    pddl_mutex_pairs_t h2;
    pddl_iset_t h2_unreachable_op;
    pddl_iset_t h2_unreachable_fact;
    pddlMutexPairsInitStrips(&h2, &C.strips);
    pddlISetInit(&h2_unreachable_op);
    pddlISetInit(&h2_unreachable_fact);
    for (int i = 0; i < 5; ++i)
        pddlISetAdd(&h2_unreachable_op, rnd_ops[i]);
    for (int i = 0; i < 3; ++i){
        pddlISetAdd(&h2_unreachable_fact, rnd_facts[i]);
        pddlMutexPairsAdd(&h2, rnd_facts[i], rnd_facts[i]);
        pddlMutexPairsSetFwMutex(&h2, rnd_facts[i], rnd_facts[i]);
    }
    for (int i = 3; i < 6; ++i){
        for (int j = 6; j < 9; ++j){
            pddlMutexPairsAdd(&h2, rnd_facts[i], rnd_facts[j]);
            pddlMutexPairsSetFwMutex(&h2, rnd_facts[i], rnd_facts[j]);
        }
    }

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.alg = PDDL_HM_MUTEX_ALG_ARR;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.mutex_pairs = &h2;
    cfg.unreachable_facts = &h2_unreachable_fact;
    cfg.unreachable_ops = &h2_unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &h2;
    res.unreachable_facts = &h2_unreachable_fact;
    res.unreachable_ops = &h2_unreachable_op;

    //pddlErrInfoEnable(&err, stdout);
    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    pddl_mutex_pairs_t h2bs;
    pddl_iset_t h2bs_unreachable_op;
    pddl_iset_t h2bs_unreachable_fact;
    pddlMutexPairsInitStrips(&h2bs, &C.strips);
    pddlISetInit(&h2bs_unreachable_op);
    pddlISetInit(&h2bs_unreachable_fact);
    for (int i = 0; i < 5; ++i)
        pddlISetAdd(&h2bs_unreachable_op, rnd_ops[i]);
    for (int i = 0; i < 3; ++i){
        pddlISetAdd(&h2bs_unreachable_fact, rnd_facts[i]);
        pddlMutexPairsAdd(&h2bs, rnd_facts[i], rnd_facts[i]);
        pddlMutexPairsSetFwMutex(&h2bs, rnd_facts[i], rnd_facts[i]);
    }
    for (int i = 3; i < 6; ++i){
        for (int j = 6; j < 9; ++j){
            pddlMutexPairsAdd(&h2bs, rnd_facts[i], rnd_facts[j]);
            pddlMutexPairsSetFwMutex(&h2bs, rnd_facts[i], rnd_facts[j]);
        }
    }

    pddl_hm_mutex_config_t cfgbs = PDDL_HM_MUTEX_CONFIG_INIT;
    cfgbs.m = 2;
    cfgbs.alg = PDDL_HM_MUTEX_ALG_ARR;
    cfgbs.dir = PDDL_HM_MUTEX_DIR_FW;
    cfgbs.strips = &C.strips;
    cfgbs.mutex_pairs = &h2bs;
    cfgbs.unreachable_facts = &h2bs_unreachable_fact;
    cfgbs.unreachable_ops = &h2bs_unreachable_op;

    pddl_hm_mutex_result_t resbs = PDDL_HM_MUTEX_RESULT_INIT;
    resbs.mutex_pairs = &h2bs;
    resbs.unreachable_facts = &h2bs_unreachable_fact;
    resbs.unreachable_ops = &h2bs_unreachable_op;

    ret = pddlHm(&cfgbs, &resbs, &C.err);
    assert(ret == 0);

    assert(pddlISetEq(&h2_unreachable_fact, &h2bs_unreachable_fact));
    assert(pddlISetEq(&h2_unreachable_op, &h2bs_unreachable_op));

    assert(h2.num_mutex_pairs == h2bs.num_mutex_pairs);
    for (int f1 = 0; f1 < C.strips.fact.fact_size; ++f1){
        for (int f2 = 0; f2 < C.strips.fact.fact_size; ++f2){
            assert(!!pddlMutexPairsIsMutex(&h2, f1, f2)
                    == !!pddlMutexPairsIsMutex(&h2bs, f1, f2));
            assert(!!pddlMutexPairsIsFwMutex(&h2, f1, f2)
                    == !!pddlMutexPairsIsFwMutex(&h2bs, f1, f2));
            assert(pddlMutexPairsIsBwMutex(&h2, f1, f2)
                    == pddlMutexPairsIsBwMutex(&h2bs, f1, f2));
        }
    }

    pddlMutexPairsFree(&h2);
    pddlISetFree(&h2_unreachable_op);
    pddlISetFree(&h2_unreachable_fact);

    pddlMutexPairsFree(&h2bs);
    pddlISetFree(&h2bs_unreachable_op);
    pddlISetFree(&h2bs_unreachable_fact);
}

TEST(h2fwbw, h2)
{
    PDDL_ISET(h2fwbw_unreachable_op);
    PDDL_ISET(h2fwbw_unreachable_fact);
    pddl_mutex_pairs_t h2fwbw;
    pddlMutexPairsInitStrips(&h2fwbw, &C.strips);
    pddlISetInit(&h2fwbw_unreachable_fact);
    pddlISetInit(&h2fwbw_unreachable_op);

    //pddlErrInfoEnable(&err, stdout);
    int ret = pddlH2FwBwMutexFactsOps(&C.strips, &C.mg, &h2fwbw,
                                      &h2fwbw_unreachable_fact,
                                      &h2fwbw_unreachable_op,
                                      &C.err);
    assert(ret == 0);

    assert(pddlISetIsSubset(&h2_unreachable_fact, &h2fwbw_unreachable_fact));
    assert(pddlISetIsSubset(&h2_unreachable_op, &h2fwbw_unreachable_op));

    if (h2fwbw.num_mutex_pairs - h2.num_mutex_pairs > 0){
        unsigned long num_fw = 0;
        unsigned long num_bw = 0;
        unsigned long num = 0;
        PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw, f1, f2){
            if (f1 == f2)
                continue;
            ++num;
            if (pddlMutexPairsIsFwMutex(&h2fwbw, f1, f2))
                ++num_fw;
            if (pddlMutexPairsIsBwMutex(&h2fwbw, f1, f2))
                ++num_bw;
        }
        assert(num == h2fwbw.num_mutex_pairs);
        fprintf(stdout, "Mutex pairs: %lu -> %lu, fw: %lu, bw: %lu\n",
                (unsigned long)h2.num_mutex_pairs,
                (unsigned long)h2fwbw.num_mutex_pairs,
                num_fw,
                num_bw);
        assert(num_fw + num_bw == h2fwbw.num_mutex_pairs);
    }

    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw, f1, f2){
        if (f1 == f2){
            assert(pddlISetIn(f1, &h2fwbw_unreachable_fact));
        }
    }

    PDDL_ISET(rm);
    pddlISetMinus2(&rm, &h2fwbw_unreachable_fact, &h2_unreachable_fact);
    if (pddlISetSize(&rm) > 0){
        fprintf(stdout, "Unreachable facts [%d + %d/%d]:\n",
                pddlISetSize(&h2_unreachable_fact),
                pddlISetSize(&rm), C.strips.fact.fact_size);
        int fact;
        PDDL_ISET_FOR_EACH(&rm, fact){
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
        }
    }

    pddlISetMinus2(&rm, &h2fwbw_unreachable_op, &h2_unreachable_op);
    if (pddlISetSize(&rm) > 0){
        fprintf(stdout, "Unreachable ops [%d + %d/%d]:\n",
                pddlISetSize(&h2_unreachable_op),
                pddlISetSize(&rm), C.strips.op.op_size);
        int op;
        PDDL_ISET_FOR_EACH(&rm, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    pddlISetFree(&rm);

    pddl_mutex_pairs_t mutex2;
    pddlMutexPairsInitCopy(&mutex2, &h2fwbw);
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw, f1, f2){
        assert(pddlMutexPairsIsMutex(&mutex2, f1, f2));
        if (pddlMutexPairsIsFwMutex(&h2fwbw, f1, f2)){
            assert(pddlMutexPairsIsFwMutex(&mutex2, f1, f2));
        }
        if (pddlMutexPairsIsBwMutex(&h2fwbw, f1, f2)){
            assert(pddlMutexPairsIsBwMutex(&mutex2, f1, f2));
        }
    }

    if (pddlISetSize(&h2fwbw_unreachable_fact) > 0){
        int *remap = PDDL_ZALLOC_ARR(int, C.strips.fact.fact_size);
        int new_size = pddlFactsDelFactsGenRemap(C.strips.fact.fact_size,
                                                 &h2fwbw_unreachable_fact, remap);
        pddlMutexPairsRemapFacts(&mutex2, new_size, remap);
        PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw, f1, f2){
            if (remap[f1] < 0 || remap[f2] < 0)
                continue;
            if (pddlMutexPairsIsMutex(&h2fwbw, f1, f2)){
                assert(pddlMutexPairsIsMutex(&mutex2, remap[f1], remap[f2]));
            }
            if (pddlMutexPairsIsFwMutex(&h2fwbw, f1, f2)){
                assert(pddlMutexPairsIsFwMutex(&mutex2, remap[f1], remap[f2]));
            }
            if (pddlMutexPairsIsBwMutex(&h2fwbw, f1, f2)){
                assert(pddlMutexPairsIsBwMutex(&mutex2, remap[f1], remap[f2]));
            }
        }
        PDDL_FREE(remap);
    }
    pddlMutexPairsFree(&mutex2);
    

    pddlMutexPairsFree(&h2fwbw);
    pddlISetFree(&h2fwbw_unreachable_fact);
    pddlISetFree(&h2fwbw_unreachable_op);
}

TEST(h2fwbw_cmp, h2)
{
    PDDL_ISET(h2fwbw_unreachable_op);
    PDDL_ISET(h2fwbw_unreachable_fact);
    pddl_mutex_pairs_t h2fwbw;
    pddlMutexPairsInitStrips(&h2fwbw, &C.strips);
    pddlISetInit(&h2fwbw_unreachable_fact);
    pddlISetInit(&h2fwbw_unreachable_op);

    PDDL_ISET(bs_h2fwbw_unreachable_op);
    PDDL_ISET(bs_h2fwbw_unreachable_fact);
    pddl_mutex_pairs_t bs_h2fwbw;
    pddlMutexPairsInitStrips(&bs_h2fwbw, &C.strips);
    pddlISetInit(&bs_h2fwbw_unreachable_fact);
    pddlISetInit(&bs_h2fwbw_unreachable_op);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.alg = PDDL_HM_MUTEX_ALG_ARR;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfg.strips = &C.strips;
    cfg.mgroups = &C.mg;
    cfg.disamb = PDDL_HM_MUTEX_DISAMB_STRONG;
    cfg.task = PDDL_HM_MUTEX_TASK_STRIPS;
    cfg.mutex_pairs = &h2fwbw;
    cfg.unreachable_facts = &h2fwbw_unreachable_fact;
    cfg.unreachable_ops = &h2fwbw_unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &h2fwbw;
    res.unreachable_facts = &h2fwbw_unreachable_fact;
    res.unreachable_ops = &h2fwbw_unreachable_op;

    //pddlErrLogEnable(&C.err, stdout);
    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    pddl_hm_mutex_config_t cfgbs = PDDL_HM_MUTEX_CONFIG_INIT;
    cfgbs.m = 2;
    cfgbs.alg = PDDL_HM_MUTEX_ALG_BITSET;
    cfgbs.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfgbs.strips = &C.strips;
    cfgbs.mgroups = &C.mg;
    cfgbs.disamb = PDDL_HM_MUTEX_DISAMB_STRONG;
    cfgbs.task = PDDL_HM_MUTEX_TASK_STRIPS;
    cfgbs.mutex_pairs = &bs_h2fwbw;
    cfgbs.unreachable_facts = &bs_h2fwbw_unreachable_fact;
    cfgbs.unreachable_ops = &bs_h2fwbw_unreachable_op;

    pddl_hm_mutex_result_t resbs = PDDL_HM_MUTEX_RESULT_INIT;
    resbs.mutex_pairs = &bs_h2fwbw;
    resbs.unreachable_facts = &bs_h2fwbw_unreachable_fact;
    resbs.unreachable_ops = &bs_h2fwbw_unreachable_op;

    ret = pddlHm(&cfgbs, &resbs, &C.err);
    assert(ret == 0);

    assert(pddlISetEq(&h2fwbw_unreachable_fact, &bs_h2fwbw_unreachable_fact));
    assert(pddlISetEq(&h2fwbw_unreachable_op, &bs_h2fwbw_unreachable_op));


    for (int f1 = 0; f1 < C.strips.fact.fact_size; ++f1){
        for (int f2 = 0; f2 < C.strips.fact.fact_size; ++f2){
            assert(!!pddlMutexPairsIsMutex(&h2fwbw, f1, f2)
                    == !!pddlMutexPairsIsMutex(&bs_h2fwbw, f1, f2));
            /*
            assert(!!pddlMutexPairsIsFwMutex(&h2fwbw, f1, f2)
                    == !!pddlMutexPairsIsFwMutex(&bs_h2fwbw, f1, f2));
            assert(!!pddlMutexPairsIsBwMutex(&h2fwbw, f1, f2)
                    == !!pddlMutexPairsIsBwMutex(&bs_h2fwbw, f1, f2));
            */
        }
    }
    assert(h2fwbw.num_mutex_pairs == bs_h2fwbw.num_mutex_pairs);


    pddlMutexPairsFree(&h2fwbw);
    pddlISetFree(&h2fwbw_unreachable_fact);
    pddlISetFree(&h2fwbw_unreachable_op);

    pddlMutexPairsFree(&bs_h2fwbw);
    pddlISetFree(&bs_h2fwbw_unreachable_fact);
    pddlISetFree(&bs_h2fwbw_unreachable_op);
}

TEST(h2fwbw_cmp_mgstrips, h2)
{
    pddlErrLogEnable(&C.err, stderr);
    pddl_mg_strips_t mg_strips;
    pddlMGStripsInit(&mg_strips, &C.strips, &C.mg);

    PDDL_ISET(h2fwbw_unreachable_op);
    PDDL_ISET(h2fwbw_unreachable_fact);
    pddl_mutex_pairs_t h2fwbw;
    pddlMutexPairsInitStrips(&h2fwbw, &mg_strips.strips);
    pddlISetInit(&h2fwbw_unreachable_fact);
    pddlISetInit(&h2fwbw_unreachable_op);

    PDDL_ISET(bs_h2fwbw_unreachable_op);
    PDDL_ISET(bs_h2fwbw_unreachable_fact);
    pddl_mutex_pairs_t bs_h2fwbw;
    pddlMutexPairsInitStrips(&bs_h2fwbw, &mg_strips.strips);
    pddlISetInit(&bs_h2fwbw_unreachable_fact);
    pddlISetInit(&bs_h2fwbw_unreachable_op);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.alg = PDDL_HM_MUTEX_ALG_ARR;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfg.strips = &C.strips;
    cfg.mgroups = &C.mg;
    cfg.disamb = PDDL_HM_MUTEX_DISAMB_STRONG;
    cfg.task = PDDL_HM_MUTEX_TASK_STRIPS;
    cfg.mutex_pairs = &h2fwbw;
    cfg.unreachable_facts = &h2fwbw_unreachable_fact;
    cfg.unreachable_ops = &h2fwbw_unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &h2fwbw;
    res.unreachable_facts = &h2fwbw_unreachable_fact;
    res.unreachable_ops = &h2fwbw_unreachable_op;

    //pddlErrInfoEnable(&err, stdout);
    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    pddl_hm_mutex_config_t cfgbs = PDDL_HM_MUTEX_CONFIG_INIT;
    cfgbs.m = 2;
    cfgbs.alg = PDDL_HM_MUTEX_ALG_BITSET;
    cfgbs.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfgbs.strips = &C.strips;
    cfgbs.mgroups = &C.mg;
    cfgbs.disamb = PDDL_HM_MUTEX_DISAMB_STRONG;
    cfgbs.task = PDDL_HM_MUTEX_TASK_STRIPS;
    cfgbs.mutex_pairs = &bs_h2fwbw;
    cfgbs.unreachable_facts = &bs_h2fwbw_unreachable_fact;
    cfgbs.unreachable_ops = &bs_h2fwbw_unreachable_op;

    pddl_hm_mutex_result_t resbs = PDDL_HM_MUTEX_RESULT_INIT;
    resbs.mutex_pairs = &bs_h2fwbw;
    resbs.unreachable_facts = &bs_h2fwbw_unreachable_fact;
    resbs.unreachable_ops = &bs_h2fwbw_unreachable_op;

    ret = pddlHm(&cfgbs, &resbs, &C.err);
    assert(ret == 0);

    assert(pddlISetEq(&h2fwbw_unreachable_fact, &bs_h2fwbw_unreachable_fact));
    assert(pddlISetEq(&h2fwbw_unreachable_op, &bs_h2fwbw_unreachable_op));


    for (int f1 = 0; f1 < mg_strips.strips.fact.fact_size; ++f1){
        for (int f2 = 0; f2 < mg_strips.strips.fact.fact_size; ++f2){
            assert(!!pddlMutexPairsIsMutex(&h2fwbw, f1, f2)
                    == !!pddlMutexPairsIsMutex(&bs_h2fwbw, f1, f2));
            /*
            assert(!!pddlMutexPairsIsFwMutex(&h2fwbw, f1, f2)
                    == !!pddlMutexPairsIsFwMutex(&bs_h2fwbw, f1, f2));
            assert(!!pddlMutexPairsIsBwMutex(&h2fwbw, f1, f2)
                    == !!pddlMutexPairsIsBwMutex(&bs_h2fwbw, f1, f2));
            */
        }
    }
    assert(h2fwbw.num_mutex_pairs == bs_h2fwbw.num_mutex_pairs);


    pddlMutexPairsFree(&h2fwbw);
    pddlISetFree(&h2fwbw_unreachable_fact);
    pddlISetFree(&h2fwbw_unreachable_op);

    pddlMutexPairsFree(&bs_h2fwbw);
    pddlISetFree(&bs_h2fwbw_unreachable_fact);
    pddlISetFree(&bs_h2fwbw_unreachable_op);

    pddlMGStripsFree(&mg_strips);
}

TEST(h3, h2)
{
    pddl_mutex_pairs_t h3;
    PDDL_ISET(unreachable_fact);
    PDDL_ISET(unreachable_op);

    pddlMutexPairsInitStrips(&h3, &C.strips);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 3;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.mutex_pairs = &h3;
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &h3;
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    assert(pddlISetIsSubset(&h2_unreachable_fact, &unreachable_fact));
    assert(pddlISetIsSubset(&h2_unreachable_op, &unreachable_op));
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2, f1, f2){
        assert(pddlMutexPairsIsMutex(&h3, f1, f2));
    }

    if (h3.num_mutex_pairs > 0)
        fprintf(stdout, "Mutex pairs: %lu\n",
                (unsigned long)h3.num_mutex_pairs);

    if (pddlISetSize(&unreachable_fact) > 0){
        fprintf(stdout, "Unreachable facts [%d/%d]:\n",
                pddlISetSize(&unreachable_fact), C.strips.fact.fact_size);
        int fact;
        PDDL_ISET_FOR_EACH(&unreachable_fact, fact){
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
        }
    }
    if (pddlISetSize(&unreachable_op) > 0){
        fprintf(stdout, "Unreachable ops [%d/%d]:\n",
                pddlISetSize(&unreachable_op), C.strips.op.op_size);
        int op;
        PDDL_ISET_FOR_EACH(&unreachable_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }

    pddlISetFree(&unreachable_fact);
    pddlISetFree(&unreachable_op);
    pddlMutexPairsFree(&h3);
}


static int disamb(pddl_disambiguate_t *dis,
                  const pddl_strips_t *strips,
                  const pddl_iset_t *s1,
                  pddl_iset_t *s2,
                  const char *header)
{
    int ret = pddlDisambiguateSet(dis, s2);
    if (ret > 0){
        fprintf(stdout, "%s\n", header);
        fprintf(stdout, "  ");
        pddlFactsPrintSet(s1, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");
        fprintf(stdout, "   +");
        PDDL_ISET(add);
        pddlISetMinus2(&add, s2, s1);
        pddlFactsPrintSet(&add, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");
        pddlISetFree(&add);
    }

    return ret;
}

TEST(disambiguation, h2)
{
    pddl_strips_t strips2;

    pddlStripsInitCopy(&strips2, &C.strips);

    pddl_disambiguate_t dis;
    pddlDisambiguateInit(&dis, C.strips.fact.fact_size, &h2, &C.mg);
    if (disamb(&dis, &C.strips, &C.strips.goal, &strips2.goal, "Goal:") < 0){
        fprintf(stdout, "Unsolvable\n");
    }else{
        for (int op_id = 0; op_id < C.strips.op.op_size && op_id < 500; ++op_id){
            const pddl_strips_op_t *op = C.strips.op.op[op_id];
            pddl_strips_op_t *op2 = strips2.op.op[op_id];
            char header[128];
            snprintf(header, 128, "(%s)", op->name);
            header[127] = 0;
            int ret = disamb(&dis, &C.strips, &op->pre, &op2->pre, header);
            if (ret < 0)
                fprintf(stdout, "Unreachable: (%s)\n", op->name);
        }
    }
    pddlDisambiguateFree(&dis);

    pddlStripsFree(&strips2);
}

static int disambTestMin(pddl_disambiguate_t *dis,
                         const pddl_strips_t *strips,
                         const pddl_iset_t *F,
                         const char *header)
{
    PDDL_ISET(sarc);
    PDDL_ISET(smin);
    pddlISetUnion(&sarc, F);
    pddlISetUnion(&smin, F);

    int st1 = pddlDisambiguateSet(dis, &sarc);
    int st2 = pddlDisambiguateSetMinimal(dis, &smin);
    if (st2 >= 0){
        assert(st1 >= 0);
        assert(pddlISetIsSubset(&sarc, &smin));
    }
    if (st1 < 0){
        assert(st2 < 0);
    }
    if (st2 == 0){
        assert(st1 == 0);
    }
    if (st1 > 0){
        assert(st2 != 0);
    }

    if (F == &strips->goal && C.optimal_cost >= 0){
        assert(st1 >= 0);
        assert(st2 >= 0);
    }

    if (st1 >= 0 && st2 < 0){
        fprintf(stdout, "%s ::", header);
        pddlFactsPrintSet(F, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");
        fprintf(stdout, "   Dead by minimum disambiguation, but not arc consistency\n");

    }else if (!pddlISetEq(&sarc, &smin) && st2 > 0){
        fprintf(stdout, "%s ::", header);
        pddlFactsPrintSet(F, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");
        fprintf(stdout, "   +");
        PDDL_ISET(add);
        pddlISetMinus2(&add, &sarc, F);
        pddlFactsPrintSet(&add, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");

        fprintf(stdout, "      +");
        pddlISetMinus2(&add, &smin, &sarc);
        pddlFactsPrintSet(&add, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");
        pddlISetFree(&add);
    }

    pddlISetFree(&sarc);
    pddlISetFree(&smin);

    return st1 || st2;
}

TEST_COND(disambiguation_min, disambiguation, CADICAL)
{
    pddl_mg_strips_t mg_strips;
    pddlMGStripsInit(&mg_strips, &C.strips, &C.mg);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);
    pddlMutexPairsAddMGroups(&mutex, &mg_strips.mg);

    pddl_hm_mutex_config_t hmcfg = PDDL_HM_MUTEX_CONFIG_INIT;
    hmcfg.strips = &mg_strips.strips;
    hmcfg.mgroups = &mg_strips.mg;
    hmcfg.mutex_pairs = &mutex;

    pddl_hm_mutex_result_t hm_res = PDDL_HM_MUTEX_RESULT_INIT;
    hm_res.mutex_pairs = &mutex;
    int ret = pddlHm(&hmcfg, &hm_res, &C.err);
    assert(ret == 0);

    pddl_disambiguate_t dis;
    pddlDisambiguateInit(&dis, mg_strips.strips.fact.fact_size,
                         &mutex, &mg_strips.mg);

    disambTestMin(&dis, &mg_strips.strips, &mg_strips.strips.goal, "Goal:");

    for (int op_id = 0; op_id < C.strips.op.op_size && op_id < 300; ++op_id){
        const pddl_strips_op_t *op = mg_strips.strips.op.op[op_id];
        char header[1024];
        snprintf(header, 1024, "(%s)", op->name);
        header[1023] = 0;
        disambTestMin(&dis, &mg_strips.strips, &op->pre, header);
    }


    pddlDisambiguateFree(&dis);
    pddlMutexPairsFree(&mutex);
    pddlMGStripsFree(&mg_strips);
}

TEST(mutex_pair_copy_fdr, fdr)
{
    pddl_mutex_pairs_t fdr_mutex;
    pddlFDRMutexPairsInitCopy(&fdr_mutex, &C.mutex, &C.fdr);

    for (int f1 = 0; f1 < fdr_mutex.fact_size; ++f1){
        for (int f2 = f1 + 1; f2 < fdr_mutex.fact_size; ++f2){
            if (pddlMutexPairsIsMutex(&fdr_mutex, f1, f2)){
                /*
                printf("%d: %s | %d | %d %d\n", f1,
                       C.fdr.var.global_id_to_val[f1]->name,
                       C.fdr.var.global_id_to_val[f1]->strips_id,
                       C.fdr.var.global_id_to_val[f1]->var_id,
                       C.fdr.var.global_id_to_val[f1]->val_id);
                printf("%d: %s | %d | %d %d\n", f2, C.fdr.var.global_id_to_val[f2]->name,
                       C.fdr.var.global_id_to_val[f2]->strips_id,
                       C.fdr.var.global_id_to_val[f2]->var_id,
                       C.fdr.var.global_id_to_val[f2]->val_id);
                fflush(stdout);
                */
                assert(C.fdr.var.global_id_to_val[f1]->strips_id >= 0);
                assert(C.fdr.var.global_id_to_val[f2]->strips_id >= 0);
                assert(pddlMutexPairsIsMutex(&C.mutex,
                                             C.fdr.var.global_id_to_val[f1]->strips_id,
                                             C.fdr.var.global_id_to_val[f2]->strips_id));

                if (pddlMutexPairsIsFwMutex(&fdr_mutex, f1, f2)){
                    assert(pddlMutexPairsIsFwMutex(&C.mutex,
                                                   C.fdr.var.global_id_to_val[f1]->strips_id,
                                                   C.fdr.var.global_id_to_val[f2]->strips_id));
                }else{
                    assert(!pddlMutexPairsIsFwMutex(&C.mutex,
                                                    C.fdr.var.global_id_to_val[f1]->strips_id,
                                                    C.fdr.var.global_id_to_val[f2]->strips_id));
                }

                if (pddlMutexPairsIsBwMutex(&fdr_mutex, f1, f2)){
                    assert(pddlMutexPairsIsBwMutex(&C.mutex,
                                                   C.fdr.var.global_id_to_val[f1]->strips_id,
                                                   C.fdr.var.global_id_to_val[f2]->strips_id));
                }else{
                    assert(!pddlMutexPairsIsBwMutex(&C.mutex,
                                                    C.fdr.var.global_id_to_val[f1]->strips_id,
                                                    C.fdr.var.global_id_to_val[f2]->strips_id));
                }

            }else{
                if (C.fdr.var.global_id_to_val[f1]->strips_id >= 0
                        && C.fdr.var.global_id_to_val[f2]->strips_id >= 0){
                    assert(!pddlMutexPairsIsMutex(&C.mutex,
                                                  C.fdr.var.global_id_to_val[f1]->strips_id,
                                                  C.fdr.var.global_id_to_val[f2]->strips_id));
                }
            }
        }
    }

    for (int f1 = 0; f1 < C.mutex.fact_size; ++f1){
        for (int f2 = f1 + 1; f2 < C.mutex.fact_size; ++f2){
            int x1, x2;
            PDDL_ISET_FOR_EACH(&C.fdr.var.strips_id_to_val[f1], x1){
                PDDL_ISET_FOR_EACH(&C.fdr.var.strips_id_to_val[f2], x2){
                    if (pddlMutexPairsIsMutex(&C.mutex, f1, f2)){
                        assert(pddlMutexPairsIsMutex(&fdr_mutex, x1, x2));

                        if (pddlMutexPairsIsFwMutex(&C.mutex, f1, f2)){
                            assert(pddlMutexPairsIsFwMutex(&fdr_mutex, x1, x2));
                        }else{
                            assert(!pddlMutexPairsIsFwMutex(&fdr_mutex, x1, x2));
                        }

                        if (pddlMutexPairsIsBwMutex(&C.mutex, f1, f2)){
                            assert(pddlMutexPairsIsBwMutex(&fdr_mutex, x1, x2));
                        }else{
                            assert(!pddlMutexPairsIsBwMutex(&fdr_mutex, x1, x2));
                        }

                    }else{
                        assert(!pddlMutexPairsIsMutex(&fdr_mutex, x1, x2));
                    }
                }
            }
        }
    }
    pddlMutexPairsFree(&fdr_mutex);
}
