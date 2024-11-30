#include "test.h"
#include "context.h"
#include <assert.h>

TEST(strips, lmg)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    int ret = pddlStripsGroundDatalog(&C.strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    C.strips_set = 1;

    if (C.strips.has_cond_eff)
        pddlStripsCompileAwayCondEff(&C.strips);
    assert(!C.strips.has_cond_eff);

    pddlMGroupsGround(&C.mg, &C.pddl, &C.lmg, &C.strips);
    C.mg_set = 1;
    pddlMGroupsSetExactlyOne(&C.mg, &C.strips);
    pddlMGroupsSetGoal(&C.mg, &C.strips);
    //pddlMGroupsPrint(&C.pddl, &C.strips, &C.mg, stdout);

    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    if (pddlIrrelevanceAnalysis(&C.strips, &rm_fact, &rm_op, NULL, &C.err) != 0){
        PDDL_LOG(&C.err, "Irrelevance analysis failed.");
        fprintf(stderr, "Error: ");
        pddlErrPrint(&C.err, 1, stderr);
        return;
    }
    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0){
        pddlStripsReduce(&C.strips, &rm_fact, &rm_op);
        if (pddlISetSize(&rm_fact) > 0){
            pddlMGroupsReduce(&C.mg, &rm_fact);
            pddlMGroupsSetExactlyOne(&C.mg, &C.strips);
            pddlMGroupsSetGoal(&C.mg, &C.strips);
            //fprintf(stdout, "---\n");
            //pddlMGroupsPrint(&C.pddl, &C.strips, &C.mg, stdout);
        }
    }
    pddlISetFree(&rm_fact);
    pddlISetFree(&rm_op);

    if (C.strips.op.op_size == 0){
        pddlStripsMakeUnsolvable(&C.strips);
        pddlMutexPairsFree(&C.mutex);
        pddlMutexPairsInitStrips(&C.mutex, &C.strips);
        pddlMGroupsFree(&C.mg);
        pddlMGroupsInitEmpty(&C.mg);
    }
    pddlStripsPrintDebug(&C.strips, stdout);
}

