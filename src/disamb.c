#include "test.h"
#include "context.h"
#include <assert.h>

static pddl_mg_strips_t mg_strips;
static pddl_mutex_pairs_t mutex;


TEST(disamb_root, strips_pruned)
{
    int ret = pddlMGStripsInit(&mg_strips, &C.strips, &C.mg, &C.err);
    assert(ret == 0);

    pddlMutexPairsInitStrips(&mutex, &mg_strips.strips);
    pddlMutexPairsAddMGroups(&mutex, &mg_strips.mg);

    pddl_hm_mutex_config_t hmcfg = PDDL_HM_MUTEX_CONFIG_INIT;
    hmcfg.strips = &mg_strips.strips;
    hmcfg.mgroups = &mg_strips.mg;
    hmcfg.mutex_pairs = &mutex;

    pddl_hm_mutex_result_t hm_res = PDDL_HM_MUTEX_RESULT_INIT;
    hm_res.mutex_pairs = &mutex;
    ret = pddlHm(&hmcfg, &hm_res, &C.err);
    assert(ret == 0);
}

TEST_TEAR_DOWN(disamb_root)
{
    pddlMutexPairsFree(&mutex);
    pddlMGStripsFree(&mg_strips);
}

static int disamb(pddl_disamb_t *dis,
                  const pddl_strips_t *strips,
                  const pddl_iset_t *s1,
                  pddl_iset_t *s2,
                  const char *header)
{
    int st = pddlDisambPartState(dis, s2);
    if (st > 0){
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

    return st;
}

TEST(disambiguation, disamb_root)
{
    pddl_strips_t strips2;

    pddlStripsInitCopy(&strips2, &mg_strips.strips);

    pddl_disamb_t dis;
    pddlDisambInit(&dis, mg_strips.strips.fact.fact_size, &mutex,
                   &mg_strips.mg);
    if (disamb(&dis, &mg_strips.strips, &mg_strips.strips.goal,
               &strips2.goal, "Goal:") < 0){
        fprintf(stdout, "Unsolvable\n");
    }else{
        for (int op_id = 0; op_id < mg_strips.strips.op.op_size && op_id < 500; ++op_id){
            const pddl_strips_op_t *op = mg_strips.strips.op.op[op_id];
            pddl_strips_op_t *op2 = strips2.op.op[op_id];
            char header[128];
            snprintf(header, 128, "(%s)", op->name);
            header[127] = 0;
            if (disamb(&dis, &mg_strips.strips, &op->pre, &op2->pre, header) < 0)
                fprintf(stdout, "Unreachable: (%s)\n", op->name);
        }
    }
    pddlDisambFree(&dis);

    pddlStripsFree(&strips2);
}

static void disambTestMin(pddl_disamb_t *dis,
                          const pddl_strips_t *strips,
                          const pddl_iset_t *F,
                          const char *header)
{
    PDDL_ISET(sarc);
    PDDL_ISET(smin);
    pddlISetUnion(&sarc, F);
    pddlISetUnion(&smin, F);

    int st1 = pddlDisambAlgPartState(dis, PDDL_DISAMB_ARC_CONSISTENCY, &sarc);
    int st2 = pddlDisambAlgPartState(dis, PDDL_DISAMB_MINIMAL, &smin);
    // st1 == dead implies st2 == dead
    assert(st1 >= 0 || st2 < 0);
    // st1 == change implies st2 == change or st2 == dead
    assert(st1 <= 0 || (st2 > 0 || st2 < 0));
    // st2 == nochange implies st1 == nochange
    assert(st2 != 0 || st1 == 0);

    if (st2 >= 0){
        assert(st1 >= 0);
        assert(pddlISetIsSubset(&sarc, &smin));
    }

    if (F == &strips->goal && C.optimal_cost >= 0){
        assert(st1 >= 0 && st2 >= 0);
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
}

static void disambTestMinFull(pddl_disamb_t *dis,
                              const pddl_strips_t *strips,
                              const pddl_iset_t *F,
                              const char *header)
{
    pddl_bitset_t Fbs;
    pddlBitsetInit(&Fbs, strips->fact.fact_size);
    pddlBitsetOrISet(&Fbs, F);

    PDDL_ISET(sarc);
    PDDL_ISET(smin);
    pddlISetUnion(&sarc, F);
    pddlISetUnion(&smin, F);

    pddl_disamb_args_t args_ac = {
        .ps = F,
        .alg = PDDL_DISAMB_ARC_CONSISTENCY,
    };
    pddl_disamb_args_t bs_args_ac = {
        .bps = &Fbs,
        .alg = PDDL_DISAMB_ARC_CONSISTENCY,
    };
    PDDL_ISET(imp_ac);
    PDDL_BITSET(imp_ac_bs, strips->fact.fact_size);
    PDDL_ISET(dead_ac);
    PDDL_BITSET(dead_ac_bs, strips->fact.fact_size);
    pddl_set_iset_t dset_ac;
    pddlSetISetInit(&dset_ac);
    pddl_disamb_result_t res_ac = {
        .implied_facts = &imp_ac,
        .implied_facts_bs = &imp_ac_bs,
        .dead_facts = &dead_ac,
        .dead_facts_bs = &dead_ac_bs,
        .disamb = &dset_ac,
    };
    PDDL_ISET(bs_imp_ac);
    PDDL_BITSET(bs_imp_ac_bs, strips->fact.fact_size);
    PDDL_ISET(bs_dead_ac);
    PDDL_BITSET(bs_dead_ac_bs, strips->fact.fact_size);
    pddl_set_iset_t bs_dset_ac;
    pddlSetISetInit(&bs_dset_ac);
    pddl_disamb_result_t bs_res_ac = {
        .implied_facts = &bs_imp_ac,
        .implied_facts_bs = &bs_imp_ac_bs,
        .dead_facts = &bs_dead_ac,
        .dead_facts_bs = &bs_dead_ac_bs,
        .disamb = &bs_dset_ac,
    };


    pddl_disamb_args_t args_min = {
        .ps = F,
        .alg = PDDL_DISAMB_MINIMAL,
    };
    pddl_disamb_args_t bs_args_min = {
        .bps = &Fbs,
        .alg = PDDL_DISAMB_MINIMAL,
    };
    PDDL_ISET(imp_min);
    PDDL_BITSET(imp_min_bs, strips->fact.fact_size);
    PDDL_ISET(dead_min);
    PDDL_BITSET(dead_min_bs, strips->fact.fact_size);
    pddl_set_iset_t dset_min;
    pddlSetISetInit(&dset_min);
    pddl_disamb_result_t res_min = {
        .implied_facts = &imp_min,
        .implied_facts_bs = &imp_min_bs,
        .dead_facts = &dead_min,
        .dead_facts_bs = &dead_min_bs,
        .disamb = &dset_min,
    };
    PDDL_ISET(bs_imp_min);
    PDDL_BITSET(bs_imp_min_bs, strips->fact.fact_size);
    PDDL_ISET(bs_dead_min);
    PDDL_BITSET(bs_dead_min_bs, strips->fact.fact_size);
    pddl_set_iset_t bs_dset_min;
    pddlSetISetInit(&bs_dset_min);
    pddl_disamb_result_t bs_res_min = {
        .implied_facts = &bs_imp_min,
        .implied_facts_bs = &bs_imp_min_bs,
        .dead_facts = &bs_dead_min,
        .dead_facts_bs = &bs_dead_min_bs,
        .disamb = &bs_dset_min,
    };


    pddlDisamb(dis, &args_ac, &res_ac);
    pddlDisamb(dis, &bs_args_ac, &bs_res_ac);
    assert(res_ac.is_dead == bs_res_ac.is_dead);
    assert(res_ac.disambiguated == bs_res_ac.disambiguated);
    assert(pddlISetEq(&imp_ac, &bs_imp_ac));
    assert(pddlBitsetEq(&imp_ac_bs, &bs_imp_ac_bs));
    assert(pddlISetEq(&dead_ac, &bs_dead_ac));
    assert(pddlBitsetEq(&dead_ac_bs, &bs_dead_ac_bs));
    assert(pddlSetISetSize(&dset_ac) == pddlSetISetSize(&bs_dset_ac));
    PDDL_SET_ISET_FOR_EACH(&dset_ac, s1){
        pddl_bool_t found = pddl_false;
        PDDL_SET_ISET_FOR_EACH(&bs_dset_ac, s2){
            if (pddlISetEq(s1, s2)){
                found = pddl_true;
                break;
            }
        }
        assert(found);
    }


    pddlDisamb(dis, &args_min, &res_min);
    pddlDisamb(dis, &bs_args_min, &bs_res_min);
    assert(res_min.is_dead == bs_res_min.is_dead);
    assert(res_min.disambiguated == bs_res_min.disambiguated);
    assert(pddlISetEq(&imp_min, &bs_imp_min));
    assert(pddlBitsetEq(&imp_min_bs, &bs_imp_min_bs));
    assert(pddlISetEq(&dead_min, &bs_dead_min));
    assert(pddlBitsetEq(&dead_min_bs, &bs_dead_min_bs));
    assert(pddlSetISetSize(&dset_min) == pddlSetISetSize(&bs_dset_min));
    PDDL_SET_ISET_FOR_EACH(&dset_min, s1){
        pddl_bool_t found = pddl_false;
        PDDL_SET_ISET_FOR_EACH(&bs_dset_min, s2){
            if (pddlISetEq(s1, s2)){
                found = pddl_true;
                break;
            }
        }
        assert(found);
    }

    // res_ac.dead implies res_min.dead
    assert(!res_ac.is_dead || res_min.is_dead);
    // res_ac.disambiguated implies res_min.disambiguated or res_min.dead
    assert(!res_ac.disambiguated || res_min.disambiguated || res_min.is_dead);
    if (!res_min.is_dead){
        /*
        pddlFactsPrintSet(&imp_min, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");
        pddlFactsPrintSet(&imp_ac, &strips->fact, " ", "", stdout);
        fprintf(stdout, "\n");
        fflush(stdout);
        */
        assert(pddlISetIsSubset(&imp_ac, &imp_min));
        assert(pddlBitsetIsSubset(&imp_ac_bs, &imp_min_bs));
        assert(pddlISetIsSubset(&dead_ac, &dead_min));
        assert(pddlBitsetIsSubset(&dead_ac_bs, &dead_min_bs));
        PDDL_SET_ISET_FOR_EACH(&dset_min, smin){
            pddl_bool_t found = pddl_false;
            PDDL_SET_ISET_FOR_EACH(&dset_ac, sac){
                if (pddlISetIsSubset(smin, sac)){
                    found = pddl_true;
                    break;
                }
            }
            assert(found);
        }

        PDDL_SET_ISET_FOR_EACH(&dset_ac, sac){
            pddl_bool_t found = pddl_false;
            PDDL_SET_ISET_FOR_EACH(&dset_min, smin){
                if (pddlISetIsSubset(smin, sac)){
                    found = pddl_true;
                    break;
                }
            }
            assert(found);
        }
    }



    pddlISetFree(&imp_min);
    pddlBitsetFree(&imp_min_bs);
    pddlISetFree(&dead_min);
    pddlBitsetFree(&dead_min_bs);
    pddlSetISetFree(&dset_min);
    pddlISetFree(&bs_imp_min);
    pddlBitsetFree(&bs_imp_min_bs);
    pddlISetFree(&bs_dead_min);
    pddlBitsetFree(&bs_dead_min_bs);
    pddlSetISetFree(&bs_dset_min);

    pddlISetFree(&imp_ac);
    pddlBitsetFree(&imp_ac_bs);
    pddlISetFree(&dead_ac);
    pddlBitsetFree(&dead_ac_bs);
    pddlSetISetFree(&dset_ac);
    pddlISetFree(&bs_imp_ac);
    pddlBitsetFree(&bs_imp_ac_bs);
    pddlISetFree(&bs_dead_ac);
    pddlBitsetFree(&bs_dead_ac_bs);
    pddlSetISetFree(&bs_dset_ac);

    pddlBitsetFree(&Fbs);
    pddlISetFree(&sarc);
    pddlISetFree(&smin);
}

TEST_COND(disambiguation_min, disambiguation, CADICAL)
{
    pddl_disamb_t dis;
    pddlDisambInit(&dis, mg_strips.strips.fact.fact_size,
                   &mutex, &mg_strips.mg);

    disambTestMin(&dis, &mg_strips.strips, &mg_strips.strips.goal, "Goal:");

    for (int op_id = 0; op_id < mg_strips.strips.op.op_size && op_id < 300; ++op_id){
        const pddl_strips_op_t *op = mg_strips.strips.op.op[op_id];
        char header[1024];
        snprintf(header, 1024, "(%s)", op->name);
        header[1023] = 0;
        disambTestMin(&dis, &mg_strips.strips, &op->pre, header);
    }


    pddlDisambFree(&dis);
}

TEST_COND(disambiguation_min_full, disambiguation, CADICAL)
{

    pddl_disamb_t dis;
    pddlDisambInit(&dis, mg_strips.strips.fact.fact_size,
                   &mutex, &mg_strips.mg);

    disambTestMinFull(&dis, &mg_strips.strips, &mg_strips.strips.goal, "Goal:");

    for (int op_id = 0; op_id < mg_strips.strips.op.op_size && op_id < 300; op_id += 3){
        const pddl_strips_op_t *op = mg_strips.strips.op.op[op_id];
        char header[1024];
        snprintf(header, 1024, "(%s)", op->name);
        header[1023] = 0;
        disambTestMinFull(&dis, &mg_strips.strips, &op->pre, header);
    }


    pddlDisambFree(&dis);
}
