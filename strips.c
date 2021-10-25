#include <assert.h>
#include "test.h"
#include "context.h"

TEST(strips, lifted_mgroup)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    int ret = pddlStripsGround(&C.strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    pddlStripsPrintDebug(&C.strips, stdout);
}

TEST_TEAR_DOWN(strips)
{
    pddlStripsFree(&C.strips);
}

TEST(strips_noce, lifted_mgroup)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    int ret = pddlStripsGround(&C.strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    pddlStripsCompileAwayCondEff(&C.strips);
    pddlStripsPrintDebug(&C.strips, stdout);
}

TEST_TEAR_DOWN(strips_noce)
{
    pddlStripsFree(&C.strips);
}

TEST(strips_prune, lifted_mgroup)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    ground_cfg.lifted_mgroups = &C.lmg;
    ground_cfg.prune_op_pre_mutex = 1;
    ground_cfg.prune_op_dead_end = 1;
    int ret = pddlStripsGround(&C.strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);

    /*
    char fn[128];
    FILE *fout;

    sprintf(fn, "reg/tmp.TSStripsGround.testStripsGround_%s."
                "useless_del_effs.out", outfn);
    fout = fopen(fn, "w");
    if (fout != NULL){
        pddl_strips_t strips2;
        pddlStripsInitCopy(&strips2, &strips);
        BOR_ISET(changed_ops);
        ret = pddlStripsRemoveUselessDelEffs(&strips2, NULL,
                                             &changed_ops, &err);
        assertEquals(ret, borISetSize(&changed_ops));

        int op_id;
        BOR_ISET_FOR_EACH(&changed_ops, op_id){
            pddlStripsOpPrintDebug(strips.op.op[op_id], &strips.fact, fout);
            pddlStripsOpPrintDebug(strips2.op.op[op_id], &strips2.fact, fout);
            fprintf(fout, "----\n");
        }
        borISetFree(&changed_ops);
        pddlStripsFree(&strips2);
        fclose(fout);
    }
    */
    pddlStripsPrintDebug(&C.strips, stdout);
}

TEST_TEAR_DOWN(strips_prune)
{
    pddlStripsFree(&C.strips);
}

TEST(strips_dl, lifted_mgroup)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    int ret = pddlStripsGroundDatalog(&C.strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    pddlStripsPrintDebug(&C.strips, stdout);
}

TEST_TEAR_DOWN(strips_dl)
{
    pddlStripsFree(&C.strips);
}

TEST(strips_sql, lifted_mgroup)
{
    pddl_ground_config_t ground_cfg = PDDL_GROUND_CONFIG_INIT;
    int ret = pddlStripsGroundDatalog(&C.strips, &C.pddl, &ground_cfg, &C.err);
    assert(ret == 0);
    pddlStripsPrintDebug(&C.strips, stdout);
}

TEST_TEAR_DOWN(strips_sql)
{
    pddlStripsFree(&C.strips);
}
