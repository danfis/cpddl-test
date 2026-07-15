#include "test.h"
#include "context.h"
#include <assert.h>

struct names {
    char **name;
    int size;
};
typedef struct names names_t;

static int cmpNames(const void *a, const void *b)
{
    const char *n1 = *(const char **)a;
    const char *n2 = *(const char **)b;
    return strcmp(n1, n2);
}

static void namesInitFacts(names_t *n, const pddl_strips_t *strips)
{
    n->size = strips->fact.fact_size;
    n->name = PDDL_ALLOC_ARR(char *, n->size);
    for (int i = 0; i < n->size; ++i)
        n->name[i] = PDDL_STRDUP(strips->fact.fact[i]->name);
    qsort(n->name, n->size, sizeof(char *), cmpNames);
}

static void namesInitOps(names_t *n, const pddl_strips_t *strips)
{
    n->size = strips->op.op_size;
    n->name = PDDL_ALLOC_ARR(char *, n->size);
    for (int i = 0; i < n->size; ++i)
        n->name[i] = PDDL_STRDUP(strips->op.op[i]->name);
    qsort(n->name, n->size, sizeof(char *), cmpNames);
}

static void namesFree(names_t *n)
{
    for (int i = 0; i < n->size; ++i)
        PDDL_FREE(n->name[i]);
    if (n->name != NULL)
        PDDL_FREE(n->name);
}

/** Prints names from BEFORE that are missing in AFTER; both are sorted, and
 *  duplicated names are matched one-to-one. */
static void printRemoved(const char *label,
                         const names_t *before,
                         const names_t *after)
{
    if (before->size == after->size)
        return;
    printf("%s [%d]:\n", label, before->size - after->size);
    int ai = 0;
    for (int bi = 0; bi < before->size; ++bi){
        if (ai < after->size
                && strcmp(before->name[bi], after->name[ai]) == 0){
            ++ai;
        }else{
            printf("  (%s)\n", before->name[bi]);
        }
    }
}

/** Prints the summary of the current C.strips along with the sorted lists
 *  of facts and operators removed since FACTS_BEFORE/OPS_BEFORE snapshots
 *  were taken. Frees both snapshots. */
static void printSummary(const char *prefix,
                         names_t *facts_before,
                         names_t *ops_before)
{
    printf("%s facts: %d, ops: %d, goal-unreachable: %d\n",
           prefix, C.strips.fact.fact_size, C.strips.op.op_size,
           (int)C.strips.goal_is_unreachable);

    names_t facts_after, ops_after;
    namesInitFacts(&facts_after, &C.strips);
    namesInitOps(&ops_after, &C.strips);
    printRemoved("removed facts", facts_before, &facts_after);
    printRemoved("removed ops", ops_before, &ops_after);
    namesFree(&facts_after);
    namesFree(&ops_after);
    namesFree(facts_before);
    namesFree(ops_before);
    fflush(stdout);
}

static void assertSameStrips(const pddl_strips_t *a, const pddl_strips_t *b)
{
    assert(a->fact.fact_size == b->fact.fact_size);
    assert(a->op.op_size == b->op.op_size);
    assert(a->goal_is_unreachable == b->goal_is_unreachable);

    for (int i = 0; i < a->fact.fact_size; ++i)
        assert(strcmp(a->fact.fact[i]->name, b->fact.fact[i]->name) == 0);

    assert(pddlISetEq(&a->init, &b->init));
    assert(pddlISetEq(&a->goal, &b->goal));

    for (int i = 0; i < a->op.op_size; ++i){
        const pddl_strips_op_t *op1 = a->op.op[i];
        const pddl_strips_op_t *op2 = b->op.op[i];
        assert(strcmp(op1->name, op2->name) == 0);
        assert(op1->cost == op2->cost);
        assert(pddlISetEq(&op1->pre, &op2->pre));
        assert(pddlISetEq(&op1->add_eff, &op2->add_eff));
        assert(pddlISetEq(&op1->del_eff, &op2->del_eff));

        assert(op1->cond_eff_size == op2->cond_eff_size);
        for (int ci = 0; ci < op1->cond_eff_size; ++ci){
            const pddl_strips_op_cond_eff_t *ce1 = op1->cond_eff + ci;
            const pddl_strips_op_cond_eff_t *ce2 = op2->cond_eff + ci;
            assert(pddlISetEq(&ce1->pre, &ce2->pre));
            assert(pddlISetEq(&ce1->add_eff, &ce2->add_eff));
            assert(pddlISetEq(&ce1->del_eff, &ce2->del_eff));
        }
    }
}