TEST(strips_ground_only_facts, lmg)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.remove_static_facts = pddl_false;
    ground_cfg.keep_all_static_facts = pddl_false;
    ground_cfg.ground_only_facts = pddl_false;

    pddl_strips_t strips_ops;
    int ret = pddlStripsGroundDatalog(&strips_ops, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    pddl_strips_t strips_facts;
    ground_cfg.ground_only_facts = pddl_true;
    ret = pddlStripsGroundDatalog(&strips_facts, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    assert(strips_ops.fact.fact_size == strips_facts.fact.fact_size);
    assert(strips_facts.op.op_size == 0);

    int remap[strips_ops.fact.fact_size];
    pddlFactsSort(&strips_ops.fact, remap);
    pddlFactsSort(&strips_facts.fact, remap);

    for (int i = 0; i < strips_ops.fact.fact_size; ++i){
        assert(strcmp(strips_ops.fact.fact[i]->name,
                      strips_facts.fact.fact[i]->name) == 0);
    }

    pddlStripsFree(&strips_facts);
    pddlStripsFree(&strips_ops);
}

TEST(strips_ground_only_facts_with_static, lmg)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.remove_static_facts = pddl_false;
    ground_cfg.keep_all_static_facts = pddl_true;
    ground_cfg.ground_only_facts = pddl_false;

    pddl_strips_t strips_ops;
    int ret = pddlStripsGroundDatalog(&strips_ops, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    pddl_strips_t strips_facts;
    ground_cfg.ground_only_facts = pddl_true;
    ret = pddlStripsGroundDatalog(&strips_facts, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    assert(strips_ops.fact.fact_size == strips_facts.fact.fact_size);
    assert(strips_facts.op.op_size == 0);

    int remap[strips_ops.fact.fact_size];
    pddlFactsSort(&strips_ops.fact, remap);
    pddlFactsSort(&strips_facts.fact, remap);

    for (int i = 0; i < strips_ops.fact.fact_size; ++i){
        assert(strcmp(strips_ops.fact.fact[i]->name,
                      strips_facts.fact.fact[i]->name) == 0);
    }

    pddlStripsFree(&strips_facts);
    pddlStripsFree(&strips_ops);
}


TEST(strips_ground_unit_cost, pddl_unit_cost)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    int ret = pddlStripsGroundDatalog(&C.strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    C.strips_set = 1;

    if (C.strips.has_cond_eff)
        pddlStripsCompileAwayCondEff(&C.strips);
    assert(!C.strips.has_cond_eff);
    pddlStripsPrintDebug(&C.strips, stdout);
}

TEST(strips_ce, lmg)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    int ret = pddlStripsGroundDatalog(&C.strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    C.strips_set = 1;

    pddlMGroupsGround(&C.mg, &C.pddl, &C.lmg, &C.strips);
    C.mg_set = 1;
    pddlMGroupsSetExactlyOne(&C.mg, &C.strips);
    pddlMGroupsSetGoal(&C.mg, &C.strips);
    //pddlMGroupsPrint(&C.pddl, &C.strips, &C.mg, stdout);

    if (C.strips.op.op_size == 0){
        pddlStripsMakeUnsolvable(&C.strips);
        pddlMutexPairsFree(&C.mutex);
        pddlMutexPairsInitStrips(&C.mutex, &C.strips);
        pddlMGroupsFree(&C.mg);
        pddlMGroupsInitEmpty(&C.mg);
    }
    pddlStripsPrintDebug(&C.strips, stdout);
}

TEST(strips_pruned, strips)
{
    pddlMutexPairsInitStrips(&C.mutex, &C.strips);
    C.mutex_set = 1;

    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    int ret = pddlH2FwBw(&C.strips, &C.mg, &C.mutex, &rm_fact, &rm_op, 0., &C.err);
    assert(ret == 0);

    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0){
        pddlStripsReduce(&C.strips, &rm_fact, &rm_op);
        if (pddlISetSize(&rm_fact) > 0){
            pddlMGroupsReduce(&C.mg, &rm_fact);
            pddlMGroupsSetExactlyOne(&C.mg, &C.strips);
            pddlMGroupsSetGoal(&C.mg, &C.strips);
            pddlMutexPairsReduce(&C.mutex, &rm_fact);
        }
    }
    pddlISetFree(&rm_fact);
    pddlISetFree(&rm_op);

    pddlStripsRemoveUselessDelEffs(&C.strips, &C.mutex, NULL, &C.err);

    if (C.strips.op.op_size == 0){
        pddlStripsMakeUnsolvable(&C.strips);
        pddlMutexPairsFree(&C.mutex);
        pddlMutexPairsInitStrips(&C.mutex, &C.strips);
        pddlMGroupsFree(&C.mg);
        pddlMGroupsInitEmpty(&C.mg);
    }
    pddlStripsPrintDebug(&C.strips, stdout);
}

TEST(strips_ground_prune, lmg)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.lifted_mgroups = &C.lmg;
    ground_cfg.prune_op_pre_mutex = 1;
    ground_cfg.prune_op_dead_end = 1;
    pddl_strips_t strips;
    int ret = pddlStripsGroundTrie(&strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    pddlStripsPrintDebug(&strips, stdout);
    pddlStripsFree(&strips);
}


TEST(strips_useless_del_effs, strips)
{
    pddl_strips_t strips;
    pddlStripsInitCopy(&strips, &C.strips);

    pddl_mutex_pairs_t mutex;
    pddlMutexPairsInitStrips(&mutex, &strips);

    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    int ret = pddlH2FwBw(&strips, &C.mg, &mutex, &rm_fact, &rm_op, 0., &C.err);
    assert(ret == 0);

    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0)
        pddlStripsReduce(&strips, &rm_fact, &rm_op);
    pddlISetFree(&rm_fact);
    pddlISetFree(&rm_op);

    PDDL_ISET(changed_op);
    pddlStripsRemoveUselessDelEffs(&strips, &mutex, NULL, &C.err);

    int op;
    PDDL_ISET_FOR_EACH(&changed_op, op)
        printf("(%s)\n", strips.op.op[op]->name);
    pddlISetFree(&changed_op);

    pddlMutexPairsFree(&mutex);
    pddlStripsFree(&strips);
}

