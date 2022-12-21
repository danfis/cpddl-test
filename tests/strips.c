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
        PDDL_INFO(&C.err, "Irrelevance analysis failed.");
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
    int ret = pddlStripsGround(&strips, &C.pddl, &ground_cfg, &C.err);
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

static void testCompileInLMG(int mutex, int dead_end)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.lifted_mgroups = &C.lmg;
    ground_cfg.prune_op_pre_mutex = mutex;
    ground_cfg.prune_op_dead_end = dead_end;
    pddl_strips_t base;
    int ret = pddlStripsGround(&base, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    ground_cfg.lifted_mgroups = NULL;
    ground_cfg.prune_op_pre_mutex = 0;
    ground_cfg.prune_op_dead_end = 0;

    pddl_t pddl;
    pddlInitCopy(&pddl, &C.pddl);

    pddl_compile_in_lmg_config_t lmg_cfg = PDDL_COMPILE_IN_LMG_CONFIG_INIT;
    lmg_cfg.prune_mutex = mutex;
    lmg_cfg.prune_dead_end = dead_end;
    pddlCompileInLiftedMGroups(&pddl, &C.lmg, &lmg_cfg, &C.err);

    pddl_strips_t strips;
    ret = pddlStripsGroundDatalog(&strips, &pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    assert(strips.op.op_size == base.op.op_size);
    for (int i = 0; i < strips.op.op_size; ++i)
        assert(strcmp(strips.op.op[i]->name, base.op.op[i]->name) == 0);
    assert(strips.fact.fact_size == base.fact.fact_size);
    for (int i = 0; i < strips.fact.fact_size; ++i)
        assert(strcmp(strips.fact.fact[i]->name, base.fact.fact[i]->name) == 0);
    pddlStripsFree(&strips);

    ret = pddlStripsGroundSql(&strips, &pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    assert(strips.op.op_size == base.op.op_size);
    for (int i = 0; i < strips.op.op_size; ++i)
        assert(strcmp(strips.op.op[i]->name, base.op.op[i]->name) == 0);
    assert(strips.fact.fact_size == base.fact.fact_size);
    for (int i = 0; i < strips.fact.fact_size; ++i)
        assert(strcmp(strips.fact.fact[i]->name, base.fact.fact[i]->name) == 0);
    pddlStripsFree(&strips);

    ret = pddlStripsGround(&strips, &pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    assert(strips.op.op_size == base.op.op_size);
    for (int i = 0; i < strips.op.op_size; ++i)
        assert(strcmp(strips.op.op[i]->name, base.op.op[i]->name) == 0);
    assert(strips.fact.fact_size == base.fact.fact_size);
    for (int i = 0; i < strips.fact.fact_size; ++i)
        assert(strcmp(strips.fact.fact[i]->name, base.fact.fact[i]->name) == 0);
    pddlStripsFree(&strips);

    pddlFree(&pddl);
    pddlStripsFree(&base);
}

TEST(strips_compile_in_lmg, lmg)
{
    testCompileInLMG(1, 1);
    testCompileInLMG(1, 0);
    testCompileInLMG(0, 1);
}

static pddl_strips_conj_t stripsc;
TEST(strips_conj, strips)
{
    if (C.strips.fact.fact_size <= 4)
        return;

    pddl_strips_conj_config_t cfg;
    pddlStripsConjConfigInit(&cfg);
    PDDL_ISET(set);
    pddlISetAdd(&set, 0);
    pddlISetAdd(&set, 1);
    pddlStripsConjConfigAddConj(&cfg, &set);

    pddlISetEmpty(&set);
    pddlISetAdd(&set, 0);
    pddlISetAdd(&set, 2);
    pddlStripsConjConfigAddConj(&cfg, &set);

    pddlISetEmpty(&set);
    pddlISetAdd(&set, 0);
    pddlISetAdd(&set, 1);
    pddlISetAdd(&set, 2);
    pddlStripsConjConfigAddConj(&cfg, &set);

    pddlISetEmpty(&set);
    pddlISetAdd(&set, 1);
    pddlISetAdd(&set, 3);
    pddlStripsConjConfigAddConj(&cfg, &set);
    pddlISetFree(&set);

    pddlStripsConjInit(&stripsc, &C.strips, &cfg, &C.err);
    pddlStripsPrintDebug(&stripsc.strips, stdout);
    pddlStripsConjConfigFree(&cfg);
}

TEST_TEAR_DOWN(strips_conj)
{
    pddlStripsConjFree(&stripsc);
}

TEST(strips_conj_hmax, strips_conj)
{
    pddl_hmax_t hmax, hmaxc;
    pddlHMaxInitStrips(&hmax, &C.strips);
    pddlHMaxInitStrips(&hmaxc, &stripsc.strips);
    assert(pddlHMaxStrips(&hmax, &C.strips.init)
           <= pddlHMaxStrips(&hmaxc, &stripsc.strips.init));
    pddlHMaxFree(&hmax);
    pddlHMaxFree(&hmaxc);
}


TEST(strips_conj_hadd, strips_conj)
{
    pddl_hadd_t hadd, haddc;
    pddlHAddInitStrips(&hadd, &C.strips);
    pddlHAddInitStrips(&haddc, &stripsc.strips);
    assert(pddlHAddStrips(&hadd, &C.strips.init)
           <= pddlHAddStrips(&haddc, &stripsc.strips.init));
    pddlHAddFree(&hadd);
    pddlHAddFree(&haddc);
}