TEST(prune_strips, lmg)
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

    pddlMutexPairsInitStrips(&C.mutex, &C.strips);
    C.mutex_set = 1;

    if (C.strips.op.op_size == 0){
        pddlStripsMakeUnsolvable(&C.strips);
        pddlMutexPairsFree(&C.mutex);
        pddlMutexPairsInitStrips(&C.mutex, &C.strips);
        pddlMGroupsFree(&C.mg);
        pddlMGroupsInitEmpty(&C.mg);
    }

    printf("facts: %d, ops: %d, mgroups: %d\n",
           C.strips.fact.fact_size, C.strips.op.op_size, C.mg.mgroup_size);
    fflush(stdout);
}

TEST(prune_strips_irr, prune_strips)
{
    names_t facts_before, ops_before;
    namesInitFacts(&facts_before, &C.strips);
    namesInitOps(&ops_before, &C.strips);

    PDDL_ISET(irr_fact);
    PDDL_ISET(irr_op);
    int ret = pddlIrrelevanceAnalysis(&C.strips, &irr_fact, &irr_op,
                                      NULL, &C.err);
    assert(ret == 0);
    int want_facts = C.strips.fact.fact_size - pddlISetSize(&irr_fact);
    int want_ops = C.strips.op.op_size - pddlISetSize(&irr_op);
    pddlISetFree(&irr_fact);
    pddlISetFree(&irr_op);

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsAddIrr(&prune);
    ret = pddlPruneStripsExecute(&prune, &C.strips, NULL, NULL, &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);

    assert(C.strips.fact.fact_size == want_facts);
    assert(C.strips.op.op_size == want_ops);
    printSummary("irr:", &facts_before, &ops_before);
}

TEST(prune_strips_h2fw, prune_strips)
{
    names_t facts_before, ops_before;
    namesInitFacts(&facts_before, &C.strips);
    namesInitOps(&ops_before, &C.strips);

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsAddIrr(&prune);
    pddlPruneStripsAddH2Fw(&prune, -1);
    int ret = pddlPruneStripsExecute(&prune, &C.strips, NULL, NULL, &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);
    printSummary("h2fw:", &facts_before, &ops_before);
}

TEST(prune_strips_h2fwbw, prune_strips)
{
    names_t facts_before, ops_before;
    namesInitFacts(&facts_before, &C.strips);
    namesInitOps(&ops_before, &C.strips);

    pddlMutexPairsAddMGroups(&C.mutex, &C.mg);

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsAddUnreachOps(&prune);
    pddlPruneStripsAddIrrPreHm(&prune);
    pddlPruneStripsAddFAMGroupDeadEnd(&prune);
    pddlPruneStripsAddH2FwBw(&prune, PDDL_HM_MUTEX_TASK_MG_STRIPS,
                             pddl_true, -1);
    pddlPruneStripsAddIrr(&prune);
    pddlPruneStripsAddRmUselessDelEffs(&prune);
    pddlPruneStripsAddDedup(&prune);
    int ret = pddlPruneStripsExecute(&prune, &C.strips, &C.mg, &C.mutex,
                                     &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);

    assert(C.mutex.fact_size == C.strips.fact.fact_size);
    for (int mi = 0; mi < C.mg.mgroup_size; ++mi){
        PDDL_ISET_FOR_EACH(&C.mg.mgroup[mi].mgroup, fact_id)
            assert(fact_id >= 0 && fact_id < C.strips.fact.fact_size);
    }

    printSummary("h2fwbw:", &facts_before, &ops_before);
    printf("num mutex pairs: %lu\n",
           (unsigned long)C.mutex.num_mutex_pairs);
    fflush(stdout);
}

TEST(prune_strips_dtg, prune_strips)
{
    names_t facts_before, ops_before;
    namesInitFacts(&facts_before, &C.strips);
    namesInitOps(&ops_before, &C.strips);

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsAddUnreachableInDTGs(&prune);
    int ret = pddlPruneStripsExecute(&prune, &C.strips, &C.mg, NULL, &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);
    printSummary("dtg:", &facts_before, &ops_before);
}

TEST(prune_strips_misc, prune_strips)
{
    names_t facts_before, ops_before;
    namesInitFacts(&facts_before, &C.strips);
    namesInitOps(&ops_before, &C.strips);

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsAddRmOpsEmptyAddEff(&prune);
    pddlPruneStripsAddDedup(&prune);
    pddlPruneStripsAddSortOpsByName(&prune);
    int ret = pddlPruneStripsExecute(&prune, &C.strips, NULL, NULL, &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);

    for (int i = 1; i < C.strips.op.op_size; ++i){
        assert(strcmp(C.strips.op.op[i - 1]->name,
                      C.strips.op.op[i]->name) <= 0);
    }
    printSummary("misc:", &facts_before, &ops_before);
}

