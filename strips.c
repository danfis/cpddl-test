#include <assert.h>
#include "test.h"
#include "context.h"

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

    BOR_ISET(rm_fact);
    BOR_ISET(rm_op);
    if (pddlIrrelevanceAnalysis(&C.strips, &rm_fact, &rm_op, NULL, &C.err) != 0){
        BOR_INFO2(&C.err, "Irrelevance analysis failed.");
        fprintf(stderr, "Error: ");
        borErrPrint(&C.err, 1, stderr);
        return;
    }
    if (borISetSize(&rm_fact) > 0 || borISetSize(&rm_op) > 0){
        pddlStripsReduce(&C.strips, &rm_fact, &rm_op);
        if (borISetSize(&rm_fact) > 0){
            pddlMGroupsReduce(&C.mg, &rm_fact);
            pddlMGroupsSetExactlyOne(&C.mg, &C.strips);
            pddlMGroupsSetGoal(&C.mg, &C.strips);
            //fprintf(stdout, "---\n");
            //pddlMGroupsPrint(&C.pddl, &C.strips, &C.mg, stdout);
        }
    }
    borISetFree(&rm_fact);
    borISetFree(&rm_op);

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

    BOR_ISET(rm_fact);
    BOR_ISET(rm_op);
    int ret = pddlH2FwBw(&C.strips, &C.mg, &C.mutex, &rm_fact, &rm_op, 0., &C.err);
    assert(ret == 0);

    if (borISetSize(&rm_fact) > 0 || borISetSize(&rm_op) > 0){
        pddlStripsReduce(&C.strips, &rm_fact, &rm_op);
        if (borISetSize(&rm_fact) > 0){
            pddlMGroupsReduce(&C.mg, &rm_fact);
            pddlMGroupsSetExactlyOne(&C.mg, &C.strips);
            pddlMGroupsSetGoal(&C.mg, &C.strips);
            pddlMutexPairsReduce(&C.mutex, &rm_fact);
        }
    }
    borISetFree(&rm_fact);
    borISetFree(&rm_op);

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

    BOR_ISET(rm_fact);
    BOR_ISET(rm_op);
    int ret = pddlH2FwBw(&strips, &C.mg, &mutex, &rm_fact, &rm_op, 0., &C.err);
    assert(ret == 0);

    if (borISetSize(&rm_fact) > 0 || borISetSize(&rm_op) > 0)
        pddlStripsReduce(&strips, &rm_fact, &rm_op);
    borISetFree(&rm_fact);
    borISetFree(&rm_op);

    BOR_ISET(changed_op);
    pddlStripsRemoveUselessDelEffs(&strips, &mutex, NULL, &C.err);

    int op;
    BOR_ISET_FOR_EACH(&changed_op, op)
        printf("(%s)\n", strips.op.op[op]->name);
    borISetFree(&changed_op);

    pddlMutexPairsFree(&mutex);
    pddlStripsFree(&strips);
}