TEST_COND(ground_layered, lmg, SQLITE)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    pddl_ground_atoms_t ga;
    pddlGroundAtomsInit(&ga);
    int ret = pddlStripsGroundSqlLayered(&C.pddl, &ground_cfg, 2, INT_MAX,
                                         NULL, &ga, &C.err);
    assert(ret == 0);

    pddlGroundAtomsPrint(&ga, &C.pddl, stdout);
    pddlGroundAtomsFree(&ga);
}

static void checkGroundingEqual(int (*ground)(pddl_strips_t *strips,
                                              const pddl_t *pddl,
                                              const pddl_ground_config_t *cfg,
                                              pddl_err_t *err),
                                const pddl_t *pddl,
                                const pddl_ground_config_t *ground_cfg,
                                const pddl_strips_t *base)
{
    pddl_strips_t strips;
    int ret = ground(&strips, pddl, ground_cfg, &C.err);
    if (ret != 0)
        pddlErrPrint(&C.err, 1, stderr);
    assert(ret == 0);

    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    pddlIrrelevanceAnalysis(&strips, &rm_fact, &rm_op, NULL, NULL);
    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0)
        pddlStripsReduce(&strips, &rm_fact, &rm_op);
    pddlISetFree(&rm_fact);
    pddlISetFree(&rm_op);

    assert(strips.op.op_size == base->op.op_size);
    for (int i = 0; i < strips.op.op_size; ++i){
        assert(strcmp(strips.op.op[i]->name, base->op.op[i]->name) == 0);
        assert(strips.op.op[i]->cost == base->op.op[i]->cost);
    }
    assert(strips.fact.fact_size == base->fact.fact_size);
    for (int i = 0; i < strips.fact.fact_size; ++i)
        assert(strcmp(strips.fact.fact[i]->name, base->fact.fact[i]->name) == 0);
    pddlStripsFree(&strips);
}

static void testCompileInLMG(int mutex, int dead_end)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.lifted_mgroups = &C.lmg;
    ground_cfg.prune_op_pre_mutex = mutex;
    ground_cfg.prune_op_dead_end = dead_end;
    pddl_strips_t base;
    int ret = pddlStripsGroundTrie(&base, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    pddlIrrelevanceAnalysis(&base, &rm_fact, &rm_op, NULL, NULL);
    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0)
        pddlStripsReduce(&base, &rm_fact, &rm_op);
    pddlISetFree(&rm_fact);
    pddlISetFree(&rm_op);

    ground_cfg.lifted_mgroups = NULL;
    ground_cfg.prune_op_pre_mutex = 0;
    ground_cfg.prune_op_dead_end = 0;

    pddl_t pddl;
    pddlInitCopy(&pddl, &C.pddl);

    pddl_compile_in_lmg_config_t lmg_cfg = PDDL_COMPILE_IN_LMG_CONFIG_INIT;
    lmg_cfg.prune_mutex = mutex;
    lmg_cfg.prune_dead_end = dead_end;
    pddlCompileInLiftedMGroups(&pddl, &C.lmg, &lmg_cfg, &C.err);

    checkGroundingEqual(pddlStripsGroundDatalog, &pddl, &ground_cfg, &base);
#ifdef PDDL_SQL
    checkGroundingEqual(pddlStripsGroundSql, &pddl, &ground_cfg, &base);
#endif
    checkGroundingEqual(pddlStripsGroundTrie, &pddl, &ground_cfg, &base);

