#include "test.h"
#include "context.h"
#include <assert.h>

/* Shared state between the h1 root test and its children */
static pddl_mutex_pairs_t h1_mutex;
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
 * Assert that result A is at least as strong as result B:
 * every mutex pair in B is also in A, and every unreachable fact/op in B
 * is also unreachable in A.  NULL arguments for the mutex or unreachable
 * sets are silently skipped.
 */
static void assert_stronger(const pddl_mutex_pairs_t *a_mutex,
                            const pddl_iset_t *a_fact,
                            const pddl_iset_t *a_op,
                            const pddl_mutex_pairs_t *b_mutex,
                            const pddl_iset_t *b_fact,
                            const pddl_iset_t *b_op)
{
    if (b_mutex != NULL && a_mutex != NULL){
        PDDL_MUTEX_PAIRS_FOR_EACH(b_mutex, f1, f2)
            assert(pddlMutexPairsIsMutex(a_mutex, f1, f2));
    }
    if (b_fact != NULL && a_fact != NULL)
        assert(pddlISetIsSubset(b_fact, a_fact));
    if (b_op != NULL && a_op != NULL)
        assert(pddlISetIsSubset(b_op, a_op));
}

/*
 * Root of the hm test tree.
 * Depends on strips, providing C.strips and C.mg.
 * Runs h^1 forward reachability and stores results for children.
 */
TEST(hm_h1, strips)
{
    pddlMutexPairsInitStrips(&h1_mutex, &C.strips);
    pddlISetInit(&h1_unreachable_fact);
    pddlISetInit(&h1_unreachable_op);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 1;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.mutex_pairs = &h1_mutex;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &h1_mutex;
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
    pddlMutexPairsFree(&h1_mutex);
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
    assert_stronger(&h2fw_mutex, &h2fw_unreachable_fact, &h2fw_unreachable_op,
                    &h1_mutex, &h1_unreachable_fact, &h1_unreachable_op);

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
    assert_stronger(&mutex, &unreachable_fact, &unreachable_op,
                    &seeded_mutex, NULL, &seeded_op);

    /* Result must also contain the clean h2fw results */
    assert_stronger(&mutex, &unreachable_fact, &unreachable_op,
                    &h2fw_mutex, &h2fw_unreachable_fact, &h2fw_unreachable_op);

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
    assert_stronger(&h2fwbw_mutex, &h2fwbw_unreachable_fact, &h2fwbw_unreachable_op,
                    &h2fw_mutex, &h2fw_unreachable_fact, &h2fw_unreachable_op);

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
    assert_stronger(&mutex, &unreachable_fact, &unreachable_op,
                    &h2fw_mutex, &h2fw_unreachable_fact, &h2fw_unreachable_op);

    /* Strong disambiguation must be at least as strong as no-disamb */
    assert_stronger(&h2fwbw_mutex, &h2fwbw_unreachable_fact, &h2fwbw_unreachable_op,
                    &mutex, &unreachable_fact, &unreachable_op);

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
    assert_stronger(&mutex, &unreachable_fact, &unreachable_op,
                    &h2fwbw_mutex, &h2fwbw_unreachable_fact, &h2fwbw_unreachable_op);

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
 * h^2 forward-only with mutex output disabled (res.mutex_pairs = NULL).
 * Verifies that unreachable facts/ops are still correctly computed when
 * the caller is not interested in collecting mutex pairs.
 */
TEST(hm_h2_fw_null_mutex, hm_h2_fw)
{
    PDDL_ISET(unreachable_fact);
    PDDL_ISET(unreachable_op);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    /* mutex_pairs intentionally left NULL */
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* Must still detect all unreachable facts/ops found by h2fw */
    assert_stronger(NULL, &unreachable_fact, &unreachable_op,
                    NULL, &h2fw_unreachable_fact, &h2fw_unreachable_op);

    pddlISetFree(&unreachable_fact);
    pddlISetFree(&unreachable_op);
}

/*
 * h^2 forward-only with unreachable output disabled
 * (res.unreachable_facts = NULL, res.unreachable_ops = NULL).
 * Verifies that mutex pairs are still correctly computed when the caller
 * is not interested in tracking unreachable facts/ops.
 */
TEST(hm_h2_fw_null_unreachable, hm_h2_fw)
{
    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &C.strips);

    pddl_hm_mutex_config_t cfg = PDDL_HM_MUTEX_CONFIG_INIT;
    cfg.m = 2;
    cfg.dir = PDDL_HM_MUTEX_DIR_FW;
    cfg.strips = &C.strips;
    cfg.mutex_pairs = &mutex;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;
    /* unreachable_facts and unreachable_ops intentionally left NULL */

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* Must still detect all mutex pairs found by h2fw */
    assert_stronger(&mutex, NULL, NULL, &h2fw_mutex, NULL, NULL);

    pddlMutexPairsFree(&mutex);
}

