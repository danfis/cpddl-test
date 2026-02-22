#include "test.h"
#include "context.h"
#include <assert.h>

/* Shared state between the h1 root test and its children */
static pddl_iset_t h1_unreachable_fact;
static pddl_iset_t h1_unreachable_op;

/* Shared state between h2_fw and its children */
static pddl_mutex_pairs_t h2fw_mutex;
static pddl_iset_t h2fw_unreachable_fact;
static pddl_iset_t h2fw_unreachable_op;

/* Shared state between h2_fwbw and its children */
static pddl_mutex_pairs_t h2fwbw_mutex;
static pddl_iset_t h2fwbw_unreachable_fact;
static pddl_iset_t h2fwbw_unreachable_op;

/*
 * Root of the hm test tree.
 * Depends on strips, providing C.strips and C.mg.
 * Runs h^1 forward reachability and stores results for children.
 */
TEST(hm_h1, strips)
{
    pddlISetInit(&h1_unreachable_fact);
    pddlISetInit(&h1_unreachable_op);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 1;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.unreachable_facts = &h1_unreachable_fact;
    res.unreachable_ops = &h1_unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    if (pddlISetSize(&h1_unreachable_fact) > 0){
        fprintf(stdout, "h1 unreachable facts [%d/%d]:\n",
                pddlISetSize(&h1_unreachable_fact),
                C.strips.fact.fact_size);
        PDDL_ISET_FOR_EACH(&h1_unreachable_fact, fact)
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
    }
    if (pddlISetSize(&h1_unreachable_op) > 0){
        fprintf(stdout, "h1 unreachable ops [%d/%d]:\n",
                pddlISetSize(&h1_unreachable_op),
                C.strips.op.op_size);
        PDDL_ISET_FOR_EACH(&h1_unreachable_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
}

TEST_TEAR_DOWN(hm_h1)
{
    pddlISetFree(&h1_unreachable_fact);
    pddlISetFree(&h1_unreachable_op);
}

/*
 * h^2 forward-only reachability.
 * Verifies that h^1 results are a subset of h^2 results.
 * Results stored in file-statics for child tests.
 */
TEST(hm_h2_fw, hm_h1)
{
    pddlMutexPairsInitStrips(&h2fw_mutex, &C.strips);
    pddlISetInit(&h2fw_unreachable_fact);
    pddlISetInit(&h2fw_unreachable_op);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.mutex_pairs = &h2fw_mutex;
    cfg.unreachable_facts = &h2fw_unreachable_fact;
    cfg.unreachable_ops = &h2fw_unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &h2fw_mutex;
    res.unreachable_facts = &h2fw_unreachable_fact;
    res.unreachable_ops = &h2fw_unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* h^2 must be at least as strong as h^1 */
    assert(pddlISetIsSubset(&h1_unreachable_fact, &h2fw_unreachable_fact));
    assert(pddlISetIsSubset(&h1_unreachable_op, &h2fw_unreachable_op));

    if (h2fw_mutex.num_mutex_pairs > 0)
        fprintf(stdout, "h2fw mutex pairs: %lu\n",
                (unsigned long)h2fw_mutex.num_mutex_pairs);
    if (pddlISetSize(&h2fw_unreachable_fact) > 0){
        fprintf(stdout, "h2fw unreachable facts [%d/%d]:\n",
                pddlISetSize(&h2fw_unreachable_fact),
                C.strips.fact.fact_size);
        PDDL_ISET_FOR_EACH(&h2fw_unreachable_fact, fact)
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
    }
    if (pddlISetSize(&h2fw_unreachable_op) > 0){
        fprintf(stdout, "h2fw unreachable ops [%d/%d]:\n",
                pddlISetSize(&h2fw_unreachable_op),
                C.strips.op.op_size);
        PDDL_ISET_FOR_EACH(&h2fw_unreachable_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
}

TEST_TEAR_DOWN(hm_h2_fw)
{
    pddlMutexPairsFree(&h2fw_mutex);
    pddlISetFree(&h2fw_unreachable_fact);
    pddlISetFree(&h2fw_unreachable_op);
}

/*
 * h^2 forward with pre-seeded input mutex pairs and unreachable sets.
 * Verifies that pre-existing knowledge is preserved and only extended.
 * The result must include both the seeded input and the clean h2fw results.
 */
TEST(hm_h2_fw_inout, hm_h2_fw)
{
    pddl_rand_t rnd;
    pddlRandInit(&rnd, 42);

    pddl_mutex_pairs_t mutex;
    PDDL_ISET(unreachable_fact);
    PDDL_ISET(unreachable_op);
    pddlMutexPairsInitStrips(&mutex, &C.strips);

    /* Seed with random fact pairs as known mutexes */
    if (C.strips.fact.fact_size > 1){
        for (int i = 0; i < 5; ++i){
            int f1 = pddlRand(&rnd, 0, C.strips.fact.fact_size);
            int f2 = pddlRand(&rnd, 0, C.strips.fact.fact_size);
            pddlMutexPairsAdd(&mutex, f1, f2);
        }
    }
    /* Seed one random op as unreachable */
    if (C.strips.op.op_size > 0)
        pddlISetAdd(&unreachable_op, pddlRand(&rnd, 0, C.strips.op.op_size));
    pddlRandFree(&rnd);

    /* Snapshot seeded state before running h^2 */
    pddl_mutex_pairs_t seeded_mutex;
    pddlMutexPairsInitCopy(&seeded_mutex, &mutex);
    PDDL_ISET(seeded_op);
    pddlISetUnion(&seeded_op, &unreachable_op);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.mutex_pairs = &mutex;
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* All seeded pairs must be preserved in the result */
    PDDL_MUTEX_PAIRS_FOR_EACH(&seeded_mutex, f1, f2)
        assert(pddlMutexPairsIsMutex(&mutex, f1, f2));
    assert(pddlISetIsSubset(&seeded_op, &unreachable_op));

    /* Result must also contain the clean h2fw results */
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fw_mutex, f1, f2)
        assert(pddlMutexPairsIsMutex(&mutex, f1, f2));
    assert(pddlISetIsSubset(&h2fw_unreachable_fact, &unreachable_fact));
    assert(pddlISetIsSubset(&h2fw_unreachable_op, &unreachable_op));

    pddlMutexPairsFree(&seeded_mutex);
    pddlMutexPairsFree(&mutex);
    pddlISetFree(&unreachable_fact);
    pddlISetFree(&unreachable_op);
    pddlISetFree(&seeded_op);
}

/*
 * h^2 forward-backward with strong (full) disambiguation.
 * Must be at least as strong as forward-only h^2.
 * Results stored in file-statics for child tests.
 */
TEST(hm_h2_fwbw, hm_h2_fw)
{
    pddlMutexPairsInitStrips(&h2fwbw_mutex, &C.strips);
    pddlISetInit(&h2fwbw_unreachable_fact);
    pddlISetInit(&h2fwbw_unreachable_op);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfg.strips = &C.strips;
    cfg.mgroups = &C.mg;
    cfg.mutex_pairs = &h2fwbw_mutex;
    cfg.unreachable_facts = &h2fwbw_unreachable_fact;
    cfg.unreachable_ops = &h2fwbw_unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &h2fwbw_mutex;
    res.unreachable_facts = &h2fwbw_unreachable_fact;
    res.unreachable_ops = &h2fwbw_unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* fwbw must be at least as strong as forward-only */
    assert(pddlISetIsSubset(&h2fw_unreachable_fact, &h2fwbw_unreachable_fact));
    assert(pddlISetIsSubset(&h2fw_unreachable_op, &h2fwbw_unreachable_op));
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fw_mutex, f1, f2)
        assert(pddlMutexPairsIsMutex(&h2fwbw_mutex, f1, f2));

    /* Self-mutex pairs must correspond to unreachable facts */
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw_mutex, f1, f2){
        if (f1 == f2)
            assert(pddlISetIn(f1, &h2fwbw_unreachable_fact));
    }

    if (h2fwbw_mutex.num_mutex_pairs > h2fw_mutex.num_mutex_pairs){
        fprintf(stdout, "h2fwbw mutex pairs: %lu -> %lu\n",
                (unsigned long)h2fw_mutex.num_mutex_pairs,
                (unsigned long)h2fwbw_mutex.num_mutex_pairs);
    }else{
        fprintf(stdout, "h2fwbw mutex pairs: %lu\n",
                (unsigned long)h2fwbw_mutex.num_mutex_pairs);
    }

    PDDL_ISET(extra_fact);
    pddlISetMinus2(&extra_fact, &h2fwbw_unreachable_fact, &h2fw_unreachable_fact);
    if (pddlISetSize(&extra_fact) > 0){
        fprintf(stdout, "h2fwbw extra unreachable facts [%d]:\n",
                pddlISetSize(&extra_fact));
        PDDL_ISET_FOR_EACH(&extra_fact, fact)
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
    }
    pddlISetFree(&extra_fact);

    PDDL_ISET(extra_op);
    pddlISetMinus2(&extra_op, &h2fwbw_unreachable_op, &h2fw_unreachable_op);
    if (pddlISetSize(&extra_op) > 0){
        fprintf(stdout, "h2fwbw extra unreachable ops [%d]:\n",
                pddlISetSize(&extra_op));
        PDDL_ISET_FOR_EACH(&extra_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    pddlISetFree(&extra_op);
}

TEST_TEAR_DOWN(hm_h2_fwbw)
{
    pddlMutexPairsFree(&h2fwbw_mutex);
    pddlISetFree(&h2fwbw_unreachable_fact);
    pddlISetFree(&h2fwbw_unreachable_op);
}

/*
 * h^2 fwbw with no disambiguation.
 * Must be at least as strong as forward-only h^2 and at most as strong
 * as h^2 fwbw with full (strong) disambiguation.
 */
TEST(hm_h2_fwbw_no_disamb, hm_h2_fwbw)
{
    pddl_mutex_pairs_t mutex;
    PDDL_ISET(unreachable_fact);
    PDDL_ISET(unreachable_op);
    pddlMutexPairsInitStrips(&mutex, &C.strips);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfg.strips = &C.strips;
    cfg.mgroups = &C.mg;
    cfg.disamb = PDDL_HM_MUTEX_DISAMB_NONE;
    cfg.mutex_pairs = &mutex;
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* no-disamb must be at least as strong as forward-only h^2 */
    assert(pddlISetIsSubset(&h2fw_unreachable_fact, &unreachable_fact));
    assert(pddlISetIsSubset(&h2fw_unreachable_op, &unreachable_op));
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fw_mutex, f1, f2)
        assert(pddlMutexPairsIsMutex(&mutex, f1, f2));

    /* Strong disambiguation must be at least as strong as no-disamb */
    assert(pddlISetIsSubset(&unreachable_fact, &h2fwbw_unreachable_fact));
    assert(pddlISetIsSubset(&unreachable_op, &h2fwbw_unreachable_op));
    PDDL_MUTEX_PAIRS_FOR_EACH(&mutex, f1, f2)
        assert(pddlMutexPairsIsMutex(&h2fwbw_mutex, f1, f2));

    pddlMutexPairsFree(&mutex);
    pddlISetFree(&unreachable_fact);
    pddlISetFree(&unreachable_op);
}

/*
 * h^2 fwbw with MG_STRIPS task transformation.
 * Encodes mutex groups into the state space, making h^2 more informative.
 * Results are in terms of the original STRIPS facts and must be at least
 * as strong as plain h^2 fwbw.
 */
TEST(hm_h2_fwbw_mg_strips, hm_h2_fwbw)
{
    pddl_mutex_pairs_t mutex;
    PDDL_ISET(unreachable_fact);
    PDDL_ISET(unreachable_op);
    pddlMutexPairsInitStrips(&mutex, &C.strips);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW_BW;
    cfg.strips = &C.strips;
    cfg.mgroups = &C.mg;
    cfg.task = PDDL_HM_MUTEX_TASK_MG_STRIPS;
    cfg.mutex_pairs = &mutex;
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* MG_STRIPS encoding must be at least as strong as plain fwbw */
    assert(pddlISetIsSubset(&h2fwbw_unreachable_fact, &unreachable_fact));
    assert(pddlISetIsSubset(&h2fwbw_unreachable_op, &unreachable_op));
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fwbw_mutex, f1, f2)
        assert(pddlMutexPairsIsMutex(&mutex, f1, f2));

    if (mutex.num_mutex_pairs > h2fwbw_mutex.num_mutex_pairs){
        fprintf(stdout, "h2fwbw mg_strips extra mutex pairs: %lu -> %lu\n",
                (unsigned long)h2fwbw_mutex.num_mutex_pairs,
                (unsigned long)mutex.num_mutex_pairs);
    }

    PDDL_ISET(extra_fact);
    pddlISetMinus2(&extra_fact, &unreachable_fact, &h2fwbw_unreachable_fact);
    if (pddlISetSize(&extra_fact) > 0){
        fprintf(stdout, "h2fwbw mg_strips extra unreachable facts [%d]:\n",
                pddlISetSize(&extra_fact));
        PDDL_ISET_FOR_EACH(&extra_fact, fact)
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
    }
    pddlISetFree(&extra_fact);

    PDDL_ISET(extra_op);
    pddlISetMinus2(&extra_op, &unreachable_op, &h2fwbw_unreachable_op);
    if (pddlISetSize(&extra_op) > 0){
        fprintf(stdout, "h2fwbw mg_strips extra unreachable ops [%d]:\n",
                pddlISetSize(&extra_op));
        PDDL_ISET_FOR_EACH(&extra_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    pddlISetFree(&extra_op);

    pddlMutexPairsFree(&mutex);
    pddlISetFree(&unreachable_fact);
    pddlISetFree(&unreachable_op);
}

/*
 * h^3 forward reachability.
 * Must be at least as strong as h^2 forward results.
 */
TEST(hm_h3, hm_h2_fw)
{
    pddl_mutex_pairs_t mutex;
    PDDL_ISET(unreachable_fact);
    PDDL_ISET(unreachable_op);
    pddlMutexPairsInitStrips(&mutex, &C.strips);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 3;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.mutex_pairs = &mutex;
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* h^3 must be at least as strong as h^2 forward */
    assert(pddlISetIsSubset(&h2fw_unreachable_fact, &unreachable_fact));
    assert(pddlISetIsSubset(&h2fw_unreachable_op, &unreachable_op));
    PDDL_MUTEX_PAIRS_FOR_EACH(&h2fw_mutex, f1, f2)
        assert(pddlMutexPairsIsMutex(&mutex, f1, f2));

    if (mutex.num_mutex_pairs > h2fw_mutex.num_mutex_pairs){
        fprintf(stdout, "h3 mutex pairs: %lu -> %lu\n",
                (unsigned long)h2fw_mutex.num_mutex_pairs,
                (unsigned long)mutex.num_mutex_pairs);
    }else{
        fprintf(stdout, "h3 mutex pairs: %lu\n",
                (unsigned long)mutex.num_mutex_pairs);
    }

    PDDL_ISET(extra_fact);
    pddlISetMinus2(&extra_fact, &unreachable_fact, &h2fw_unreachable_fact);
    if (pddlISetSize(&extra_fact) > 0){
        fprintf(stdout, "h3 extra unreachable facts [%d]:\n",
                pddlISetSize(&extra_fact));
        PDDL_ISET_FOR_EACH(&extra_fact, fact)
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
    }
    pddlISetFree(&extra_fact);

    PDDL_ISET(extra_op);
    pddlISetMinus2(&extra_op, &unreachable_op, &h2fw_unreachable_op);
    if (pddlISetSize(&extra_op) > 0){
        fprintf(stdout, "h3 extra unreachable ops [%d]:\n",
                pddlISetSize(&extra_op));
        PDDL_ISET_FOR_EACH(&extra_op, op)
            fprintf(stdout, "  (%s)\n", C.strips.op.op[op]->name);
    }
    pddlISetFree(&extra_op);

    pddlMutexPairsFree(&mutex);
    pddlISetFree(&unreachable_fact);
    pddlISetFree(&unreachable_op);
}