#ifdef PDDL_CLINGO
    checkGroundingEqual(pddlStripsGroundGringo, &pddl, &ground_cfg, &base);
    // Skip these tasks because clingo tends to run out of memory there
    if (strcmp(TEST_TASK, "ipc-2006/pathways/p20") != 0
            && strcmp(TEST_TASK, "unsolve-ipc-2016/pegsol-row5/satprob05") != 0
            && strcmp(TEST_TASK, "ipc-2008/seq-sat/sokoban/p10") != 0
            && strcmp(TEST_TASK, "unsolve-ipc-2016/bottleneck/prob01") != 0
            && strcmp(TEST_TASK, "ipc-2023/opt/slitherlink/p10") != 0){
        checkGroundingEqual(pddlStripsGroundClingo, &pddl, &ground_cfg, &base);
    }
#endif /* PDDL_CLINGO */

    pddlFree(&pddl);
    pddlStripsFree(&base);
}

TEST(strips_compile_in_lmg, lmg)
{
    testCompileInLMG(1, 1);
    testCompileInLMG(1, 0);
    testCompileInLMG(0, 1);
}

TEST(strips_grounding, pddl)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.lifted_mgroups = NULL;
    ground_cfg.prune_op_pre_mutex = 0;
    ground_cfg.prune_op_dead_end = 0;
    pddl_strips_t base;
    int ret = pddlStripsGroundTrie(&base, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    PDDL_ISET(rm_fact);
    PDDL_ISET(rm_op);
    pddlIrrelevanceAnalysis(&base, &rm_fact, &rm_op, NULL, NULL);
    if (pddlISetSize(&rm_fact) > 0 || pddlISetSize(&rm_op) > 0)
        pddlStripsReduce(&base, &rm_fact, &rm_op);
    pddlISetFree(&rm_fact);
    pddlISetFree(&rm_op);

    checkGroundingEqual(pddlStripsGroundDatalog, &C.pddl, &ground_cfg, &base);
#ifdef PDDL_SQL
    checkGroundingEqual(pddlStripsGroundSql, &C.pddl, &ground_cfg, &base);
#endif
    checkGroundingEqual(pddlStripsGroundTrie, &C.pddl, &ground_cfg, &base);

#ifdef PDDL_CLINGO
    checkGroundingEqual(pddlStripsGroundGringo, &C.pddl, &ground_cfg, &base);
    // Skip these tasks because clingo tends to run out of memory there
    if (strcmp(TEST_TASK, "ipc-2006/pathways/p20") != 0
            && strcmp(TEST_TASK, "ipc-2008/seq-sat/sokoban/p10") != 0
            && strcmp(TEST_TASK, "unsolve-ipc-2016/pegsol-row5/satprob05") != 0
            && strcmp(TEST_TASK, "ipc-2023/opt/slitherlink/p10") != 0){
        checkGroundingEqual(pddlStripsGroundClingo, &C.pddl, &ground_cfg, &base);
    }
#endif /* PDDL_CLINGO */

    pddlStripsFree(&base);
}