/*
 * h^2 fwbw with disambiguation of forward operators' preconditions enabled.
 * This is an additional strengthening on top of the default DISAMB_STRONG,
 * so results must be at least as strong as plain h2fwbw.
 */
TEST(hm_h2_fwbw_disamb_fw_op_pre, hm_h2_fwbw)
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
    cfg.disambiguate_fw_op_pre = pddl_true;
    cfg.mutex_pairs = &mutex;
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* Enabling fw_op_pre disambiguation must produce >= h2fwbw results */
    assert_stronger(&mutex, &unreachable_fact, &unreachable_op,
                    &h2fwbw_mutex, &h2fwbw_unreachable_fact, &h2fwbw_unreachable_op);

    if (mutex.num_mutex_pairs > h2fwbw_mutex.num_mutex_pairs){
        fprintf(stdout, "h2fwbw disamb_fw_op_pre extra mutex pairs: %lu -> %lu\n",
                (unsigned long)h2fwbw_mutex.num_mutex_pairs,
                (unsigned long)mutex.num_mutex_pairs);
    }

    PDDL_ISET(extra_fact);
    pddlISetMinus2(&extra_fact, &unreachable_fact, &h2fwbw_unreachable_fact);
    if (pddlISetSize(&extra_fact) > 0){
        fprintf(stdout, "h2fwbw disamb_fw_op_pre extra unreachable facts [%d]:\n",
                pddlISetSize(&extra_fact));
        PDDL_ISET_FOR_EACH(&extra_fact, fact)
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
    }
    pddlISetFree(&extra_fact);

    PDDL_ISET(extra_op);
    pddlISetMinus2(&extra_op, &unreachable_op, &h2fwbw_unreachable_op);
    if (pddlISetSize(&extra_op) > 0){
        fprintf(stdout, "h2fwbw disamb_fw_op_pre extra unreachable ops [%d]:\n",
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
 * h^2 fwbw with disambiguation of backward operators' preconditions enabled.
 * This is an additional strengthening on top of the default DISAMB_STRONG,
 * so results must be at least as strong as plain h2fwbw.
 */
TEST(hm_h2_fwbw_disamb_bw_op_pre, hm_h2_fwbw)
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
    cfg.disambiguate_bw_op_pre = pddl_true;
    cfg.mutex_pairs = &mutex;
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* Enabling bw_op_pre disambiguation must produce >= h2fwbw results */
    assert_stronger(&mutex, &unreachable_fact, &unreachable_op,
                    &h2fwbw_mutex, &h2fwbw_unreachable_fact, &h2fwbw_unreachable_op);

    if (mutex.num_mutex_pairs > h2fwbw_mutex.num_mutex_pairs){
        fprintf(stdout, "h2fwbw disamb_bw_op_pre extra mutex pairs: %lu -> %lu\n",
                (unsigned long)h2fwbw_mutex.num_mutex_pairs,
                (unsigned long)mutex.num_mutex_pairs);
    }

    PDDL_ISET(extra_fact);
    pddlISetMinus2(&extra_fact, &unreachable_fact, &h2fwbw_unreachable_fact);
    if (pddlISetSize(&extra_fact) > 0){
        fprintf(stdout, "h2fwbw disamb_bw_op_pre extra unreachable facts [%d]:\n",
                pddlISetSize(&extra_fact));
        PDDL_ISET_FOR_EACH(&extra_fact, fact)
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
    }
    pddlISetFree(&extra_fact);

    PDDL_ISET(extra_op);
    pddlISetMinus2(&extra_op, &unreachable_op, &h2fwbw_unreachable_op);
    if (pddlISetSize(&extra_op) > 0){
        fprintf(stdout, "h2fwbw disamb_bw_op_pre extra unreachable ops [%d]:\n",
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
 * h^2 fwbw with all *_use_dead flags disabled.
 * Disabling use of dead facts from disambiguation weakens the analysis,
 * placing the result between h2fw (lower bound) and h2fwbw (upper bound).
 */
TEST(hm_h2_fwbw_no_dead, hm_h2_fwbw)
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
    cfg.disambiguate_fw_op_pre_use_dead = pddl_false;
    cfg.disambiguate_fw_op_prevail_add_use_dead = pddl_false;
    cfg.disambiguate_bw_op_pre_use_dead = pddl_false;
    cfg.disambiguate_bw_init_use_dead = pddl_false;
    cfg.mutex_pairs = &mutex;
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* no_dead must be at least as strong as forward-only h^2 */
    assert_stronger(&mutex, &unreachable_fact, &unreachable_op,
                    &h2fw_mutex, &h2fw_unreachable_fact, &h2fw_unreachable_op);

    /* full h2fwbw must be at least as strong as no_dead */
    assert_stronger(&h2fwbw_mutex, &h2fwbw_unreachable_fact, &h2fwbw_unreachable_op,
                    &mutex, &unreachable_fact, &unreachable_op);

    pddlMutexPairsFree(&mutex);
    pddlISetFree(&unreachable_fact);
    pddlISetFree(&unreachable_op);
}

/*
 * h^2 fwbw using the FDR task transformation.
 * Builds FDR variables from STRIPS + mutex groups + known mutex pairs,
 * then derives MG-STRIPS from the FDR encoding as input to h^2.
 * Results (mapped back to original fact IDs) must be at least as strong
 * as forward-only h^2.
 */
TEST(hm_h2_fwbw_task_fdr, hm_h2_fwbw)
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
    cfg.task = PDDL_HM_MUTEX_TASK_FDR;
    cfg.mutex_pairs = &mutex;  /* required for TASK_FDR */
    cfg.unreachable_facts = &unreachable_fact;
    cfg.unreachable_ops = &unreachable_op;

    pddl_hm_mutex_result_t res = PDDL_HM_MUTEX_RESULT_INIT;
    res.mutex_pairs = &mutex;
    res.unreachable_facts = &unreachable_fact;
    res.unreachable_ops = &unreachable_op;

    int ret = pddlHm(&cfg, &res, &C.err);
    assert(ret == 0);

    /* FDR task must produce results at least as strong as forward-only h^2 */
    assert_stronger(&mutex, &unreachable_fact, &unreachable_op,
                    &h2fw_mutex, &h2fw_unreachable_fact, &h2fw_unreachable_op);

    if (mutex.num_mutex_pairs > h2fw_mutex.num_mutex_pairs){
        fprintf(stdout, "h2fwbw task_fdr extra mutex pairs: %lu -> %lu\n",
                (unsigned long)h2fw_mutex.num_mutex_pairs,
                (unsigned long)mutex.num_mutex_pairs);
    }else{
        fprintf(stdout, "h2fwbw task_fdr mutex pairs: %lu\n",
                (unsigned long)mutex.num_mutex_pairs);
    }

    PDDL_ISET(extra_fact);
    pddlISetMinus2(&extra_fact, &unreachable_fact, &h2fw_unreachable_fact);
    if (pddlISetSize(&extra_fact) > 0){
        fprintf(stdout, "h2fwbw task_fdr extra unreachable facts [%d]:\n",
                pddlISetSize(&extra_fact));
        PDDL_ISET_FOR_EACH(&extra_fact, fact)
            fprintf(stdout, "  (%s)\n", C.strips.fact.fact[fact]->name);
    }
    pddlISetFree(&extra_fact);

    PDDL_ISET(extra_op);
    pddlISetMinus2(&extra_op, &unreachable_op, &h2fw_unreachable_op);
    if (pddlISetSize(&extra_op) > 0){
        fprintf(stdout, "h2fwbw task_fdr extra unreachable ops [%d]:\n",
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
    assert_stronger(&mutex, &unreachable_fact, &unreachable_op,
                    &h2fw_mutex, &h2fw_unreachable_fact, &h2fw_unreachable_op);

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