TEST(prune_strips_h3fw, prune_strips)
{
    names_t facts_before, ops_before;
    namesInitFacts(&facts_before, &C.strips);
    namesInitOps(&ops_before, &C.strips);

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsAddIrr(&prune);
    pddlPruneStripsAddH3Fw(&prune, -1, 0);
    int ret = pddlPruneStripsExecute(&prune, &C.strips, NULL, NULL, &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);
    printSummary("h3fw:", &facts_before, &ops_before);
}

TEST(prune_strips_apply_each_h2fw, prune_strips)
{
    pddl_strips_t strips2;
    pddlStripsInitCopy(&strips2, &C.strips);

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsSetAlwaysApply(&prune, pddl_false);
    pddlPruneStripsAddIrr(&prune);
    pddlPruneStripsAddH2Fw(&prune, -1);
    int ret = pddlPruneStripsExecute(&prune, &C.strips, NULL, NULL, &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);

    pddlPruneStripsInit(&prune);
    pddlPruneStripsSetAlwaysApply(&prune, pddl_true);
    pddlPruneStripsAddIrr(&prune);
    pddlPruneStripsAddH2Fw(&prune, -1);
    ret = pddlPruneStripsExecute(&prune, &strips2, NULL, NULL, &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);

    assertSameStrips(&C.strips, &strips2);
    pddlStripsFree(&strips2);
}

static void addH2FwBwPipeline(pddl_prune_strips_t *prune)
{
    pddlPruneStripsAddUnreachOps(prune);
    pddlPruneStripsAddIrrPreHm(prune);
    pddlPruneStripsAddFAMGroupDeadEnd(prune);
    pddlPruneStripsAddH2FwBw(prune, PDDL_HM_MUTEX_TASK_MG_STRIPS,
                             pddl_true, -1);
    pddlPruneStripsAddIrr(prune);
    pddlPruneStripsAddRmUselessDelEffs(prune);
    pddlPruneStripsAddDedup(prune);
}

TEST(prune_strips_apply_each_h2fwbw, prune_strips)
{
    pddl_strips_t strips2;
    pddlStripsInitCopy(&strips2, &C.strips);
    pddl_mgroups_t mg2;
    pddlMGroupsInitCopy(&mg2, &C.mg);
    pddl_mutex_pairs_t mutex2;
    pddlMutexPairsInitStrips(&mutex2, &strips2);

    pddlMutexPairsAddMGroups(&C.mutex, &C.mg);
    pddlMutexPairsAddMGroups(&mutex2, &mg2);

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsSetAlwaysApply(&prune, pddl_false);
    addH2FwBwPipeline(&prune);
    int ret = pddlPruneStripsExecute(&prune, &C.strips, &C.mg, &C.mutex,
                                     &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);

    pddlPruneStripsInit(&prune);
    pddlPruneStripsSetAlwaysApply(&prune, pddl_true);
    addH2FwBwPipeline(&prune);
    ret = pddlPruneStripsExecute(&prune, &strips2, &mg2, &mutex2, &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);

    assertSameStrips(&C.strips, &strips2);
    assert(C.mutex.fact_size == mutex2.fact_size);
    assert(C.mutex.num_mutex_pairs == mutex2.num_mutex_pairs);

    pddlMutexPairsFree(&mutex2);
    pddlMGroupsFree(&mg2);
    pddlStripsFree(&strips2);
}

TEST_COND(prune_strips_op_mutex, prune_strips, BLISS)
{
    names_t facts_before, ops_before;
    namesInitFacts(&facts_before, &C.strips);
    namesInitOps(&ops_before, &C.strips);

    pddl_prune_strips_op_mutex_config_t cfg
        = PDDL_PRUNE_STRIPS_OP_MUTEX_CONFIG_INIT;
    cfg.method = PDDL_PRUNE_STRIPS_OP_MUTEX_TS;
    cfg.ts_merge_size = 1;

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsAddOpMutex(&prune, &cfg);
    int ret = pddlPruneStripsExecute(&prune, &C.strips, &C.mg, NULL, &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);
    printSummary("op-mutex:", &facts_before, &ops_before);
}

TEST_COND(prune_strips_endo, prune_strips, CPOPTIMIZER)
{
    int op_size_before = C.strips.op.op_size;

    pddl_endomorphism_config_t cfg = PDDL_ENDOMORPHISM_CONFIG_INIT;
    cfg.run_in_subprocess = 1;

    pddl_prune_strips_t prune;
    pddlPruneStripsInit(&prune);
    pddlPruneStripsAddEndomorphism(&prune, PDDL_PRUNE_STRIPS_ENDO_FDR, &cfg);
    int ret = pddlPruneStripsExecute(&prune, &C.strips, &C.mg, &C.mutex,
                                     &C.err);
    assert(ret == 0);
    pddlPruneStripsFree(&prune);

    assert(C.strips.op.op_size <= op_size_before);
}