static pddl_strips_conj_t stripsc;
static int stripsc_set = 0;
TEST(strips_conj, strips_pruned)
{
    if (C.strips.fact.fact_size <= 4 || C.strips.goal_is_unreachable)
        return;

    pddl_strips_conj_config_t cfg;
    pddlStripsConjConfigInit(&cfg);

    int f0 = -1, f1 = -1, f2 = -1, f3 = -1;
    for (f0 = 0; f0 < C.strips.fact.fact_size && f1 < 0; ++f0){
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

    for (f3 = 0; f3 < C.strips.fact.fact_size; ++f3){
        if (f3 == f0 || f3 == f1 || f3 == f2)
            continue;
        if (!pddlMutexPairsIsMutex(&C.mutex, f1, f3))
            break;
    }
    if (f3 >= C.strips.fact.fact_size)
        f3 = -1;

    if (f1 < 0)
        return;

    PDDL_ISET(set);
    pddlISetAdd(&set, f0);
    pddlISetAdd(&set, f1);
    pddlStripsConjConfigAddConj(&cfg, &set);
    if (pddlMutexPairsIsMutexSet(&C.mutex, &set))
        fprintf(stderr, "mutex f0, f1: (%s) (%s)\n",
                C.strips.fact.fact[f0]->name,
                C.strips.fact.fact[f1]->name);

    pddlISetEmpty(&set);
    pddlISetAdd(&set, f0);
    pddlISetAdd(&set, f2);
    pddlStripsConjConfigAddConj(&cfg, &set);
    if (pddlMutexPairsIsMutexSet(&C.mutex, &set))
        fprintf(stderr, "mutex f0, f2: (%s) (%s)\n",
                C.strips.fact.fact[f0]->name,
                C.strips.fact.fact[f2]->name);

    pddlISetEmpty(&set);
    pddlISetAdd(&set, f0);
    pddlISetAdd(&set, f1);
    pddlISetAdd(&set, f2);
    pddlStripsConjConfigAddConj(&cfg, &set);
    if (pddlMutexPairsIsMutexSet(&C.mutex, &set))
        fprintf(stderr, "mutex f0, f1, f2: (%s) (%s) (%s)\n",
                C.strips.fact.fact[f0]->name,
                C.strips.fact.fact[f1]->name,
                C.strips.fact.fact[f2]->name);

    if (f3 >= 0 && f3 < C.strips.fact.fact_size){
        pddlISetEmpty(&set);
        pddlISetAdd(&set, f1);
        pddlISetAdd(&set, f3);
        pddlStripsConjConfigAddConj(&cfg, &set);
        if (pddlMutexPairsIsMutexSet(&C.mutex, &set))
            fprintf(stderr, "mutex f1, f3: (%s) (%s)\n",
                    C.strips.fact.fact[f1]->name,
                    C.strips.fact.fact[f3]->name);

        pddlISetEmpty(&set);
        pddlISetAdd(&set, f0);
        pddlISetAdd(&set, f1);
        pddlISetAdd(&set, f2);
        pddlISetAdd(&set, f3);
        pddlStripsConjConfigAddConj(&cfg, &set);
        if (pddlMutexPairsIsMutexSet(&C.mutex, &set))
            fprintf(stderr, "mutex f0-f3: (%s) (%s) (%s) (%s)\n",
                    C.strips.fact.fact[f0]->name,
                    C.strips.fact.fact[f1]->name,
                    C.strips.fact.fact[f2]->name,
                    C.strips.fact.fact[f3]->name);
    }
    pddlISetFree(&set);

    cfg.mutex = &C.mutex;
    pddlStripsConjInit(&stripsc, &C.strips, &cfg, &C.err);
    stripsc_set = 1;

    for (int fact_id = 0; fact_id < stripsc.strips.fact.fact_size; ++fact_id){
        if (fact_id < stripsc.num_singletons){
            //assert(!stripsc.strips.fact.fact[fact_id]->is_conjunction);
        }else{
            //assert(stripsc.strips.fact.fact[fact_id]->is_conjunction);
            assert(pddlISetSize(stripsc.fact_to_conj + fact_id) >= 2);
        }
    }

    pddlStripsPrintDebug(&stripsc.strips, stdout);
    //pddlStripsPrintDebug(&C.strips, stdout);
    pddlStripsConjConfigFree(&cfg);
}

TEST_TEAR_DOWN(strips_conj)
{
    if (stripsc_set)
        pddlStripsConjFree(&stripsc);
}

static void testStripsConjHMax(const pddl_strips_conj_t *stripsc)
{
    pddl_hmax_t hmax, hmaxc;
    pddlHMaxInitStrips(&hmax, &C.strips);
    pddlHMaxInitStrips(&hmaxc, &stripsc->strips);
    int h = pddlHMaxStrips(&hmax, &C.strips.init);
    int hc = pddlHMaxStrips(&hmaxc, &stripsc->strips.init);
    assert(h <= hc);
    if (C.optimal_cost >= 0){
        if (hc > C.optimal_cost)
            fprintf(stderr, "%d > %d\n", hc, C.optimal_cost);
        assert(hc <= C.optimal_cost);
    }
    pddlHMaxFree(&hmax);
    pddlHMaxFree(&hmaxc);
}

TEST(strips_conj_hmax, strips_conj)
{
    if (!stripsc_set)
        return;

    testStripsConjHMax(&stripsc);
}

TEST(strips_conj_hmax_rand, strips_pruned)
{
    if (C.strips.fact.fact_size <= 10 || C.strips.goal_is_unreachable)
        return;

    pddl_rand_t rnd;
    pddlRandInitAuto(&rnd);
    for (int _ = 0; _ < 20; ++_){
        int f1 = pddlRand(&rnd, 0, C.strips.fact.fact_size);
        int f2 = pddlRand(&rnd, 0, C.strips.fact.fact_size);
        assert(f1 >= 0 && f1 < C.strips.fact.fact_size);
        assert(f2 >= 0 && f2 < C.strips.fact.fact_size);
        if (f1 == f2)
            continue;
        if (pddlMutexPairsIsMutex(&C.mutex, f1, f2))
            continue;

        pddl_strips_conj_config_t cfg;
        pddlStripsConjConfigInit(&cfg);
        PDDL_ISET(set);
        pddlISetAdd(&set, f1);
        pddlISetAdd(&set, f2);
        pddlStripsConjConfigAddConj(&cfg, &set);
        pddlISetFree(&set);

        pddl_strips_conj_t stripsc;
        cfg.mutex = &C.mutex;
        pddlStripsConjInit(&stripsc, &C.strips, &cfg, &C.err);
        testStripsConjHMax(&stripsc);
        pddlStripsConjFree(&stripsc);
        pddlStripsConjConfigFree(&cfg);

        for (int __ = 0; __ < 10; ++__){
            int f3 = pddlRand(&rnd, 0, C.strips.fact.fact_size);
            int f4 = pddlRand(&rnd, 0, C.strips.fact.fact_size);
            int f5 = pddlRand(&rnd, 0, C.strips.fact.fact_size);
            if (f3 == f4 || f3 == f5 || f4 == f5)
                continue;
            if (pddlMutexPairsIsMutex(&C.mutex, f3, f4)
                    || pddlMutexPairsIsMutex(&C.mutex, f3, f5)
                    || pddlMutexPairsIsMutex(&C.mutex, f4, f5)){
                continue;
            }
            pddl_strips_conj_config_t cfg;
            pddlStripsConjConfigInit(&cfg);
            PDDL_ISET(set);
            pddlISetAdd(&set, f1);
            pddlISetAdd(&set, f2);
            pddlStripsConjConfigAddConj(&cfg, &set);

            pddlISetEmpty(&set);
            pddlISetAdd(&set, f3);
            pddlISetAdd(&set, f4);
            pddlISetAdd(&set, f5);
            pddlStripsConjConfigAddConj(&cfg, &set);
            pddlISetFree(&set);

            pddl_strips_conj_t stripsc;
            cfg.mutex = &C.mutex;
            pddlStripsConjInit(&stripsc, &C.strips, &cfg, &C.err);
            testStripsConjHMax(&stripsc);
            pddlStripsConjFree(&stripsc);
            pddlStripsConjConfigFree(&cfg);
        }
    }
}


TEST(strips_conj_hadd, strips_conj)
{
    if (!stripsc_set)
        return;

    pddl_hadd_t hadd, haddc;
    pddlHAddInitStrips(&hadd, &C.strips);
    pddlHAddInitStrips(&haddc, &stripsc.strips);
    int h = pddlHAddStrips(&hadd, &C.strips.init);
    int hc = pddlHAddStrips(&haddc, &stripsc.strips.init);
    assert(h <= hc);
    pddlHAddFree(&hadd);
    pddlHAddFree(&haddc);
}

TEST_COND(strips_conj_hflow, strips_conj, LP)
{
    if (!stripsc_set)
        return;

    pddl_fdr_config_t cfg = PDDL_FDR_CONFIG_INIT;
    cfg.var.alg = PDDL_FDR_VARS_ALG_LARGEST_FIRST;

    pddl_fdr_t fdr, fdrc;
    pddlFDRInitFromStrips(&fdr, &C.strips, &C.mg, &C.mutex, &cfg, &C.err);

    pddl_mutex_pairs_t mutexc;
    pddlStripsConjMutexPairsInitCopy(&mutexc, &C.mutex, &stripsc);
    pddlFDRInitFromStrips(&fdrc, &stripsc.strips, &C.mg, &mutexc, &cfg, &C.err);
    pddlMutexPairsFree(&mutexc);

    pddl_hflow_t hflow, hflowc;
    pddlHFlowInit(&hflow, &fdr, 0);
    pddlHFlowInit(&hflowc, &fdrc, 0);
    int h = pddlHFlow(&hflow, fdr.init, NULL);
    int hc = pddlHFlow(&hflowc, fdrc.init, NULL);
    assert(h <= hc);
    if (C.optimal_cost >= 0){
        if (hc > C.optimal_cost)
            fprintf(stderr, "%d > %d\n", hc, C.optimal_cost);
        assert(hc <= C.optimal_cost);
    }
    pddlHFlowFree(&hflow);
    pddlHFlowFree(&hflowc);

    pddlFDRFree(&fdr);
    pddlFDRFree(&fdrc);
}

TEST_COND(strips_conj_hpot, strips_conj, LP)
{
    if (!stripsc_set)
        return;

    pddl_fdr_config_t fdr_cfg = PDDL_FDR_CONFIG_INIT;
    fdr_cfg.var.alg = PDDL_FDR_VARS_ALG_LARGEST_FIRST;

    pddl_fdr_t fdr, fdrc;
    pddlFDRInitFromStrips(&fdr, &C.strips, &C.mg, &C.mutex, &fdr_cfg, &C.err);

    pddl_mutex_pairs_t mutexc;
    pddlStripsConjMutexPairsInitCopy(&mutexc, &C.mutex, &stripsc);
    pddlFDRInitFromStrips(&fdrc, &stripsc.strips, &C.mg, &mutexc, &fdr_cfg, &C.err);

    pddl_hpot_config_t cfg;
    pddl_hpot_config_opt_state_t cfg_init = PDDL_HPOT_CONFIG_OPT_STATE_INIT;
    pddlHPotConfigInit(&cfg);
    pddlHPotConfigAdd(&cfg, &cfg_init.cfg);

    pddl_pot_solutions_t sol, solc;
    pddlPotSolutionsInit(&sol);
    pddlPotSolutionsInit(&solc);

    cfg.fdr = &fdr;
    cfg.mutex = &C.mutex;
    pddlHPot(&sol, &cfg, &C.err);

    cfg.fdr = &fdrc;
    cfg.mutex = &mutexc;
    pddlHPot(&solc, &cfg, &C.err);
    pddlHPotConfigFree(&cfg);
    pddlMutexPairsFree(&mutexc);

    int h = pddlPotSolutionsEvalMaxFDRState(&sol, &fdr.var, fdr.init);
    int hc = pddlPotSolutionsEvalMaxFDRState(&solc, &fdrc.var, fdrc.init);
    assert(h <= hc);
    if (C.optimal_cost >= 0){
        if (hc > C.optimal_cost)
            fprintf(stderr, "%d > %d\n", hc, C.optimal_cost);
        assert(hc <= C.optimal_cost);
    }

    pddlPotSolutionsFree(&sol);
    pddlPotSolutionsFree(&solc);
    pddlFDRFree(&fdr);
    pddlFDRFree(&fdrc);
}
